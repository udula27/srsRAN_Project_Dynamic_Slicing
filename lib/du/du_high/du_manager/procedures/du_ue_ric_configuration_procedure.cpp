/*
 *
 * Copyright 2021-2026 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "du_ue_ric_configuration_procedure.h"
#include "srsran/mac/mac_cell_manager.h"
#include "srsran/ran/band_helper.h"
#include "srsran/scheduler/config/scheduler_expert_config.h"
#include "srsran/scheduler/config/slice_rrm_policy_config.h"
#include "srsran/srslog/srslog.h"
#include "srsran/support/async/execute_on_blocking.h"

using namespace srsran;
using namespace srs_du;

du_ue_ric_configuration_procedure::du_ue_ric_configuration_procedure(const du_mac_sched_control_config& request_,
                                                                     du_ue_manager_repository&          ue_mng_,
                                                                     const du_manager_params&           du_params_) :
  request(request_), ue_mng(ue_mng_), du_params(du_params_)
{
}

void du_ue_ric_configuration_procedure::operator()(coro_context<async_task<du_mac_sched_control_config_response>>& ctx)
{
  CORO_BEGIN(ctx);

  // Switch to DU manager execution context.
  CORO_AWAIT(execute_on_blocking(du_params.services.du_mng_exec, du_params.services.timers));

  if (request.rrm_policy_ratio_list.empty()) {
    CORO_EARLY_RETURN((du_mac_sched_control_config_response{false, false, false}));
  }

  ue = ue_mng.find_f1ap_ue_id(request.ue_id);

  if (ue == nullptr) {
    // Cell-level path: queue slice reconfiguration via MAC cell controller so
    // scheduler applies it at TTI boundary through its event queue.
    srslog::fetch_basic_logger("DU-MNG").info(
        "[STEP2-CELL-LEVEL] Slice PRB quota update for cell (no UE), queuing via MAC cell event path");

    if (!build_cell_level_reconfig()) {
      CORO_EARLY_RETURN((du_mac_sched_control_config_response{false, false, false}));
    }

    CORO_AWAIT_VALUE(cell_reconf_resp,
                     du_params.mac.mgr.get_cell_manager()
                         .get_cell_controller(to_du_cell_index(0))
                         .reconfigure(cell_reconf_req));

    CORO_EARLY_RETURN((du_mac_sched_control_config_response{true, true, true}));
  }

  CORO_AWAIT(dispatch_ue_config_task());
  CORO_RETURN(ue_config_completed.get());
}

bool du_ue_ric_configuration_procedure::build_cell_level_reconfig()
{
  const auto& rrm_policy   = request.rrm_policy_ratio_list[0];
  unsigned    min_prb_ratio = std::clamp(rrm_policy.minimum_ratio.value_or(0), 0u, 100u);
  unsigned    max_prb_ratio = std::clamp(rrm_policy.maximum_ratio.value_or(100), 0u, 100u);
  unsigned    ded_prb_ratio = std::clamp(rrm_policy.dedicated_ratio.value_or(0), 0u, 100u);

  const unsigned nof_prbs = get_max_Nprb(du_params.ran.cells[0].dl_carrier.carrier_bw_mhz,
                                          du_params.ran.cells[0].scs_common,
                                          band_helper::get_freq_range(du_params.ran.cells[0].dl_carrier.band));

  const unsigned min_prb = static_cast<unsigned>((1.0 * min_prb_ratio / 100) * nof_prbs);
  const unsigned max_prb = static_cast<unsigned>((1.0 * max_prb_ratio / 100) * nof_prbs);
    const unsigned ded_prb = static_cast<unsigned>((1.0 * ded_prb_ratio / 100) * nof_prbs);

  if (min_prb > max_prb) {
    srslog::fetch_basic_logger("DU-MNG").warning(
        "[STEP2-CELL-LEVEL] Rejecting slice reconfig: min_prb={} > max_prb={}", min_prb, max_prb);
    return false;
  }

  du_cell_slice_reconfig_request slice_req;
  slice_req.cell_index = to_du_cell_index(0);

  for (const auto& policy_member : rrm_policy.policy_members_list) {
    if (slice_req.rrm_policies.full()) {
      srslog::fetch_basic_logger("DU-MNG").warning("[STEP2-CELL-LEVEL] RRM policy list full, truncating");
      break;
    }
    srslog::fetch_basic_logger("DU-MNG").info(
        "[STEP2-VALIDATION] Queuing cell-level slice reconfig via event queue: "
        "SST={} min={}%% ({} PRBs) max={}%% ({} PRBs)",
        policy_member.s_nssai.sst.value(),
        min_prb_ratio, min_prb,
        max_prb_ratio, max_prb);

      slice_req.rrm_policies.push_back(
          du_cell_slice_reconfig_request::rrm_policy_config{policy_member, {ded_prb, min_prb, max_prb}});

    // Mirror into Phase-1 policy store for validation/KPI logging.
    auto* slice_mgr = du_params.mac.mgr.get_cell_slice_resource_manager(to_du_cell_index(0));
    if (slice_mgr != nullptr) {
      slice_mgr->update_slice_policy(policy_member, min_prb_ratio, max_prb_ratio, ded_prb_ratio);
    }
  }

  if (slice_req.rrm_policies.empty()) {
    return false;
  }

  cell_reconf_req = mac_cell_reconfig_request{};
  cell_reconf_req.slice_reconf_req.emplace(slice_req);
  return true;
}

manual_event<du_mac_sched_control_config_response>& du_ue_ric_configuration_procedure::dispatch_ue_config_task()
{
  ue_mng.schedule_async_task(ue->ue_index, launch_async([this](coro_context<async_task<void>>& ctx) {
                               CORO_BEGIN(ctx);
                               CORO_AWAIT_VALUE(const mac_ue_reconfiguration_response result, handle_mac_config());
                               if (result.result) {
                                 du_params.mac.mgr.get_ue_configurator().handle_ue_config_applied(result.ue_index);
                               }
                               ue_config_completed.set(
                                   du_mac_sched_control_config_response{result.result, result.result, result.result});
                               CORO_RETURN();
                             }));
  return ue_config_completed;
}

async_task<mac_ue_reconfiguration_response> du_ue_ric_configuration_procedure::handle_mac_config()
{
  mac_ue_reconfiguration_request mac_request;

  mac_request.ue_index    = ue->ue_index;
  mac_request.crnti       = ue->rnti;
  mac_request.pcell_index = ue->pcell_index;

  // Configure UE resource allocation parameters.
  mac_request.sched_cfg.res_alloc_cfg.emplace();
  auto& res_alloc_cfg = mac_request.sched_cfg.res_alloc_cfg.value();
  // For now take first parameter set, in future we will have to support multiple parameter sets for different slices.
  res_alloc_cfg.rrm_policy_group = request.rrm_policy_ratio_list[0];

  // TODO remove when RRM group support is added to scheduler.
  unsigned nof_prbs = get_max_Nprb(du_params.ran.cells[0].dl_carrier.carrier_bw_mhz,
                                   du_params.ran.cells[0].scs_common,
                                   band_helper::get_freq_range(du_params.ran.cells[0].dl_carrier.band));

  unsigned min_prb_ratio = res_alloc_cfg.rrm_policy_group.minimum_ratio.value_or(0);
  unsigned max_prb_ratio = res_alloc_cfg.rrm_policy_group.maximum_ratio.value_or(100);

  min_prb_ratio = std::clamp(min_prb_ratio, static_cast<unsigned>(0), static_cast<unsigned>(100));
  max_prb_ratio = std::clamp(max_prb_ratio, static_cast<unsigned>(0), static_cast<unsigned>(100));

  unsigned min_prb_limit = static_cast<int>((1.0 * min_prb_ratio / 100) * nof_prbs);
  unsigned max_prb_limit = static_cast<int>((1.0 * max_prb_ratio / 100) * nof_prbs);

  res_alloc_cfg.pdsch_grant_size_limits = {min_prb_limit, max_prb_limit};
  res_alloc_cfg.pusch_grant_size_limits = {min_prb_limit, max_prb_limit};

  res_alloc_cfg.max_pdsch_harq_retxs = request.num_harq_retransmissions.has_value()
                                           ? request.num_harq_retransmissions.value()
                                           : du_params.mac.sched_cfg.ue.max_nof_dl_harq_retxs;
  res_alloc_cfg.max_pusch_harq_retxs = res_alloc_cfg.max_pdsch_harq_retxs;

  // [STEP2-E2-BRIDGE] Update cell-level slice resource manager with E2 control policies
  // This bridges E2SM-RC Control Action 2.6 to Phase 1 slice infrastructure
  auto* slice_mgr = du_params.mac.mgr.get_cell_slice_resource_manager(ue->pcell_index);
  if (slice_mgr != nullptr && !request.rrm_policy_ratio_list.empty()) {
    const auto& rrm_policy = request.rrm_policy_ratio_list[0];
    unsigned ded_ratio = rrm_policy.dedicated_ratio.value_or(0);

    // Update slice policy for each member in the RRM policy
    for (const auto& policy_member : rrm_policy.policy_members_list) {
      srslog::fetch_basic_logger("DU-MNG").info(
          "[STEP2-VALIDATION] Updating slice-level PRB policy: SST={} min={}% max={}% ded={}%",
          policy_member.s_nssai.sst.value(),
          min_prb_ratio,
          max_prb_ratio,
          ded_ratio);

      slice_mgr->update_slice_policy(policy_member, min_prb_ratio, max_prb_ratio, ded_ratio);
    }
  }

  return du_params.mac.mgr.get_ue_configurator().handle_ue_reconfiguration_request(mac_request);
}
