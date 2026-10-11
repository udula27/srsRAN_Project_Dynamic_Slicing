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

#include "srsran/scheduler/config/slice_rrm_policy_config.h"
#include "srsran/srslog/srslog.h"
#include <cstdarg>
#include <cstdio>

using namespace srsran;

// Internal logger wrapper to avoid including srslog in header
class srsran::slice_resource_logger {
public:
  explicit slice_resource_logger(const char* logger_name) : logger(srslog::fetch_basic_logger(logger_name)) {}

  srslog::basic_logger& get_logger() { return logger; }

private:
  srslog::basic_logger& logger;
};

// Logging helpers using printf-style formatting
void cell_slice_resource_manager::log_info(const char* fmt, ...) const {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logger_impl->get_logger().info("{}", buffer);
}

void cell_slice_resource_manager::log_error(const char* fmt, ...) const {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logger_impl->get_logger().error("{}", buffer);
}

void cell_slice_resource_manager::log_debug(const char* fmt, ...) const {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logger_impl->get_logger().debug("{}", buffer);
}

void cell_slice_resource_manager::log_warning(const char* fmt, ...) const {
  char buffer[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logger_impl->get_logger().warning("{}", buffer);
}

cell_slice_resource_manager::cell_slice_resource_manager(unsigned total_prbs, const char* logger_prefix) :
  total_cell_prbs(total_prbs), logger_impl(std::make_unique<slice_resource_logger>(logger_prefix))
{
  log_info("[SLICE-PRB-PH1] Slice resource manager initialized with %u total PRBs", total_cell_prbs);
  log_info("[SLICE-PRB-PH1] Infrastructure ready for slice-level PRB policy enforcement");
}

cell_slice_resource_manager::~cell_slice_resource_manager() = default;

bool cell_slice_resource_manager::update_slice_policy(
    const rrm_policy_member& slice_id,
    unsigned min_ratio,
    unsigned max_ratio,
    unsigned ded_ratio)
{
  std::lock_guard<std::mutex> lock(mtx);

  log_info("[SLICE-PRB-PH1] ========================================");
  log_info("[SLICE-PRB-PH1] Slice PRB policy update requested");
  log_info("[SLICE-PRB-PH1] Slice ID: PLMN=%s SST=%u SD=%u",
           slice_id.plmn_id.to_string().c_str(),
           slice_id.s_nssai.sst.value(),
           slice_id.s_nssai.sd.is_set() ? slice_id.s_nssai.sd.value() : 0);
  log_info("[SLICE-PRB-PH1] Incoming ratios: min=%u%% max=%u%% dedicated=%u%%",
           min_ratio, max_ratio, ded_ratio);

  // PHASE 1 VALIDATION RULE 1: min_ratio <= max_ratio
  if (min_ratio > max_ratio) {
    log_error("[SLICE-PRB-PH1] VALIDATION FAILED: min_ratio (%u%%) > max_ratio (%u%%)", min_ratio, max_ratio);
    log_error("[SLICE-PRB-PH1] Policy update REJECTED");
    log_info("[SLICE-PRB-PH1] ========================================");
    return false;
  }

  // PHASE 1 VALIDATION RULE 2: max_ratio <= 100
  if (max_ratio > 100) {
    log_error("[SLICE-PRB-PH1] VALIDATION FAILED: max_ratio (%u%%) > 100%%", max_ratio);
    log_error("[SLICE-PRB-PH1] Policy update REJECTED");
    log_info("[SLICE-PRB-PH1] ========================================");
    return false;
  }

  // PHASE 1 VALIDATION RULE 3: dedicated_ratio <= max_ratio
  if (ded_ratio > max_ratio) {
    log_error("[SLICE-PRB-PH1] VALIDATION FAILED: dedicated_ratio (%u%%) > max_ratio (%u%%)", ded_ratio, max_ratio);
    log_error("[SLICE-PRB-PH1] Policy update REJECTED");
    log_info("[SLICE-PRB-PH1] ========================================");
    return false;
  }

  // Compute RB limits from ratios
  unsigned ded_rbs = (ded_ratio * total_cell_prbs) / 100;
  unsigned min_rbs = (min_ratio * total_cell_prbs) / 100;
  unsigned max_rbs = (max_ratio * total_cell_prbs) / 100;

  log_info("[SLICE-PRB-PH1] Computed PRB limits: ded=%u PRBs, min=%u PRBs, max=%u PRBs",
           ded_rbs, min_rbs, max_rbs);

  // Get current total min sum BEFORE update
  unsigned old_sum = sum_min_ratios.load();
  log_info("[SLICE-PRB-PH1] Current total min sum (before update) = %u%%", old_sum);

  // Store old values for potential rollback
  auto old_limits_it = slice_limits.find(slice_id.s_nssai);
  auto old_policy_it = slice_policies.find(slice_id.s_nssai);
  std::optional<rrm_policy_ratio_rb_limits> old_limits;
  std::optional<slice_prb_policy> old_policy;

  if (old_limits_it != slice_limits.end()) {
    old_limits = old_limits_it->second;
    old_policy = old_policy_it->second;
    log_info("[SLICE-PRB-PH1] Updating existing configuration for SST=%u", slice_id.s_nssai.sst.value());
    log_info("[SLICE-PRB-PH1] Previous ratios: min=%u%% max=%u%% ded=%u%%",
             old_policy->min_ratio, old_policy->max_ratio, old_policy->dedicated_ratio);
  } else {
    log_info("[SLICE-PRB-PH1] No existing configuration found - this is a new slice");
  }

  // Update slice limits (computed PRBs)
  slice_limits[slice_id.s_nssai] = rrm_policy_ratio_rb_limits(ded_rbs, min_rbs, max_rbs);

  // PHASE 1: Store raw ratios in slice_policies map
  slice_policies[slice_id.s_nssai] = slice_prb_policy(min_ratio, max_ratio, ded_ratio);

  // PHASE 1 VALIDATION RULE 4: Recompute sum of min ratios
  unsigned new_sum = 0;
  log_debug("[SLICE-PRB-PH1] Recalculating total minimum ratio sum across all slices:");
  for (const auto& [snssai, policy] : slice_policies) {
    new_sum += policy.min_ratio;
    log_debug("[SLICE-PRB-PH1]   - SST=%u SD=%u contributes min=%u%%",
              snssai.sst.value(),
              snssai.sd.is_set() ? snssai.sd.value() : 0,
              policy.min_ratio);
  }

  log_info("[SLICE-PRB-PH1] New total min sum (after update) = %u%%", new_sum);

  // PHASE 1 VALIDATION RULE 4: Sum of all slice min_ratio <= 100
  if (new_sum > 100) {
    log_error("[SLICE-PRB-PH1] VALIDATION FAILED: sum of min ratios (%u%%) exceeds 100%%", new_sum);
    log_error("[SLICE-PRB-PH1] This would cause over-subscription of guaranteed PRB resources");
    log_info("[SLICE-PRB-PH1] Rolling back to previous configuration...");

    if (old_limits.has_value()) {
      // Restore old values
      slice_limits[slice_id.s_nssai] = old_limits.value();
      slice_policies[slice_id.s_nssai] = old_policy.value();
      log_info("[SLICE-PRB-PH1] Restored previous configuration for SST=%u", slice_id.s_nssai.sst.value());
    } else {
      // Remove newly added entries
      slice_limits.erase(slice_id.s_nssai);
      slice_policies.erase(slice_id.s_nssai);
      log_info("[SLICE-PRB-PH1] Removed invalid configuration for SST=%u", slice_id.s_nssai.sst.value());
    }

    log_error("[SLICE-PRB-PH1] Policy update REJECTED");
    log_info("[SLICE-PRB-PH1] ========================================");
    return false;
  }

  // Update atomic cache
  sum_min_ratios.store(new_sum);
  policy_revision.fetch_add(1, std::memory_order_relaxed);

  // Success!
  log_info("[SLICE-PRB-PH1] ✓ VALIDATION PASSED");
  log_info("[SLICE-PRB-PH1] ✓ Policy successfully configured for SST=%u SD=%u",
           slice_id.s_nssai.sst.value(),
           slice_id.s_nssai.sd.is_set() ? slice_id.s_nssai.sd.value() : 0);
  log_info("[SLICE-PRB-PH1] Total configured slices: %lu", slice_policies.size());
  log_info("[SLICE-PRB-PH1] Aggregate minimum commitment: %u%% (%u PRBs)",
           new_sum, (new_sum * total_cell_prbs) / 100);
  log_info("[SLICE-PRB-PH1] ========================================");

  return true;
}

std::optional<rrm_policy_ratio_rb_limits>
cell_slice_resource_manager::get_slice_limits(const s_nssai_t& snssai) const
{
  std::lock_guard<std::mutex> lock(mtx);
  auto it = slice_limits.find(snssai);
  if (it != slice_limits.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::map<s_nssai_t, rrm_policy_ratio_rb_limits> cell_slice_resource_manager::get_all_slice_limits() const
{
  std::lock_guard<std::mutex> lock(mtx);
  return slice_limits;
}

bool cell_slice_resource_manager::validate_ratios() const
{
  unsigned current_sum = sum_min_ratios.load();
  bool valid = current_sum <= 100;

  if (!valid) {
    log_warning("[SLICE-PRB-PH1] Validation check failed - sum of min ratios = %u%%", current_sum);
  }

  return valid;
}

unsigned cell_slice_resource_manager::get_num_slices() const
{
  std::lock_guard<std::mutex> lock(mtx);
  return slice_policies.size();
}

std::optional<slice_prb_policy>
cell_slice_resource_manager::get_slice_prb_policy(const s_nssai_t& snssai) const
{
  std::lock_guard<std::mutex> lock(mtx);
  auto it = slice_policies.find(snssai);
  if (it != slice_policies.end()) {
    log_debug("[SLICE-PRB-PH1] Retrieved policy for SST=%u SD=%u: min=%u%% max=%u%% ded=%u%%",
              snssai.sst.value(),
              snssai.sd.is_set() ? snssai.sd.value() : 0,
              it->second.min_ratio,
              it->second.max_ratio,
              it->second.dedicated_ratio);
    return it->second;
  }
  log_debug("[SLICE-PRB-PH1] No policy found for SST=%u SD=%u",
            snssai.sst.value(),
            snssai.sd.is_set() ? snssai.sd.value() : 0);
  return std::nullopt;
}

void cell_slice_resource_manager::dump_all_slice_prb_policies() const
{
  std::lock_guard<std::mutex> lock(mtx);

  log_info("[SLICE-PRB-PH1] ========================================");
  log_info("[SLICE-PRB-PH1] SLICE PRB POLICY DUMP");
  log_info("[SLICE-PRB-PH1] ========================================");
  log_info("[SLICE-PRB-PH1] Cell total PRBs: %u", total_cell_prbs);
  log_info("[SLICE-PRB-PH1] Total configured slices: %lu", slice_policies.size());
  log_info("[SLICE-PRB-PH1] Sum of minimum ratios: %u%%", sum_min_ratios.load());
  log_info("[SLICE-PRB-PH1] ========================================");

  if (slice_policies.empty()) {
    log_info("[SLICE-PRB-PH1] No slice policies configured");
  } else {
    unsigned slice_num = 1;
    for (const auto& [snssai, policy] : slice_policies) {
      // Get the corresponding RB limits
      auto limits_it = slice_limits.find(snssai);

      log_info("[SLICE-PRB-PH1] Slice #%u:", slice_num);
      log_info("[SLICE-PRB-PH1]   SST: %u", snssai.sst.value());
      log_info("[SLICE-PRB-PH1]   SD: %u", snssai.sd.is_set() ? snssai.sd.value() : 0);
      log_info("[SLICE-PRB-PH1]   Min ratio: %u%% (%u PRBs guaranteed)",
               policy.min_ratio,
               limits_it != slice_limits.end() ? limits_it->second.min() : 0);
      log_info("[SLICE-PRB-PH1]   Max ratio: %u%% (%u PRBs maximum)",
               policy.max_ratio,
               limits_it != slice_limits.end() ? limits_it->second.max() : 0);
      log_info("[SLICE-PRB-PH1]   Dedicated ratio: %u%% (%u PRBs reserved)",
               policy.dedicated_ratio,
               limits_it != slice_limits.end() ? limits_it->second.dedicated() : 0);

      if (limits_it != slice_limits.end()) {
        log_info("[SLICE-PRB-PH1]   Prioritized PRBs: %u", limits_it->second.prioritized());
        log_info("[SLICE-PRB-PH1]   Shared PRBs: %u", limits_it->second.shared());
      }

      log_info("[SLICE-PRB-PH1]   ----------------------------------------");
      slice_num++;
    }
  }

  log_info("[SLICE-PRB-PH1] ========================================");
  log_info("[SLICE-PRB-PH1] END OF SLICE PRB POLICY DUMP");
  log_info("[SLICE-PRB-PH1] ========================================");
}
