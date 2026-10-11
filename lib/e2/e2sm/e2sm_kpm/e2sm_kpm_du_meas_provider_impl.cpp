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

#include "e2sm_kpm_du_meas_provider_impl.h"

using namespace asn1::e2ap;
using namespace asn1::e2sm;
using namespace srsran;

e2sm_kpm_du_meas_provider_impl::e2sm_kpm_du_meas_provider_impl(srs_du::f1ap_ue_id_translator& f1ap_ue_id_translator_) :
  logger(srslog::fetch_basic_logger("E2SM-KPM")), f1ap_ue_id_provider(f1ap_ue_id_translator_)
{
  // Array of supported metrics.
  supported_metrics.emplace(
      "CQI", e2sm_kpm_supported_metric_t{NO_LABEL, UNKNOWN_LEVEL, false, &e2sm_kpm_du_meas_provider_impl::get_cqi});
  supported_metrics.emplace(
      "RSRP", e2sm_kpm_supported_metric_t{NO_LABEL, UNKNOWN_LEVEL, false, &e2sm_kpm_du_meas_provider_impl::get_rsrp});
  supported_metrics.emplace(
      "RSRQ", e2sm_kpm_supported_metric_t{NO_LABEL, UNKNOWN_LEVEL, false, &e2sm_kpm_du_meas_provider_impl::get_rsrq});

  supported_metrics.emplace(
      "RRU.PrbAvailDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_avail_dl});

  supported_metrics.emplace(
      "RRU.PrbAvailUl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_avail_ul});

  supported_metrics.emplace(
      "RRU.PrbUsedDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL | SLICE_ID_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_used_dl});

  supported_metrics.emplace(
      "RRU.PrbUsedUl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL | SLICE_ID_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_used_ul});

  supported_metrics.emplace(
      "RRU.PrbTotDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_use_perc_dl});

  supported_metrics.emplace(
      "RRU.PrbTotUl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prb_use_perc_ul});

  supported_metrics.emplace(
      "DRB.RlcSduDelayDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_drb_dl_rlc_sdu_latency});

  supported_metrics.emplace(
      "DRB.UEThpDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_drb_dl_mean_throughput});
  supported_metrics.emplace(
      "DRB.UEThpUl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, E2_NODE_LEVEL | UE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_drb_ul_mean_throughput});
  supported_metrics.emplace(
      "DRB.RlcPacketDropRateDl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_drb_rlc_packet_drop_rate_dl});
  supported_metrics.emplace(
      "DRB.RlcSduTransmittedVolumeDL",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_drb_rlc_sdu_transmitted_volume_dl});
  supported_metrics.emplace(
      "DRB.RlcSduTransmittedVolumeUL",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_drb_rlc_sdu_transmitted_volume_ul});

  supported_metrics.emplace(
      "DRB.AirIfDelayUl",
      e2sm_kpm_supported_metric_t{NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_delay_ul});

  supported_metrics.emplace(
      "DRB.RlcDelayUl",
      e2sm_kpm_supported_metric_t{
          NO_LABEL, ALL_LEVELS, true, &e2sm_kpm_du_meas_provider_impl::get_drb_ul_rlc_sdu_latency});

  supported_metrics.emplace("RACH.PreambleDedCell",
                            e2sm_kpm_supported_metric_t{
                                NO_LABEL, E2_NODE_LEVEL, true, &e2sm_kpm_du_meas_provider_impl::get_prach_cell_count});

  // Check if the supported metrics are matching e2sm_kpm metrics definitions.
  check_e2sm_kpm_metrics_definitions(get_e2sm_kpm_28_552_metrics());
  check_e2sm_kpm_metrics_definitions(get_e2sm_kpm_oran_metrics());
}

e2sm_kpm_du_meas_provider_impl::e2sm_kpm_du_meas_provider_impl(srs_du::f1ap_ue_id_translator& f1ap_ue_id_translator_,
                                                               int                            max_rlc_metrics_) :
  e2sm_kpm_du_meas_provider_impl(f1ap_ue_id_translator_)
{
  max_rlc_metrics = max_rlc_metrics_;
}

bool e2sm_kpm_du_meas_provider_impl::check_e2sm_kpm_metrics_definitions(span<const e2sm_kpm_metric_t> metric_defs)
{
  std::string metric_name;
  auto        name_matches = [&metric_name](const e2sm_kpm_metric_t& x) {
    return (x.name == metric_name.c_str() or x.name == metric_name);
  };

  for (auto& supported_metric : supported_metrics) {
    metric_name = supported_metric.first;
    auto it     = std::find_if(metric_defs.begin(), metric_defs.end(), name_matches);
    if (it == metric_defs.end()) {
      continue;
    }

    if (supported_metric.second.supported_labels & ~(it->optional_labels | e2sm_kpm_label_enum::NO_LABEL)) {
      logger.debug("Wrong definition of the supported metric: \"{}\", labels cannot be supported.", metric_name);
    }

    if (supported_metric.second.supported_levels & ~it->optional_levels) {
      logger.debug("Wrong definition of the supported metric: \"{}\", level cannot be supported.", metric_name);
    }

    if (is_cell_id_required(*it) and not supported_metric.second.cell_scope_supported) {
      logger.debug("Wrong definition of the supported metric: \"{}\", cell scope has to be supported.",
                   metric_name.c_str());
    }
  }
  return true;
}

void e2sm_kpm_du_meas_provider_impl::report_metrics(const scheduler_cell_metrics& cell_metrics)
{
  last_ue_metrics.clear();
  nof_cell_prbs          = cell_metrics.nof_prbs;
  nof_dl_slots           = cell_metrics.nof_dl_slots;
  nof_ul_slots           = cell_metrics.nof_ul_slots;
  nof_ded_cell_preambles = cell_metrics.nof_prach_preambles;

  // Phase 1: Store UE metrics with slice info (validated and working)
  for (auto& ue_metric : cell_metrics.ue_metrics) {
    last_ue_metrics.push_back(ue_metric);

    // Debug log: Verify s_nssai field is populated (only at debug level)
    if (ue_metric.s_nssai.has_value()) {
      logger.debug("UE {} metrics include slice SST={}, SD={}",
                   ue_metric.ue_index,
                   ue_metric.s_nssai->sst.value(),
                   ue_metric.s_nssai->sd.is_set() ? ue_metric.s_nssai->sd.value() : 0);
    }
  }

  // Scheduler metrics are always delivered, RLC only when UEs are present to clear RLC history.
  clear_rlc_metrics();
}

void e2sm_kpm_du_meas_provider_impl::report_metrics(const rlc_metrics& metrics)
{
  logger.debug("Received RLC metrics: ue={} {}.", fmt::underlying(metrics.ue_index), metrics.rb_id.get_drb_id());
  ue_aggr_rlc_metrics[metrics.ue_index].push_back(metrics);
  if (ue_aggr_rlc_metrics[metrics.ue_index].size() > max_rlc_metrics) {
    ue_aggr_rlc_metrics[metrics.ue_index].pop_front();
  }
}

void e2sm_kpm_du_meas_provider_impl::clear_rlc_metrics()
{
  // Check if enough time has passed since last clear (at least 1 second)
  auto current_time = std::chrono::system_clock::now();
  auto time_since_last_clear =
      std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_rlc_metrics_clear_time);

  if (time_since_last_clear.count() <= 1000) {
    return;
  }

  last_rlc_metrics_clear_time = current_time;
  auto it                     = ue_aggr_rlc_metrics.begin();
  while (it != ue_aggr_rlc_metrics.end()) {
    du_ue_index_t       ue_index   = to_du_ue_index(it->first);
    gnb_du_ue_f1ap_id_t ue_f1ap_id = f1ap_ue_id_provider.get_gnb_du_ue_f1ap_id(ue_index);
    if (ue_f1ap_id == gnb_du_ue_f1ap_id_t::invalid) {
      it = ue_aggr_rlc_metrics.erase(it);
    } else {
      ++it;
    }
  }
}

std::vector<std::string> e2sm_kpm_du_meas_provider_impl::get_supported_metric_names(e2sm_kpm_metric_level_enum level)
{
  std::vector<std::string> metrics;
  for (auto& m : supported_metrics) {
    if ((level & E2_NODE_LEVEL) and (m.second.supported_levels & E2_NODE_LEVEL)) {
      metrics.push_back(m.first);
    } else if ((level & UE_LEVEL) and (m.second.supported_levels & UE_LEVEL)) {
      metrics.push_back(m.first);
    }
  }
  return metrics;
}

bool e2sm_kpm_du_meas_provider_impl::is_cell_supported(const asn1::e2sm::cgi_c& cell_global_id)
{
  // TODO: check if CELL is supported
  return true;
}

bool e2sm_kpm_du_meas_provider_impl::is_ue_supported(const asn1::e2sm::ue_id_c& ueid)
{
  // TODO: check if UE is supported
  return true;
}

bool e2sm_kpm_du_meas_provider_impl::is_test_cond_supported(const asn1::e2sm::test_cond_type_c& test_cond_type)
{
  // TODO: check if test condition is supported
  return true;
}

bool e2sm_kpm_du_meas_provider_impl::is_metric_supported(const asn1::e2sm::meas_type_c&   meas_type,
                                                         const asn1::e2sm::meas_label_s&  label,
                                                         const e2sm_kpm_metric_level_enum level,
                                                         const bool&                      cell_scope)
{
  // Phase 2: Accept NO_LABEL (aggregate) and SLICE_ID_LABEL (slice-filtered)
  if (!label.no_label_present && !label.slice_id_present) {
    logger.debug("Label not supported. Only NO_LABEL and SLICE_ID_LABEL are supported.");
    return false;
  }

  logger.debug("E2SM-KPM: Label validation passed - NO_LABEL={}, SLICE_ID={}",
               label.no_label_present, label.slice_id_present);

  for (auto& metric : supported_metrics) {
    if (strcmp(meas_type.meas_name().to_string().c_str(), metric.first.c_str()) == 0) {
      logger.debug("E2SM-KPM: Metric '{}' is supported", metric.first);
      return true;
    }
  }

  logger.debug("E2SM-KPM: Metric '{}' not found in supported metrics list", meas_type.meas_name().to_string());
  // TODO: check if metric supported with required label, level and cell_scope
  return false;
}

bool e2sm_kpm_du_meas_provider_impl::get_ues_matching_test_conditions(
    const asn1::e2sm::matching_cond_list_l& matching_cond_list,
    std::vector<asn1::e2sm::ue_id_c>&       ues)
{
  // TODO: add test condition checking, now return all UEs
  for (const auto& ue : ue_aggr_rlc_metrics) {
    du_ue_index_t                      ue_index          = to_du_ue_index(ue.first);
    std::optional<gnb_cu_ue_f1ap_id_t> gnb_cu_ue_f1ap_id = f1ap_ue_id_provider.get_gnb_cu_ue_f1ap_id(ue_index);
    if (not gnb_cu_ue_f1ap_id.has_value()) {
      continue;
    }
    ue_id_c        ueid;
    ue_id_gnb_du_s ueid_gnb_du{};
    ueid_gnb_du.gnb_cu_ue_f1ap_id = gnb_cu_ue_f1ap_id_to_uint(gnb_cu_ue_f1ap_id.value());
    ueid_gnb_du.ran_ue_id_present = false;
    ueid.set_gnb_du_ue_id()       = ueid_gnb_du;
    ues.push_back(ueid);
  }

  return true;
}

bool e2sm_kpm_du_meas_provider_impl::get_ues_matching_test_conditions(
    const asn1::e2sm::matching_ue_cond_per_sub_list_l& matching_ue_cond_list,
    std::vector<asn1::e2sm::ue_id_c>&                  ues)
{
  // TODO: add test condition checking, now return all UEs
  for (const auto& ue : ue_aggr_rlc_metrics) {
    du_ue_index_t                      ue_index          = to_du_ue_index(ue.first);
    std::optional<gnb_cu_ue_f1ap_id_t> gnb_cu_ue_f1ap_id = f1ap_ue_id_provider.get_gnb_cu_ue_f1ap_id(ue_index);
    if (not gnb_cu_ue_f1ap_id.has_value()) {
      continue;
    }
    ue_id_c        ueid;
    ue_id_gnb_du_s ueid_gnb_du{};
    ueid_gnb_du.gnb_cu_ue_f1ap_id = gnb_cu_ue_f1ap_id_to_uint(gnb_cu_ue_f1ap_id.value());
    ueid_gnb_du.ran_ue_id_present = false;
    ueid.set_gnb_du_ue_id()       = ueid_gnb_du;
    ues.push_back(ueid);
  }

  return true;
}

bool e2sm_kpm_du_meas_provider_impl::get_meas_data(const asn1::e2sm::meas_type_c&               meas_type,
                                                   const asn1::e2sm::label_info_list_l          label_info_list,
                                                   const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                   const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                   std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  metric_meas_getter_func_ptr metric_meas_getter_func;
  auto                        it = supported_metrics.find(meas_type.meas_name().to_string().c_str());
  if (it == supported_metrics.end()) {
    logger.debug("Metric {} not supported.", meas_type.meas_name().to_string());
    return false;
  }
  metric_meas_getter_func = it->second.func;
  srsran_assert(metric_meas_getter_func != nullptr, "Metric getter function cannot be empty.");
  return (this->*metric_meas_getter_func)(label_info_list, ues, cell_global_id, items);
}

bool e2sm_kpm_du_meas_provider_impl::handle_no_meas_data_available(
    const std::vector<asn1::e2sm::ue_id_c>&        ues,
    std::vector<asn1::e2sm::meas_record_item_c>&   items,
    asn1::e2sm::meas_record_item_c::types::options value_type)
{
  if (ues.empty()) {
    // Fill with zero if E2 Node Measurement (Report Style 1)
    meas_record_item_c meas_record_item;
    if (value_type == asn1::e2sm::meas_record_item_c::types::options::integer) {
      meas_record_item.set_integer() = 0;
    } else if (value_type == asn1::e2sm::meas_record_item_c::types::options::real) {
      meas_record_item.set_real();
      meas_record_item.real().value = 0;
    } else if (value_type == asn1::e2sm::meas_record_item_c::types::options::not_satisfied) {
      meas_record_item.set_not_satisfied();
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    return true;
  }
  return false;
}

bool e2sm_kpm_du_meas_provider_impl::get_cqi(const asn1::e2sm::label_info_list_l          label_info_list,
                                             const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                             const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                             std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  scheduler_ue_metrics ue_metrics = last_ue_metrics[0];

  meas_record_item_c meas_record_item;
  meas_record_item.set_integer() = ue_metrics.cqi_stats.get_nof_observations() > 0
                                       ? static_cast<uint64_t>(std::round(ue_metrics.cqi_stats.get_mean()))
                                       : 0;
  items.push_back(meas_record_item);
  meas_collected = true;

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_rsrp(const asn1::e2sm::label_info_list_l          label_info_list,
                                              const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                              const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                              std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  scheduler_ue_metrics ue_metrics = last_ue_metrics[0];

  meas_record_item_c meas_record_item;
  meas_record_item.set_integer() = (int)ue_metrics.pusch_snr_db;
  items.push_back(meas_record_item);
  meas_collected = true;

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_rsrq(const asn1::e2sm::label_info_list_l          label_info_list,
                                              const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                              const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                              std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  scheduler_ue_metrics ue_metrics = last_ue_metrics[0];

  meas_record_item_c meas_record_item;
  meas_record_item.set_integer() = (int)ue_metrics.pusch_snr_db;
  items.push_back(meas_record_item);
  meas_collected = true;

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_avail_dl(const asn1::e2sm::label_info_list_l          label_info_list,
                                                      const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                      const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                      std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: RRU.PrbAvailDl supports only NO_LABEL label.");
    return meas_collected;
  }
  int mean_dl_prbs_used =
      std::accumulate(last_ue_metrics.begin(),
                      last_ue_metrics.end(),
                      0,
                      [](size_t sum, const scheduler_ue_metrics& metric) { return sum + metric.tot_pdsch_prbs_used; }) /
      nof_dl_slots;

  for (size_t i = 0; i < std::max(ues.size(), size_t(1)); ++i) {
    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = (nof_cell_prbs - mean_dl_prbs_used);
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_avail_ul(const asn1::e2sm::label_info_list_l          label_info_list,
                                                      const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                      const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                      std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: RRU.PrbAvailUl supports only NO_LABEL label.");
    return meas_collected;
  }
  int mean_ul_prbs_used =
      std::accumulate(last_ue_metrics.begin(),
                      last_ue_metrics.end(),
                      0,
                      [](size_t sum, const scheduler_ue_metrics& metric) { return sum + metric.tot_pusch_prbs_used; }) /
      nof_ul_slots;

  for (size_t i = 0; i < std::max(ues.size(), size_t(1)); ++i) {
    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = (nof_cell_prbs - mean_ul_prbs_used);
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_used_dl(const asn1::e2sm::label_info_list_l          label_info_list,
                                                     const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                     const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                     std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;

  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }

  // Phase 2B: Parse label to check for SLICE_ID filtering
  std::optional<s_nssai_t> requested_slice;
  bool                     has_slice_filter = false;

  if (label_info_list.size() == 1) {
    const auto& label = label_info_list[0].meas_label;
    if (label.slice_id_present) {
      requested_slice  = extract_slice_id_from_label(label);
      has_slice_filter = true;
      logger.debug("Phase 2B: RRU.PrbUsedDl filtering by slice SST={}, SD={}",
                   requested_slice.value().sst.value(),
                   requested_slice.value().sd.is_set() ? requested_slice.value().sd.value() : 0);
    } else if (!label.no_label_present) {
      logger.debug("Metric: RRU.PrbUsedDl supports only NO_LABEL and SLICE_ID labels.");
      return meas_collected;
    }
  } else if (label_info_list.size() > 1) {
    logger.debug("Metric: RRU.PrbUsedDl supports only single label.");
    return meas_collected;
  }

  // Phase 2B: E2 Node level metrics (aggregate across UEs)
  if (ues.empty()) {
    double dl_prbs_used = 0;
    unsigned ue_count   = 0;

    if (has_slice_filter) {
      // Filter by slice
      logger.debug("Phase 2B: Filtering {} UE metrics for slice SST={}, SD={}",
                   last_ue_metrics.size(),
                   requested_slice.value().sst.value(),
                   requested_slice.value().sd.is_set() ? requested_slice.value().sd.value() : 0);
      for (size_t metric_idx = 0; metric_idx < last_ue_metrics.size(); ++metric_idx) {
        const auto& metric = last_ue_metrics[metric_idx];
        if (metric.s_nssai.has_value()) {
          const auto& ue_slice = metric.s_nssai.value();
          const auto& req_slice = requested_slice.value();

          // Compare S-NSSAI: SST must match, SD is optional
          bool sst_match = (ue_slice.sst == req_slice.sst);
          bool sd_match = true;

          // Only enforce SD matching if request has explicit non-zero SD
          if (req_slice.sd.is_set() && req_slice.sd.value() != 0) {
            // Request wants specific SD - UE must match
            sd_match = ue_slice.sd.is_set() && (ue_slice.sd.value() == req_slice.sd.value());
          }

          if (sst_match && sd_match) {
            logger.debug("Phase 2B: MATCH! Adding UE with {} DL PRBs", metric.tot_pdsch_prbs_used);
            dl_prbs_used += metric.tot_pdsch_prbs_used;
            ue_count++;
          } else {
            logger.debug("Phase 2B: NO MATCH - skipping UE");
          }
        } else {
          logger.debug("Phase 2B: UE has no slice info - skipping");
        }
      }

      logger.debug("Phase 2B: RRU.PrbUsedDl slice-filtered {} UEs, {} PRBs", ue_count, dl_prbs_used);

      // Update per-slice statistics
      if (requested_slice.has_value()) {
        update_slice_prb_stats(requested_slice.value(), (uint32_t)dl_prbs_used, 0, ue_count);
      }
    } else {
      // No filter - aggregate all UEs
      dl_prbs_used = std::accumulate(last_ue_metrics.begin(),
                                     last_ue_metrics.end(),
                                     0.0,
                                     [](double sum, const scheduler_ue_metrics& metric) {
                                       return sum + metric.tot_pdsch_prbs_used;
                                     });
      logger.debug("Phase 2B: RRU.PrbUsedDl aggregate {} PRBs", dl_prbs_used);
    }

    dl_prbs_used = nof_dl_slots > 0 ? dl_prbs_used / nof_dl_slots : 0;

    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = dl_prbs_used;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  // Phase 2B: UE-level metrics
  for (auto& ue : ues) {
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    meas_record_item_c  meas_record_item;

    if (ue_idx != du_ue_index_t::INVALID_DU_UE_INDEX && ue_idx < last_ue_metrics.size()) {
      const auto& ue_metric = last_ue_metrics[ue_idx];

      // Apply slice filter if requested
      if (has_slice_filter) {
        if (ue_metric.s_nssai.has_value() && ue_metric.s_nssai.value() == requested_slice.value()) {
          unsigned ue_mean_dl_prbs_used = nof_dl_slots > 0 ? ue_metric.tot_pdsch_prbs_used / nof_dl_slots : 0;
          meas_record_item.set_integer() = ue_mean_dl_prbs_used;
        } else {
          // UE not in requested slice
          meas_record_item.set_no_value();
        }
      } else {
        // No filter - return UE metric
        unsigned ue_mean_dl_prbs_used = nof_dl_slots > 0 ? ue_metric.tot_pdsch_prbs_used / nof_dl_slots : 0;
        meas_record_item.set_integer() = ue_mean_dl_prbs_used;
      }
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_used_ul(const asn1::e2sm::label_info_list_l          label_info_list,
                                                     const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                     const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                     std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }

  // Phase 2C: Parse label to check for SLICE_ID filtering
  std::optional<s_nssai_t> requested_slice;
  bool                     has_slice_filter = false;

  if (label_info_list.size() == 1) {
    const auto& label = label_info_list[0].meas_label;
    if (label.slice_id_present) {
      requested_slice  = extract_slice_id_from_label(label);
      has_slice_filter = true;
      logger.debug("Phase 2C: RRU.PrbUsedUl filtering by slice SST={}, SD={}",
                   requested_slice.value().sst.value(),
                   requested_slice.value().sd.is_set() ? requested_slice.value().sd.value() : 0);
    } else if (!label.no_label_present) {
      logger.debug("Metric: RRU.PrbUsedUl supports only NO_LABEL and SLICE_ID labels.");
      return meas_collected;
    }
  } else if (label_info_list.size() > 1) {
    logger.debug("Metric: RRU.PrbUsedUl supports only single label.");
    return meas_collected;
  }

  // Phase 2C: E2 Node level metrics (aggregate across UEs)
  if (ues.empty()) {
    double ul_prbs_used = 0;
    unsigned ue_count   = 0;

    if (has_slice_filter) {
      // Filter by slice
      logger.debug("Phase 2C: Filtering {} UE metrics for slice SST={}, SD={}",
                   last_ue_metrics.size(),
                   requested_slice.value().sst.value(),
                   requested_slice.value().sd.is_set() ? requested_slice.value().sd.value() : 0);
      for (size_t metric_idx = 0; metric_idx < last_ue_metrics.size(); ++metric_idx) {
        const auto& metric = last_ue_metrics[metric_idx];
        if (metric.s_nssai.has_value()) {
          // STEP 4: Verify slice comparison logic
          const auto& ue_slice = metric.s_nssai.value();
          const auto& req_slice = requested_slice.value();

          logger.debug("Phase 2C: UE slice SST={}, SD={} (0x{:x}), is_set={}, comparing with requested SST={}, SD={} (0x{:x}), is_set={}",
                       metric.s_nssai.value().sst.value(),
                       metric.s_nssai.value().sd.value(),
                       metric.s_nssai.value().sd.value(),
                       metric.s_nssai.value().sd.is_set(),
                       requested_slice.value().sst.value(),
                       requested_slice.value().sd.value(),
                       requested_slice.value().sd.value(),
                       requested_slice.value().sd.is_set());
          // Compare S-NSSAI: SST must match, SD is optional
          bool sst_match = (ue_slice.sst == req_slice.sst);
          bool sd_match = true;

          // Only enforce SD matching if request has explicit non-zero SD
          if (req_slice.sd.is_set() && req_slice.sd.value() != 0) {
            // Request wants specific SD - UE must match
            sd_match = ue_slice.sd.is_set() && (ue_slice.sd.value() == req_slice.sd.value());
          }
          // Otherwise: SD=0 or SD not set → match based on SST only

          if (sst_match && sd_match) {
            logger.debug("Phase 2C: MATCH! Adding UE with {} UL PRBs", metric.tot_pusch_prbs_used);
            ul_prbs_used += metric.tot_pusch_prbs_used;
            ue_count++;
          } else {
            logger.debug("Phase 2C: NO MATCH - skipping UE");
          }
        } else {
          logger.debug("Phase 2C: UE has no slice info - skipping");
        }
      }


      logger.debug("Phase 2C: RRU.PrbUsedUl slice-filtered {} UEs, {} PRBs", ue_count, ul_prbs_used);

      // Update per-slice statistics (merge with existing DL stats)
      if (requested_slice.has_value()) {
        uint64_t key = slice_key(requested_slice.value());
        if (slice_stats_map.find(key) != slice_stats_map.end()) {
          slice_stats_map[key].ul_prb_used = (uint32_t)ul_prbs_used;
        } else {
          update_slice_prb_stats(requested_slice.value(), 0, (uint32_t)ul_prbs_used, ue_count);
        }
      }

    } else {
      // No filter - aggregate all UEs
      ul_prbs_used = std::accumulate(last_ue_metrics.begin(),
                                     last_ue_metrics.end(),
                                     0.0,
                                     [](double sum, const scheduler_ue_metrics& metric) {
                                       return sum + metric.tot_pusch_prbs_used;
                                     });
      logger.debug("Phase 2C: RRU.PrbUsedUl aggregate {} PRBs", ul_prbs_used);
    }

    ul_prbs_used = nof_ul_slots > 0 ? ul_prbs_used / nof_ul_slots : 0;


    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = ul_prbs_used;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  // Phase 2C: UE-level metrics
  for (auto& ue : ues) {
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    meas_record_item_c  meas_record_item;

    if (ue_idx != du_ue_index_t::INVALID_DU_UE_INDEX && ue_idx < last_ue_metrics.size()) {
      const auto& ue_metric = last_ue_metrics[ue_idx];

      // Apply slice filter if requested
      if (has_slice_filter) {
        if (ue_metric.s_nssai.has_value() && ue_metric.s_nssai.value() == requested_slice.value()) {
          unsigned ue_mean_ul_prbs_used = nof_ul_slots > 0 ? ue_metric.tot_pusch_prbs_used / nof_ul_slots : 0;
          meas_record_item.set_integer() = ue_mean_ul_prbs_used;
        } else {
          // UE not in requested slice
          meas_record_item.set_no_value();
        }
      } else {
        // No filter - return UE metric
        unsigned ue_mean_ul_prbs_used = nof_ul_slots > 0 ? ue_metric.tot_pusch_prbs_used / nof_ul_slots : 0;
        meas_record_item.set_integer() = ue_mean_ul_prbs_used;
      }
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_use_perc_dl(const asn1::e2sm::label_info_list_l          label_info_list,
                                                         const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                         const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                         std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: RRU.PrbTotDl supports only NO_LABEL label.");
    return meas_collected;
  }

  if (ues.empty()) {
    double mean_dl_prbs_used = std::accumulate(last_ue_metrics.begin(),
                                               last_ue_metrics.end(),
                                               0,
                                               [](size_t sum, const scheduler_ue_metrics& metric) {
                                                 return sum + metric.tot_pdsch_prbs_used;
                                               }) /
                               nof_dl_slots;
    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = mean_dl_prbs_used * 100 / nof_cell_prbs;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  for (auto& ue : ues) {
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    meas_record_item_c  meas_record_item;
    if (ue_idx != du_ue_index_t::INVALID_DU_UE_INDEX && ue_idx < last_ue_metrics.size()) {
      unsigned ue_mean_dl_prbs_used = nof_dl_slots > 0 ? last_ue_metrics[ue_idx].tot_pdsch_prbs_used / nof_dl_slots : 0;
      meas_record_item.set_integer() = ue_mean_dl_prbs_used * 100 / nof_cell_prbs;
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prb_use_perc_ul(const asn1::e2sm::label_info_list_l          label_info_list,
                                                         const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                         const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                         std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: RRU.PrbTotUl supports only NO_LABEL label.");
    return meas_collected;
  }
  if (ues.empty()) {
    double mean_ul_prbs_used = std::accumulate(last_ue_metrics.begin(),
                                               last_ue_metrics.end(),
                                               0,
                                               [](size_t sum, const scheduler_ue_metrics& metric) {
                                                 return sum + metric.tot_pusch_prbs_used;
                                               }) /
                               nof_ul_slots;
    meas_record_item_c meas_record_item;
    meas_record_item.set_integer() = mean_ul_prbs_used * 100 / nof_cell_prbs;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  for (auto& ue : ues) {
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    meas_record_item_c  meas_record_item;
    if (ue_idx != du_ue_index_t::INVALID_DU_UE_INDEX && ue_idx < last_ue_metrics.size()) {
      unsigned ue_mean_ul_prbs_used = nof_ul_slots > 0 ? last_ue_metrics[ue_idx].tot_pusch_prbs_used / nof_ul_slots : 0;
      meas_record_item.set_integer() = ue_mean_ul_prbs_used * 100 / nof_cell_prbs;
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}
bool e2sm_kpm_du_meas_provider_impl::get_delay_ul(const asn1::e2sm::label_info_list_l          label_info_list,
                                                  const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                  const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                  std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::no_value);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.AirIfDelayUl supports only NO_LABEL label.");
    return meas_collected;
  }

  if (ues.empty()) {
    float mean_ul_delay_ms = std::accumulate(last_ue_metrics.begin(),
                                             last_ue_metrics.end(),
                                             0,
                                             [](size_t sum, const scheduler_ue_metrics& metric) {
                                               return sum + metric.avg_crc_delay_ms.value_or(0.0f);
                                             }) /
                             last_ue_metrics.size();
    meas_record_item_c meas_record_item;
    if (mean_ul_delay_ms) {
      meas_record_item.set_real().value = mean_ul_delay_ms * 10; // unit is 0.1ms
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  for (auto& ue : ues) {
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    meas_record_item_c  meas_record_item;
    if (ue_idx != du_ue_index_t::INVALID_DU_UE_INDEX && ue_idx < last_ue_metrics.size()) {
      if (last_ue_metrics[ue_idx].avg_crc_delay_ms.has_value()) {
        meas_record_item.set_real().value = (last_ue_metrics[ue_idx].avg_crc_delay_ms.value() * 10); // unit is 0.1ms
      } else {
        meas_record_item.set_no_value();
      }
    } else {
      meas_record_item.set_no_value();
    }
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_prach_cell_count(const asn1::e2sm::label_info_list_l          label_info_list,
                                                          const std::vector<asn1::e2sm::ue_id_c>&      ues,
                                                          const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
                                                          std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (last_ue_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: RACH.PreambleDedCell supports only NO_LABEL label.");
    return meas_collected;
  }
  meas_record_item_c meas_record_item;
  meas_record_item.set_integer() = nof_ded_cell_preambles;
  items.push_back(meas_record_item);
  meas_collected = true;

  return meas_collected;
}

float e2sm_kpm_du_meas_provider_impl::bytes_to_kbits(float value)
{
  constexpr unsigned nof_bits_per_byte = 8;
  return (nof_bits_per_byte * value / 1e3);
}

// === Per-Slice PRB Statistics Management ===

void e2sm_kpm_du_meas_provider_impl::clear_slice_prb_stats()
{
  slice_stats_map.clear();
}

uint64_t e2sm_kpm_du_meas_provider_impl::slice_key(const s_nssai_t& slice) const
{
  uint64_t key = (static_cast<uint64_t>(slice.sst.value()) << 32);
  if (slice.sd.is_set()) {
    key |= slice.sd.value();
  }
  return key;
}

void e2sm_kpm_du_meas_provider_impl::update_slice_prb_stats(const s_nssai_t& slice,
                                                             uint32_t         dl_prbs,
                                                             uint32_t         ul_prbs,
                                                             uint32_t         ue_cnt)
{
  uint64_t key = slice_key(slice);
  auto&    stats = slice_stats_map[key];

  // Initialize on first access
  if (stats.sst == 0) {
    stats.sst       = slice.sst.value();
    stats.sd        = slice.sd.is_set() ? slice.sd.value() : 0;
    stats.sd_is_set = slice.sd.is_set();
  }

  stats.dl_prb_used = dl_prbs;
  stats.ul_prb_used = ul_prbs;
  stats.ue_count    = ue_cnt;
}

void e2sm_kpm_du_meas_provider_impl::log_per_slice_prb_utilization()
{
  if (slice_stats_map.empty()) {
    return;
  }

  if (nof_cell_prbs == 0) {
    logger.warning("[E2-KPM][Slice] Cannot compute utilization: nof_cell_prbs=0");
    return;
  }

  logger.info("[E2-KPM][Slice] === Per-Slice PRB Utilization ===");

  for (const auto& [key, stats] : slice_stats_map) {
    double dl_util = (stats.dl_prb_used * 100.0) / nof_cell_prbs;
    double ul_util = (stats.ul_prb_used * 100.0) / nof_cell_prbs;

    if (stats.sd_is_set) {
      logger.info("[E2-KPM][Slice] S-NSSAI: SST={} SD=0x{:06x}", stats.sst, stats.sd);
    } else {
      logger.info("[E2-KPM][Slice] S-NSSAI: SST={} SD=<not_set>", stats.sst);
    }
    logger.info("[E2-KPM][Slice]   DL_PRB_Used={}", stats.dl_prb_used);
    logger.info("[E2-KPM][Slice]   UL_PRB_Used={}", stats.ul_prb_used);
    logger.info("[E2-KPM][Slice]   Total_PRB={}", nof_cell_prbs);
    logger.info("[E2-KPM][Slice]   DL_Utilization={:.1f}%", dl_util);
    logger.info("[E2-KPM][Slice]   UL_Utilization={:.1f}%", ul_util);
    logger.info("[E2-KPM][Slice]   UE_Count={}", stats.ue_count);
  }
}

// Phase 2A: Extract S-NSSAI from measurement label for slice filtering
std::optional<s_nssai_t> e2sm_kpm_du_meas_provider_impl::extract_slice_id_from_label(
    const asn1::e2sm::meas_label_s& meas_label)
{
  if (!meas_label.slice_id_present) {
    logger.debug("Phase 2A: No slice_id in measurement label");
    return std::nullopt;
  }

  s_nssai_t slice_id;

  // Extract SST (Slice/Service Type) - always present in S-NSSAI
  uint8_t sst_value = static_cast<uint8_t>(meas_label.slice_id.sst.to_number());
  slice_id.sst = slice_service_type(sst_value);
  logger.debug("Phase 2A: Extracted SST={}", sst_value);

  // Extract SD (Slice Differentiator) if present - optional in S-NSSAI
  logger.debug("Phase 2A: sd_present={}", meas_label.slice_id.sd_present);
  if (meas_label.slice_id.sd_present) {
    uint32_t sd_value = static_cast<uint32_t>(meas_label.slice_id.sd.to_number());
    logger.debug("Phase 2A: Extracted SD value={} (0x{:x})", sd_value, sd_value);
    auto sd_result = slice_differentiator::create(sd_value);
    if (sd_result.has_value()) {
      slice_id.sd = sd_result.value();
      logger.debug("Phase 2A: SD successfully created, is_set={}, value={} (0x{:x})",
                   slice_id.sd.is_set(), slice_id.sd.value(), slice_id.sd.value());
    } else {
      logger.warning("Phase 2A: Invalid SD value in measurement label");
      return std::nullopt;
    }
  } else {
    logger.debug("Phase 2A: SD not present in label, using default (0x{:x})", slice_id.sd.value());
  }

  logger.debug("Phase 2A: Requested Slice from xApp SST={}, SD={} (0x{:x}), is_set={}",
               slice_id.sst.value(),
               slice_id.sd.is_set() ? slice_id.sd.value() : 0,
               slice_id.sd.is_set() ? slice_id.sd.value() : 0,
               slice_id.sd.is_set());

  logger.debug("Phase 2A: Final extracted slice_id: SST={}, SD={} (0x{:x}), is_set={}",
               slice_id.sst.value(),
               slice_id.sd.value(),
               slice_id.sd.value(),
               slice_id.sd.is_set());

  return slice_id;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_dl_mean_throughput(const asn1::e2sm::label_info_list_l     label_info_list,
                                                                const std::vector<asn1::e2sm::ue_id_c>& ues,
                                                                const std::optional<asn1::e2sm::cgi_c>  cell_global_id,
                                                                std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::real);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.UEThpDl supports only NO_LABEL label.");
    return meas_collected;
  }
  double                       seconds = 1;
  std::map<uint16_t, unsigned> ue_throughput;
  for (auto& ue : ue_aggr_rlc_metrics) {
    size_t num_pdu_bytes_with_segmentation;
    if (std::holds_alternative<rlc_um_tx_metrics_lower>(ue.second.front().tx.tx_low.mode_specific)) {
      // get average from queue
      num_pdu_bytes_with_segmentation =
          std::accumulate(ue.second.begin(), ue.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            auto& um = std::get<rlc_um_tx_metrics_lower>(metric.tx.tx_low.mode_specific);
            return sum + um.num_pdu_bytes_with_segmentation;
          });
      num_pdu_bytes_with_segmentation /= ue.second.size();
    } else if (std::holds_alternative<rlc_am_tx_metrics_lower>(ue.second.front().tx.tx_low.mode_specific)) {
      num_pdu_bytes_with_segmentation =
          std::accumulate(ue.second.begin(), ue.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            auto& am = std::get<rlc_am_tx_metrics_lower>(metric.tx.tx_low.mode_specific);
            return sum + am.num_pdu_bytes_with_segmentation;
          });
      num_pdu_bytes_with_segmentation /= ue.second.size();
    } else {
      num_pdu_bytes_with_segmentation = 0;
    }
    auto num_pdu_bytes_no_segmentation =
        std::accumulate(ue.second.begin(), ue.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
          return sum + metric.tx.tx_low.num_pdu_bytes_no_segmentation;
        });
    num_pdu_bytes_no_segmentation /= ue.second.size();
    seconds = (float)std::chrono::duration_cast<std::chrono::milliseconds>(ue.second.back().metrics_period).count() /
              (float)1000;
    ue_throughput[ue.first] = bytes_to_kbits(num_pdu_bytes_no_segmentation + num_pdu_bytes_with_segmentation) / seconds;
  }
  if (ues.empty()) {
    meas_record_item_c meas_record_item;
    int                total_throughput = 0;
    for (auto& ue : ue_throughput) {
      total_throughput += ue.second;
    }
    meas_record_item.set_real().value = total_throughput;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  for (auto& ue : ues) {
    meas_record_item_c  meas_record_item;
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    if (ue_throughput.count(ue_idx) == 0) {
      meas_record_item.set_no_value();
      items.push_back(meas_record_item);
      meas_collected = true;
      continue;
    }
    meas_record_item.set_real().value = ue_throughput[ue_idx];
    items.push_back(meas_record_item);
    meas_collected = true;
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_ul_mean_throughput(const asn1::e2sm::label_info_list_l     label_info_list,
                                                                const std::vector<asn1::e2sm::ue_id_c>& ues,
                                                                const std::optional<asn1::e2sm::cgi_c>  cell_global_id,
                                                                std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::real);
  }
  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.UEThpUl supports only NO_LABEL label.");
    return meas_collected;
  }

  double                       seconds = 1;
  std::map<uint16_t, unsigned> ue_throughput;
  for (auto& ue : ue_aggr_rlc_metrics) {
    auto num_pdu_bytes =
        std::accumulate(ue.second.begin(), ue.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
          return sum + metric.rx.num_pdu_bytes;
        });
    num_pdu_bytes /= ue.second.size();
    seconds = (float)std::chrono::duration_cast<std::chrono::milliseconds>(ue.second.back().metrics_period).count() /
              (float)1000;
    ue_throughput[ue.first] = bytes_to_kbits(num_pdu_bytes) / seconds; // unit is kbps
  }
  if (ues.empty()) {
    meas_record_item_c meas_record_item;
    int                total_throughput = 0;
    for (auto& ue : ue_throughput) {
      total_throughput += ue.second;
    }
    meas_record_item.set_real().value = total_throughput;
    items.push_back(meas_record_item);
    meas_collected = true;
  }

  for (auto& ue : ues) {
    meas_record_item_c  meas_record_item;
    gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
    uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
    if (ue_throughput.count(ue_idx) == 0) {
      meas_record_item.set_no_value();
      items.push_back(meas_record_item);
      meas_collected = true;
      continue;
    }
    meas_record_item.set_real().value = ue_throughput[ue_idx];
    items.push_back(meas_record_item);
    meas_collected = true;
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_rlc_packet_drop_rate_dl(
    const asn1::e2sm::label_info_list_l          label_info_list,
    const std::vector<asn1::e2sm::ue_id_c>&      ues,
    const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
    std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::no_value);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.RlcPacketDropRateDl supports only NO_LABEL label.");
    return meas_collected;
  }

  if (cell_global_id.has_value()) {
    logger.debug("Metric: DRB.RlcPacketDropRateDl currently does not support cell_global_id filter.");
  }

  if (ues.empty()) {
    // E2 level measurements.
    meas_record_item_c meas_record_item;
    float              drop_rate          = 0;
    uint32_t           total_dropped_sdus = 0;
    uint32_t           total_tx_num_sdus  = 0;
    for (auto& rlc_metric : ue_aggr_rlc_metrics) {
      total_dropped_sdus += std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.tx.tx_high.num_dropped_sdus + metric.tx.tx_high.num_discarded_sdus;
          });
      total_tx_num_sdus += std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.tx.tx_high.num_sdus;
          });
    }
    if (total_tx_num_sdus) {
      drop_rate = 1.0 * total_dropped_sdus / total_tx_num_sdus;
    }
    uint32_t drop_rate_int         = drop_rate * 100;
    meas_record_item.set_integer() = drop_rate_int;
    items.push_back(meas_record_item);
    meas_collected = true;
  } else {
    // UE level measurements.
    for (auto& ue : ues) {
      meas_record_item_c  meas_record_item;
      gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
      uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
      if (ue_aggr_rlc_metrics.count(ue_idx) == 0) {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
        continue;
      }
      float    drop_rate = 0;
      uint32_t total_dropped_sdus =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) {
                            return sum + metric.tx.tx_high.num_dropped_sdus + metric.tx.tx_high.num_discarded_sdus;
                          });
      uint32_t total_tx_num_sdus =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.tx.tx_high.num_sdus; });
      if (total_tx_num_sdus) {
        drop_rate = 1.0 * total_dropped_sdus / total_tx_num_sdus;
      }
      uint32_t drop_rate_int         = drop_rate * 100;
      meas_record_item.set_integer() = drop_rate_int;
      items.push_back(meas_record_item);
      meas_collected = true;
    }
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_rlc_sdu_transmitted_volume_dl(
    const asn1::e2sm::label_info_list_l          label_info_list,
    const std::vector<asn1::e2sm::ue_id_c>&      ues,
    const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
    std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.RlcSduTransmittedVolumeDL supports only NO_LABEL label.");
    return false;
  }

  if (cell_global_id.has_value()) {
    logger.debug("Metric: DRB.RlcSduTransmittedVolumeDL currently does not support cell_global_id filter.");
  }

  if (ues.empty()) {
    // E2 level measurements.
    meas_record_item_c meas_record_item;
    size_t             total_tx_num_sdu_bytes = 0;
    for (auto& rlc_metric : ue_aggr_rlc_metrics) {
      total_tx_num_sdu_bytes += std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.tx.tx_high.num_sdu_bytes;
          });
    }
    meas_record_item.set_integer() = total_tx_num_sdu_bytes * 8 / 1000; // unit is kbit
    items.push_back(meas_record_item);
    meas_collected = true;
  } else {
    // UE level measurements.
    for (auto& ue : ues) {
      meas_record_item_c  meas_record_item;
      gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
      uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
      if (ue_aggr_rlc_metrics.count(ue_idx) == 0) {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
        continue;
      }
      int num_sdu_bytes =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.tx.tx_high.num_sdu_bytes; });
      meas_record_item.set_integer() = num_sdu_bytes * 8 / 1000; // unit is kbit
      items.push_back(meas_record_item);
      meas_collected = true;
    }
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_rlc_sdu_transmitted_volume_ul(
    const asn1::e2sm::label_info_list_l          label_info_list,
    const std::vector<asn1::e2sm::ue_id_c>&      ues,
    const std::optional<asn1::e2sm::cgi_c>       cell_global_id,
    std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::integer);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.RlcSduTransmittedVolumeUL supports only NO_LABEL label.");
    return meas_collected;
  }

  if (cell_global_id.has_value()) {
    logger.debug("Metric: DRB.RlcSduTransmittedVolumeUL currently does not support cell_global_id filter.");
  }

  if (ues.empty()) {
    // E2 level measurements.
    meas_record_item_c meas_record_item;
    size_t             total_rx_num_sdu_bytes = 0;
    for (auto& rlc_metric : ue_aggr_rlc_metrics) {
      total_rx_num_sdu_bytes += std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.rx.num_sdu_bytes;
          });
    }
    meas_record_item.set_integer() = total_rx_num_sdu_bytes * 8 / 1000; // unit is kbit
    items.push_back(meas_record_item);
    meas_collected = true;
  } else {
    // UE level measurements.
    for (auto& ue : ues) {
      meas_record_item_c  meas_record_item;
      gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
      uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
      if (ue_aggr_rlc_metrics.count(ue_idx) == 0) {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
        continue;
      }
      int num_sdu_bytes =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.rx.num_sdu_bytes; });
      meas_record_item.set_integer() = num_sdu_bytes * 8 / 1000; // unit is kbit
      items.push_back(meas_record_item);
      meas_collected = true;
    }
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_dl_rlc_sdu_latency(const asn1::e2sm::label_info_list_l     label_info_list,
                                                                const std::vector<asn1::e2sm::ue_id_c>& ues,
                                                                const std::optional<asn1::e2sm::cgi_c>  cell_global_id,
                                                                std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::no_value);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.RlcSduDelayDl supports only NO_LABEL label.");
    return meas_collected;
  }

  if (ues.empty()) {
    meas_record_item_c meas_record_item;
    float              av_ue_sdu_latency_us = 0;
    for (auto& rlc_metric : ue_aggr_rlc_metrics) {
      int tot_num_of_pulled_sdus = std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.tx.tx_low.num_of_pulled_sdus;
          });
      int tot_sum_sdu_latency_us = std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.tx.tx_low.sum_sdu_latency_us;
          });
      if (tot_num_of_pulled_sdus && tot_sum_sdu_latency_us) {
        av_ue_sdu_latency_us += (float)tot_sum_sdu_latency_us / (float)tot_num_of_pulled_sdus;
      }
    }
    if (av_ue_sdu_latency_us) {
      float av_ue_sdu_latency_ms = (av_ue_sdu_latency_us / ue_aggr_rlc_metrics.size()) / 100; // Unit is 0.1 ms.
      av_ue_sdu_latency_ms       = std::round(av_ue_sdu_latency_ms * 10.0f) / 10.0f;
      meas_record_item.set_real();
      meas_record_item.real().value = av_ue_sdu_latency_ms;
      items.push_back(meas_record_item);
      meas_collected = true;
    } else {
      meas_record_item.set_no_value();
      items.push_back(meas_record_item);
      meas_collected = true;
      return meas_collected;
    }
  } else {
    for (auto& ue : ues) {
      meas_record_item_c  meas_record_item;
      gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
      uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
      if (ue_aggr_rlc_metrics.count(ue_idx) == 0) {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
        continue;
      }
      int tot_sdu_latency_us = std::accumulate(
          ue_aggr_rlc_metrics[ue_idx].begin(),
          ue_aggr_rlc_metrics[ue_idx].end(),
          0,
          [](size_t sum, const rlc_metrics& metric) { return sum + metric.tx.tx_low.sum_sdu_latency_us; });
      int tot_num_sdus =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.tx.tx_high.num_sdus; });
      if (tot_sdu_latency_us) {
        float av_ue_sdu_latency_ms = (static_cast<float>(tot_sdu_latency_us) / tot_num_sdus) / 100; // Unit is 0.1 ms.
        av_ue_sdu_latency_ms       = std::round(av_ue_sdu_latency_ms * 10.0f) / 10.0f;
        meas_record_item.set_real();
        meas_record_item.real().value = av_ue_sdu_latency_ms;
        items.push_back(meas_record_item);
        meas_collected = true;
      } else {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
      }
    }
  }
  return meas_collected;
}

bool e2sm_kpm_du_meas_provider_impl::get_drb_ul_rlc_sdu_latency(const asn1::e2sm::label_info_list_l     label_info_list,
                                                                const std::vector<asn1::e2sm::ue_id_c>& ues,
                                                                const std::optional<asn1::e2sm::cgi_c>  cell_global_id,
                                                                std::vector<asn1::e2sm::meas_record_item_c>& items)
{
  bool meas_collected = false;
  if (ue_aggr_rlc_metrics.empty()) {
    return handle_no_meas_data_available(ues, items, asn1::e2sm::meas_record_item_c::types::options::no_value);
  }

  if ((label_info_list.size() > 1 or
       (label_info_list.size() == 1 and not label_info_list[0].meas_label.no_label_present))) {
    logger.debug("Metric: DRB.RlcDelayUl supports only NO_LABEL label.");
    return meas_collected;
  }

  if (ues.empty()) {
    meas_record_item_c meas_record_item;
    float              av_ue_sdu_latency_us = 0;
    for (auto& rlc_metric : ue_aggr_rlc_metrics) {
      int tot_num_sdus = std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.rx.num_sdus;
          });
      int tot_sdu_latency_us = std::accumulate(
          rlc_metric.second.begin(), rlc_metric.second.end(), 0, [](size_t sum, const rlc_metrics& metric) {
            return sum + metric.rx.sdu_latency_us;
          });
      if (tot_num_sdus && tot_sdu_latency_us) {
        av_ue_sdu_latency_us += (float)tot_sdu_latency_us / (float)tot_num_sdus;
      }
    }
    if (av_ue_sdu_latency_us) {
      meas_record_item.set_real();
      meas_record_item.real().value = (av_ue_sdu_latency_us / ue_aggr_rlc_metrics.size()) / 100; // Unit is 0.1ms.
      items.push_back(meas_record_item);
      meas_collected = true;
    } else {
      meas_record_item.set_no_value();
      items.push_back(meas_record_item);
      meas_collected = true;
      return meas_collected;
    }
  } else {
    for (auto& ue : ues) {
      meas_record_item_c  meas_record_item;
      gnb_cu_ue_f1ap_id_t gnb_cu_ue_f1ap_id = int_to_gnb_cu_ue_f1ap_id(ue.gnb_du_ue_id().gnb_cu_ue_f1ap_id);
      uint32_t            ue_idx            = f1ap_ue_id_provider.get_ue_index(gnb_cu_ue_f1ap_id);
      if (ue_aggr_rlc_metrics.count(ue_idx) == 0) {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
        continue;
      }
      int tot_sdu_latency_us =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.rx.sdu_latency_us; });
      int tot_num_sdus =
          std::accumulate(ue_aggr_rlc_metrics[ue_idx].begin(),
                          ue_aggr_rlc_metrics[ue_idx].end(),
                          0,
                          [](size_t sum, const rlc_metrics& metric) { return sum + metric.rx.num_sdus; });
      if (tot_sdu_latency_us) {
        meas_record_item.set_real();
        meas_record_item.real().value = (static_cast<float>(tot_sdu_latency_us) / tot_num_sdus) / 100; // Unit is 0.1ms.
        items.push_back(meas_record_item);
        meas_collected = true;
      } else {
        meas_record_item.set_no_value();
        items.push_back(meas_record_item);
        meas_collected = true;
      }
    }
  }
  return meas_collected;
}
