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

#include "ue_scheduler_impl.h"
#include "../logging/scheduler_metrics_handler.h"

using namespace srsran;

namespace {
// Two S-NSSAIs are compatible if their SSTs match AND at least one side has no/wildcard SD.
// Mirrors the same helper in inter_slice_scheduler.cpp and intra_slice_scheduler.cpp.
static bool snssai_compatible(const s_nssai_t& lhs, const s_nssai_t& rhs)
{
  if (lhs.sst != rhs.sst) {
    return false;
  }
  if (lhs.sd == rhs.sd) {
    return true;
  }
  const auto is_no_sd = [](const s_nssai_t& s) {
    return !s.sd.is_set() || s.sd.value() == 0 || s.sd.value() == 0xFFFFFF;
  };
  return is_no_sd(lhs) || is_no_sd(rhs);
}
} // namespace

ue_scheduler_impl::ue_scheduler_impl(const scheduler_ue_expert_config& expert_cfg_) :
  expert_cfg(expert_cfg_), logger(srslog::fetch_basic_logger("SCHED")), event_mng(ue_db)
{
}

ue_cell_scheduler* ue_scheduler_impl::do_add_cell(const ue_cell_scheduler_creation_request& params)
{
  cells.emplace(params.cell_index, *this, params);
  auto& cell = cells[params.cell_index];

  // Create a cell-specific UE event manager.
  cell.ev_mng = event_mng.add_cell(cell_creation_event{*params.cell_res_alloc,
                                                       cell.cell_harqs,
                                                       cell.ue_cell_db,
                                                       cell.fallback_sched,
                                                       cell.uci_sched,
                                                       cell.slice_sched,
                                                       cell.srs_sched,
                                                       *params.cell_metrics,
                                                       *params.ev_logger});

  return &cell;
}

void ue_scheduler_impl::do_start_cell(du_cell_index_t cell_index)
{
  srsran_assert(cells.contains(cell_index), "Cell reference not found in the scheduler");

  // Signal event manager that new events can be processed for this cell.
  cells[cell_index].ev_mng->start();
}

void ue_scheduler_impl::do_stop_cell(du_cell_index_t cell_index)
{
  srsran_assert(cells.contains(cell_index), "Cell reference not found in the scheduler");
  auto& c = cells[cell_index];

  // Halt any pending events associated with this cell.
  cells[cell_index].ev_mng->stop();

  // Stop sub-schedulers.
  c.fallback_sched.stop();
  c.srs_sched.stop();
  c.uci_sched.stop();
  c.cell_harqs.stop();

  // Remove UEs from the UE repository associated with this cell.
  ue_db.handle_cell_deactivation(cell_index);
}

void ue_scheduler_impl::do_rem_cell(du_cell_index_t cell_index)
{
  srsran_assert(cells.contains(cell_index), "Cell reference not found in the scheduler");

  do_stop_cell(cell_index);

  // Remove cell from UE scheduler.
  cells.erase(cell_index);
}

void ue_scheduler_impl::run_sched_strategy(du_cell_index_t                        cell_index,
                                           std::map<s_nssai_t, slice_prb_budget>& dl_slice_budgets,
                                           std::map<s_nssai_t, slice_prb_budget>& ul_slice_budgets)
{
  auto& cell = cells[cell_index];

  auto to_remaining = [](const std::map<s_nssai_t, slice_prb_budget>& budgets) {
    std::map<s_nssai_t, unsigned> remaining;
    for (const auto& [snssai, budget] : budgets) {
      remaining[snssai] = budget.budget_prbs > budget.allocated_prbs ? (budget.budget_prbs - budget.allocated_prbs) : 0;
    }
    return remaining;
  };

  // Schedule DL first.
  // Note: DL should be scheduled first so that the right DAI value is picked in DCI format 0_1.
  while (true) {
    cell.slice_sched.set_live_remaining_rbs(to_remaining(dl_slice_budgets));
    auto dl_slice_candidate = cell.slice_sched.get_next_dl_candidate();
    if (!dl_slice_candidate.has_value()) {
      break;
    }
    scheduler_policy& policy = cell.slice_sched.get_policy(dl_slice_candidate->id());
    cell.intra_slice_sched.dl_sched(dl_slice_candidate.value(), policy, dl_slice_budgets);
  }

  while (true) {
    cell.slice_sched.set_live_remaining_rbs(to_remaining(ul_slice_budgets));
    auto ul_slice_candidate = cell.slice_sched.get_next_ul_candidate();
    if (!ul_slice_candidate.has_value()) {
      break;
    }
    scheduler_policy& policy = cell.slice_sched.get_policy(ul_slice_candidate->id());
    cell.intra_slice_sched.ul_sched(ul_slice_candidate.value(), policy, ul_slice_budgets);
  }
}

void ue_scheduler_impl::update_harq_pucch_counter(cell_resource_allocator& cell_alloc)
{
  // We need to update the PUCCH counter after the SR/CSI scheduler because the allocation of CSI/SR can add/remove
  // PUCCH grants.
  static constexpr unsigned HARQ_SLOT_DELAY = 0;
  const auto&               slot_alloc      = cell_alloc[HARQ_SLOT_DELAY];

  // Spans through the PUCCH grant list and update the HARQ-ACK PUCCH grant counter for the corresponding RNTI and HARQ
  // process id.
  for (const auto& pucch : slot_alloc.result.ul.pucchs) {
    const unsigned nof_harqs_per_rnti_per_slot = pucch.uci_bits.harq_ack_nof_bits;
    if (nof_harqs_per_rnti_per_slot == 0) {
      continue;
    }

    ue* user = ue_db.find_by_rnti(pucch.crnti);
    // This is to handle the case of a UE that gets removed after the PUCCH gets allocated and before this PUCCH is
    // expected to be sent.
    if (user == nullptr) {
      logger.warning("rnti={}: No user with such RNTI found in the ue scheduler database. Skipping PUCCH grant counter",
                     pucch.crnti,
                     slot_alloc.slot);
      continue;
    }
    // Each PUCCH grants can potentially carry ACKs for different HARQ processes (as many as the harq_ack_nof_bits)
    // expecting to be acknowledged on the same slot.
    for (unsigned harq_bit_idx = 0; harq_bit_idx != nof_harqs_per_rnti_per_slot; ++harq_bit_idx) {
      std::optional<dl_harq_process_handle> h_dl =
          user->get_pcell().harqs.find_dl_harq_waiting_ack(slot_alloc.slot, harq_bit_idx);
      if (not h_dl.has_value() or not h_dl->is_waiting_ack()) {
        logger.warning(
            "ue={} rnti={}: No DL HARQ process with state waiting-for-ack found at slot={} for harq-bit-index={}",
            fmt::underlying(user->ue_index),
            user->crnti,
            slot_alloc.slot,
            harq_bit_idx);
        continue;
      };
      h_dl->increment_pucch_counter();
    }
  }
}

[[maybe_unused]] static bool puxch_grant_sanitizer(cell_resource_allocator& cell_alloc, srslog::basic_logger& logger)
{
  const unsigned HARQ_SLOT_DELAY = 0;
  const auto&    slot_alloc      = cell_alloc[HARQ_SLOT_DELAY];

  if (not cell_alloc.cfg.is_ul_enabled(slot_alloc.slot)) {
    return true;
  }

  // Spans through the PUCCH grant list and check if there is any PUCCH grant scheduled for a UE that has a PUSCH.
  for (const auto& pucch : slot_alloc.result.ul.pucchs) {
    const auto* pusch_grant =
        std::find_if(slot_alloc.result.ul.puschs.begin(),
                     slot_alloc.result.ul.puschs.end(),
                     [&pucch](const ul_sched_info& pusch) { return pusch.pusch_cfg.rnti == pucch.crnti; });

    if (pusch_grant != slot_alloc.result.ul.puschs.end()) {
      logger.error("rnti={}: has both PUCCH and PUSCH grants scheduled at slot {}, PUCCH  format={} with uci_bits={}",
                   pucch.crnti,
                   slot_alloc.slot,
                   static_cast<unsigned>(pucch.format()),
                   pucch.uci_bits);
      return false;
    }
  }
  return true;
}

void ue_scheduler_impl::run_slot_impl(slot_point slot_tx)
{
  std::unique_lock<std::mutex> lock(cell_group_mutex, std::defer_lock);
  if (cells.size() > 1) {
    // Only mutex if the cell group has more than one cell (Carrier Aggregation case).
    lock.lock();
  }
  if (last_sl_ind == slot_tx) {
    // This slot has already been processed by a cell of the same cell group.
    return;
  }
  last_sl_ind = slot_tx;

  for (auto& group_cell : cells) {
    du_cell_index_t cell_index = group_cell.cell_res_alloc->cfg.cell_index;

    // Process any pending events that are directed at UEs.
    group_cell.ev_mng->run_slot(slot_tx);

    // Update all UEs state.
    ue_db.slot_indication(slot_tx);

    // Check for timeouts in the cell HARQ processes.
    group_cell.cell_harqs.slot_indication(slot_tx);

    // Schedule periodic UCI (SR and CSI) before any UL grants.
    group_cell.uci_sched.run_slot(*group_cell.cell_res_alloc);

    // Schedule periodic SRS before any UE grants.
    group_cell.srs_sched.run_slot(*group_cell.cell_res_alloc);

    // Run cell-specific SRB0 scheduler.
    group_cell.fallback_sched.run_slot(*group_cell.cell_res_alloc);

    // Compute live slice budgets for this slot after event processing/reconfiguration.
    auto dl_slice_budgets = compute_slice_budgets_for_slot(slot_tx, *group_cell.cell_res_alloc, *group_cell.slice_mgr, group_cell.slice_sched);
    auto ul_slice_budgets = dl_slice_budgets;

    // Update slice context and compute slice priorities.
    std::map<s_nssai_t, unsigned> initial_live_remaining;
    for (const auto& [snssai, budget] : dl_slice_budgets) {
      initial_live_remaining[snssai] = budget.budget_prbs > budget.allocated_prbs ? (budget.budget_prbs - budget.allocated_prbs)
                                                                                   : 0;
    }
    group_cell.slice_sched.slot_indication(slot_tx, *group_cell.cell_res_alloc, initial_live_remaining);

    // Update intra-slice scheduler context.
    group_cell.intra_slice_sched.slot_indication(slot_tx);

    // Run slice scheduler policies.
    run_sched_strategy(cell_index, dl_slice_budgets, ul_slice_budgets);

    // The post processing is done for DL and UL slots.
    group_cell.intra_slice_sched.post_process_results();

    // Update the PUCCH counter after the UE DL and UL scheduler.
    update_harq_pucch_counter(*group_cell.cell_res_alloc);

    // [PHASE2-ENFORCEMENT] Check if minimum guarantees are met.
    // NOTE: allocated_prbs is already updated by enforcement code in intra_slice_scheduler during Stage 2.
    if (group_cell.slice_mgr->get_num_slices() > 0 && !dl_slice_budgets.empty()) {
      for (auto& [snssai, budget] : dl_slice_budgets) {
        auto limits_opt = group_cell.slice_mgr->get_slice_limits(snssai);
        if (limits_opt.has_value() && budget.allocated_prbs >= limits_opt->min()) {
          budget.guaranteed_met = true;
        }
      }
    }

    srsran_sanity_check(puxch_grant_sanitizer(*group_cell.cell_res_alloc, logger),
                        "PUCCH and PUSCH found for the same UE in the same slot");
  }
}

namespace {

class harq_manager_timeout_notifier : public harq_timeout_notifier
{
public:
  explicit harq_manager_timeout_notifier(cell_metrics_handler& metrics_handler_) : metrics_handler(metrics_handler_) {}

  void on_harq_timeout(du_ue_index_t ue_idx, bool is_dl, bool ack) override
  {
    metrics_handler.handle_harq_timeout(ue_idx, is_dl);
  }

private:
  cell_metrics_handler& metrics_handler;
};

} // namespace

ue_scheduler_impl::cell_context::cell_context(ue_scheduler_impl&                        parent_,
                                              const ue_cell_scheduler_creation_request& params) :
  parent(parent_),
  cell_res_alloc(params.cell_res_alloc),
  cell_harqs(
      MAX_NOF_DU_UES,
      MAX_NOF_HARQS,
      std::make_unique<harq_manager_timeout_notifier>(*params.cell_metrics),
      std::make_unique<harq_manager_timeout_notifier>(*params.cell_metrics),
      parent.expert_cfg.dl_harq_retx_timeout.count() * get_nof_slots_per_subframe(cell_res_alloc->cfg.scs_common),
      parent.expert_cfg.ul_harq_retx_timeout.count() * get_nof_slots_per_subframe(cell_res_alloc->cfg.scs_common),
      cell_harq_manager::DEFAULT_ACK_TIMEOUT_SLOTS,
      params.cell_res_alloc->cfg.ntn_cs_koffset,
      params.cell_res_alloc->cfg.dl_harq_mode_b,
      params.cell_res_alloc->cfg.ul_harq_mode_b),
  ue_cell_db(parent.ue_db.add_cell(params.cell_index)),
  uci_sched(params.cell_res_alloc->cfg, *params.uci_alloc, parent.ue_db),
  fallback_sched(parent.expert_cfg,
                 params.cell_res_alloc->cfg,
                 *params.pdcch_sched,
                 *params.pucch_alloc,
                 *params.uci_alloc,
                 parent.ue_db),
  slice_sched(params.cell_res_alloc->cfg, parent.ue_db),
  intra_slice_sched(parent.expert_cfg,
                    parent.ue_db,
                    *params.pdcch_sched,
                    *params.uci_alloc,
                    *params.cell_res_alloc,
                    *params.cell_metrics,
                    cell_harqs,
                    srslog::fetch_basic_logger("SCHED")),
  srs_sched(params.cell_res_alloc->cfg, parent.ue_db),
  slice_mgr(std::make_unique<cell_slice_resource_manager>(params.cell_res_alloc->cfg.nof_dl_prbs, "SCHED"))
{
}

ue_scheduler_impl::cell_context::~cell_context()
{
  parent.ue_db.rem_cell(ue_cell_db.cell_index());
}

// ============================================================================
// Slice-Level PRB Budget Enforcement Implementation
// ============================================================================

std::map<s_nssai_t, slice_prb_budget>
ue_scheduler_impl::compute_slice_budgets_for_slot(slot_point                    sl_tx,
                                                  const cell_resource_allocator& res_alloc,
                                                  cell_slice_resource_manager&  slice_mgr,
                                                  const inter_slice_scheduler&  slice_sched)
{
  std::map<s_nssai_t, slice_prb_budget> budgets;
  unsigned num_configured_slices = slice_mgr.get_num_slices();

  // Step 1: Seed from slice_mgr (populated via REST/xApp set_slice_limits path).
  // Skip only if slice_mgr has no entries; Step 2 (inter-sched path) still runs regardless.
  if (num_configured_slices > 0) {
    const auto all_limits = slice_mgr.get_all_slice_limits();
    for (const auto& [snssai, limits] : all_limits) {
      slice_prb_budget budget;
      budget.budget_prbs    = limits.max(); // Authoritative from slice_mgr
      budget.allocated_prbs = 0;            // Reset every slot - CRITICAL for strict enforcement
      budget.guaranteed_met = false;
      budgets[snssai]       = budget;
    }
  }

  // Step 2: Cross-check with inter_slice_scheduler's cfg.rbs.max() per custom slice.
  // This is the authoritative value updated directly by xApp via handle_slice_reconfiguration_request.
  // If slice_mgr wasn't updated (common when only the RRC/E2 path fired), use cfg.rbs.max() instead.
  // If BOTH paths updated, use the STRICTER (min) of the two to never exceed either quota.
  //
  // Use compatible matching (exact first, then wildcard-SD) to avoid creating duplicate entries
  // when slice_mgr uses explicit SD but slice cfg uses wildcard SD (or vice-versa).
  const auto live_max_rbs = slice_sched.get_custom_slice_max_rbs();
  for (const auto& [snssai, sched_max] : live_max_rbs) {
    // Exact match first.
    auto it = budgets.find(snssai);
    if (it == budgets.end()) {
      // Compatible match: handles SST-matches-with-wildcard-SD case.
      it = std::find_if(budgets.begin(), budgets.end(), [&snssai](const auto& kv) {
        return snssai_compatible(kv.first, snssai);
      });
    }
    if (it != budgets.end()) {
      // Found existing entry (exact or compatible): take the stricter cap.
      it->second.budget_prbs = std::min(it->second.budget_prbs, sched_max);
    } else {
      // slice_mgr has no compatible entry but inter-slice scheduler does (xApp-only update path):
      // create the budget entry now so intra-slice enforcement can use it.
      slice_prb_budget budget;
      budget.budget_prbs    = sched_max;
      budget.allocated_prbs = 0;
      budget.guaranteed_met = false;
      budgets[snssai]       = budget;
    }
  }

  return budgets;
}

bool ue_scheduler_impl::slice_has_prb_budget(
    const s_nssai_t& snssai,
    unsigned requested_prbs,
    const std::map<s_nssai_t, slice_prb_budget>& budgets) const
{
  auto it = budgets.find(snssai);
  if (it == budgets.end()) {
    return false;
  }

  const auto& budget = it->second;
  return (budget.allocated_prbs + requested_prbs) <= budget.budget_prbs;
}

void ue_scheduler_impl::consume_slice_prbs(
    const s_nssai_t& snssai,
    unsigned allocated_prbs,
    std::map<s_nssai_t, slice_prb_budget>& budgets,
    cell_slice_resource_manager& slice_mgr)
{
  // Get slice limits from manager
  auto limits_opt = slice_mgr.get_slice_limits(snssai);
  if (!limits_opt.has_value()) {
    return;
  }

  const auto& limits = limits_opt.value();

  // Initialize budget if not present
  if (budgets.find(snssai) == budgets.end()) {
    budgets[snssai].budget_prbs    = limits.max();
    budgets[snssai].allocated_prbs = 0;
    budgets[snssai].guaranteed_met = false;
  }

  // Update allocated PRBs
  budgets[snssai].allocated_prbs += allocated_prbs;

  // Check if minimum guarantee met
  if (!budgets[snssai].guaranteed_met && budgets[snssai].allocated_prbs >= limits.min()) {
    budgets[snssai].guaranteed_met = true;
  }
}
