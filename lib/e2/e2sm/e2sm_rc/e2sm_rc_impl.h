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

#include "srsran/asn1/asn1_utils.h"
#include "srsran/asn1/e2sm/e2sm_rc_ies.h"
#include "srsran/e2/e2.h"
#include "srsran/e2/e2sm/e2sm.h"
#include "srsran/e2/e2_du_ue_context_notifier.h"
#include "srsran/f1ap/du/f1ap_du.h"
#include <map>
#include <vector>
#include <mutex>

namespace srsran {

// Forward declaration
class e2sm_rc_report_service_style4;

namespace srs_du {
class f1ap_ue_id_translator;
}

class e2sm_rc_impl : public e2sm_interface, public e2_du_ue_context_notifier
{
public:
  e2sm_rc_impl(srslog::basic_logger& logger_, e2sm_handler& e2sm_packer_, srs_du::f1ap_ue_id_translator& f1ap_ue_id_provider_);

  e2sm_handler& get_e2sm_packer() override;

  bool action_supported(const asn1::e2ap::ric_action_to_be_setup_item_s& ric_action) override;

  std::unique_ptr<e2sm_report_service> get_e2sm_report_service(const srsran::byte_buffer& action_definition) override;
  e2sm_control_service*                get_e2sm_control_service(const e2sm_ric_control_request& request) override;

  bool add_e2sm_control_service(std::unique_ptr<e2sm_control_service> control_service) override;

  // e2_du_ue_context_notifier interface
  void on_ue_context_update(const e2_ue_context_info& ue_ctx) override;
  void on_ue_context_release(du_ue_index_t ue_index) override;

  // Service registration for instance tracking
  void register_service(e2sm_rc_report_service_style4* service);
  void unregister_service(e2sm_rc_report_service_style4* service);

private:
  srslog::basic_logger&                                     logger;
  e2sm_handler&                                             e2sm_packer;
  srs_du::f1ap_ue_id_translator&                            f1ap_ue_id_provider;
  std::map<uint32_t, std::unique_ptr<e2sm_control_service>> control_services;

  // Track all active report service instances to forward UE context updates
  std::vector<e2sm_rc_report_service_style4*>               active_report_services;
  std::mutex                                                report_services_mutex;
};
} // namespace srsran
