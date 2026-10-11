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

#include "srsran/e2/e2_du_ue_context_notifier.h"
#include <memory>

namespace srsran {

/// \brief Global registry for E2SM-RC Report Service Style 4 instances.
/// This allows the DU manager to connect to RC Report Service instances
/// created during subscription without tight coupling between E2 and DU layers.
class e2_rc_report_service_registry
{
public:
  /// Get the singleton instance
  static e2_rc_report_service_registry& get_instance();

  /// Register a UE context notifier (called when RC Report Style 4 subscription is created)
  void register_ue_context_notifier(e2_du_ue_context_notifier* notifier);

  /// Unregister a UE context notifier (called when subscription is deleted)
  void unregister_ue_context_notifier(e2_du_ue_context_notifier* notifier);

  /// Get the current UE context notifier (called by DU manager)
  e2_du_ue_context_notifier* get_ue_context_notifier();

private:
  e2_rc_report_service_registry()  = default;
  ~e2_rc_report_service_registry() = default;

  // Non-copyable
  e2_rc_report_service_registry(const e2_rc_report_service_registry&) = delete;
  e2_rc_report_service_registry& operator=(const e2_rc_report_service_registry&) = delete;

  e2_du_ue_context_notifier* notifier = nullptr;
};

} // namespace srsran
