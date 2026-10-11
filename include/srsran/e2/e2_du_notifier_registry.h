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

#include "e2_du_ue_context_notifier.h"
#include <memory>
#include <mutex>

namespace srsran {

/// \brief Global registry for E2 DU UE context notifiers
/// This allows the E2 agent to register notifier instances that the DU manager can access
class e2_du_notifier_registry
{
public:
  /// \brief Get the singleton instance of the registry
  static e2_du_notifier_registry& get_instance()
  {
    static e2_du_notifier_registry instance;
    return instance;
  }

  /// \brief Register a UE context notifier (typically called when RC Report Style 4 subscription is created)
  /// \param[in] notifier Pointer to the notifier instance
  void register_ue_context_notifier(e2_du_ue_context_notifier* notifier)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ue_context_notifier_ = notifier;
  }

  /// \brief Unregister the UE context notifier (typically called when subscription is deleted)
  void unregister_ue_context_notifier()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ue_context_notifier_ = nullptr;
  }

  /// \brief Get the registered UE context notifier
  /// \return Pointer to the notifier, or nullptr if none is registered
  e2_du_ue_context_notifier* get_ue_context_notifier()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return ue_context_notifier_;
  }

private:
  e2_du_notifier_registry() = default;
  ~e2_du_notifier_registry() = default;

  // Delete copy/move constructors and assignment operators
  e2_du_notifier_registry(const e2_du_notifier_registry&) = delete;
  e2_du_notifier_registry& operator=(const e2_du_notifier_registry&) = delete;
  e2_du_notifier_registry(e2_du_notifier_registry&&) = delete;
  e2_du_notifier_registry& operator=(e2_du_notifier_registry&&) = delete;

  std::mutex                   mutex_;
  e2_du_ue_context_notifier*   ue_context_notifier_ = nullptr;
};

} // namespace srsran
