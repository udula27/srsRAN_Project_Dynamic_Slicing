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

#include "srsran/ran/du_types.h"
#include "srsran/ran/rnti.h"
#include "srsran/ran/s_nssai.h"
#include <vector>

namespace srsran {

/// \brief UE context information for E2 reporting
struct e2_ue_context_info {
  du_ue_index_t              ue_index;
  rnti_t                     crnti;
  std::vector<s_nssai_t>     slices; ///< List of S-NSSAIs this UE is attached to
};

/// \brief Notifier interface for UE context events to E2 layer
/// This interface is implemented by E2 and called by DU manager when UE context changes occur
class e2_du_ue_context_notifier
{
public:
  virtual ~e2_du_ue_context_notifier() = default;

  /// \brief Notify E2 layer about UE context update (creation or modification)
  /// \param[in] ue_ctx UE context information including slice associations
  virtual void on_ue_context_update(const e2_ue_context_info& ue_ctx) = 0;

  /// \brief Notify E2 layer about UE context removal
  /// \param[in] ue_index Index of the UE being removed
  virtual void on_ue_context_release(du_ue_index_t ue_index) = 0;
};

} // namespace srsran
