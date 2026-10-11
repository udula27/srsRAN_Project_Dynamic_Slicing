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

#pragma once

#include "srsran/ran/resource_allocation/rb_interval.h"
#include "srsran/ran/rrm.h"
#include "srsran/scheduler/config/scheduler_expert_config.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace srsran {

// Forward declare to avoid pulling in srslog headers
class slice_resource_logger;

/// PHASE 1: Slice-level PRB policy ratios (percentage-based, 3GPP TS 28.541 compliant)
/// This struct stores the raw percentage ratios (0-100) for slice resource allocation.
struct slice_prb_policy {
  uint8_t min_ratio = 0;        ///< Minimum PRB ratio (0-100%). Guaranteed allocation for this slice.
  uint8_t max_ratio = 100;      ///< Maximum PRB ratio (0-100%). Slice cannot exceed this limit.
  uint8_t dedicated_ratio = 0;  ///< Dedicated PRB ratio (0-100%). Reserved exclusively for this slice.

  /// Default constructor: no guarantees, full access, no dedicated resources
  slice_prb_policy() = default;

  /// Parameterized constructor with validation
  slice_prb_policy(uint8_t min, uint8_t max, uint8_t ded) : min_ratio(min), max_ratio(max), dedicated_ratio(ded)
  {
    srsran_assert(min <= max, "Invalid policy: min_ratio {} > max_ratio {}", min, max);
    srsran_assert(ded <= max, "Invalid policy: dedicated_ratio {} > max_ratio {}", ded, max);
  }
};

/// Cell-specific Default RAN slice configuration.
struct slice_rrm_policy_config {
  /// Maximum RAN scheduling policy.
  static constexpr unsigned max_priority = 255;

  /// RRM Policy identifier.
  rrm_policy_member rrc_member;
  /// \brief Radio resources, in RBs, allocated to this group.
  ///
  /// These limits are computed based on the rrmPolicyRatios provided for the RRM policy and based on the number of RBs
  /// of the cell.
  rrm_policy_ratio_rb_limits rbs;
  /// RAN slice scheduling priority. Values: {0, ..., 255}.
  unsigned priority = 0;
  /// Policy scheduler configuration for the slice.
  scheduler_policy_config policy_sched_cfg = time_qos_scheduler_config{};
};

/// Runtime slice PRB accounting for a single cell.
/// Thread-safe for concurrent access from E2 handler and scheduler.
/// This class implements slice-level PRB quota enforcement according to 3GPP TS 28.541.
class cell_slice_resource_manager {
public:
  cell_slice_resource_manager(unsigned total_prbs, const char* logger_prefix = "SCHED");
  ~cell_slice_resource_manager();  // Need destructor for unique_ptr with incomplete type

  /// Update slice configuration from E2 control handler.
  /// Returns true if successful, false if invalid (e.g., sum(min_ratio) > 100).
  bool update_slice_policy(const rrm_policy_member& slice_id,
                           unsigned min_ratio,
                           unsigned max_ratio,
                           unsigned ded_ratio);

  /// Get PRB limits for a slice (thread-safe).
  std::optional<rrm_policy_ratio_rb_limits> get_slice_limits(const s_nssai_t& snssai) const;

  /// Get PRB limits for all configured slices (thread-safe snapshot).
  std::map<s_nssai_t, rrm_policy_ratio_rb_limits> get_all_slice_limits() const;

  /// Validate that sum(min_ratio) <= 100.
  bool validate_ratios() const;

  /// Get total configured slices.
  unsigned get_num_slices() const;

  /// Monotonic policy revision incremented on every successful policy update.
  uint64_t get_policy_revision() const { return policy_revision.load(std::memory_order_relaxed); }

  /// Get total cell PRBs.
  unsigned get_total_cell_prbs() const { return total_cell_prbs; }

  /// PHASE 1: Get PRB policy ratios for a slice (thread-safe).
  /// Returns the percentage-based policy if configured.
  std::optional<slice_prb_policy> get_slice_prb_policy(const s_nssai_t& snssai) const;

  /// PHASE 1: Dump all configured slice PRB policies to log (for debugging).
  /// Logs SST, SD, min_ratio, max_ratio, dedicated_ratio for all slices.
  void dump_all_slice_prb_policies() const;

private:
  mutable std::mutex mtx;
  unsigned total_cell_prbs;
  std::unique_ptr<slice_resource_logger> logger_impl;

  // Maps S-NSSAI -> RB limits (computed from ratios)
  std::map<s_nssai_t, rrm_policy_ratio_rb_limits> slice_limits;

  // PHASE 1: Maps S-NSSAI -> PRB policy ratios (raw percentages)
  std::map<s_nssai_t, slice_prb_policy> slice_policies;

  // Cache for quick validation
  std::atomic<unsigned> sum_min_ratios{0};

  // Policy revision used by scheduler to detect runtime updates.
  std::atomic<uint64_t> policy_revision{0};

  // Helper for logging
  void log_info(const char* fmt, ...) const;
  void log_error(const char* fmt, ...) const;
  void log_debug(const char* fmt, ...) const;
  void log_warning(const char* fmt, ...) const;
};

} // namespace srsran
