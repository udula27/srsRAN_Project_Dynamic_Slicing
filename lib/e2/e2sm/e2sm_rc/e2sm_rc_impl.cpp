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

#include "e2sm_rc_impl.h"
#include "e2sm_rc_control_service_impl.h"
#include "e2sm_rc_report_service_impl.h"
#include "srsran/asn1/asn1_utils.h"
#include "srsran/asn1/e2sm/e2sm_rc_ies.h"
#include "srsran/e2/e2sm/e2sm.h"
#include "srsran/e2/e2_du_notifier_registry.h"
#include "fmt/format.h"

using namespace asn1::e2ap;
using namespace asn1::e2sm;
using namespace srsran;

e2sm_rc_impl::e2sm_rc_impl(srslog::basic_logger& logger_, e2sm_handler& e2sm_packer_, srs_du::f1ap_ue_id_translator& f1ap_ue_id_provider_) :
  logger(logger_), e2sm_packer(e2sm_packer_), f1ap_ue_id_provider(f1ap_ue_id_provider_)
{
  // Register this instance with the global E2 DU notifier registry
  // This allows DU manager to find and call our UE context notifier
  e2_du_notifier_registry::get_instance().register_ue_context_notifier(this);
  logger.info("E2SM-RC: Registered UE context notifier - will forward to all report services");
}

bool e2sm_rc_impl::action_supported(const ric_action_to_be_setup_item_s& ric_action)
{
  fmt::print("========================================\n");
  fmt::print("E2SM-RC: Checking action support\n");
  fmt::print("Action type: {}\n", ric_action.ric_action_type.to_string());

  // Check if Report Style 4 (UE Information) is supported
  if (ric_action.ric_action_type.value == ric_action_type_e::report) {

    // Handle empty action definition (default to Style 4 for testing)
    if (ric_action.ric_action_definition.size() == 0) {
      fmt::print("Empty action definition - defaulting to Style 4 for testing\n");
      fmt::print("✅ RC Report Style 4 (UE Information) - SUPPORTED (default)\n");
      fmt::print("========================================\n");
      return true;
    }

    // Try to unpack if not empty
    e2sm_rc_action_definition_s action_def;
    asn1::cbit_ref              bref(ric_action.ric_action_definition);
    if (action_def.unpack(bref) == asn1::SRSASN_SUCCESS) {
      fmt::print("Requested Style Type: {}\n", action_def.ric_style_type);
      if (action_def.ric_style_type == 4) {
        fmt::print("✅ RC Report Style 4 (UE Information) - SUPPORTED\n");
        fmt::print("========================================\n");
        return true;
      } else {
        fmt::print("❌ Style {} not supported (only Style 4 implemented)\n", action_def.ric_style_type);
      }
    } else {
      fmt::print("❌ Failed to unpack action definition (invalid ASN.1)\n");
      fmt::print("   For testing, accepting anyway (defaulting to Style 4)\n");
      fmt::print("========================================\n");
      return true;  // Accept anyway for testing
    }
  } else {
    fmt::print("❌ Action type not 'report'\n");
  }
  fmt::print("========================================\n");
  return false;
}

e2sm_handler& e2sm_rc_impl::get_e2sm_packer()
{
  return e2sm_packer;
}

// Helper class that auto-registers/unregisters with e2sm_rc_impl
class e2sm_rc_report_service_style4_managed : public e2sm_rc_report_service_style4 {
public:
  e2sm_rc_report_service_style4_managed(e2sm_rc_action_definition_s action_def_,
                                        srs_du::f1ap_ue_id_translator& f1ap_ue_id_provider_,
                                        e2sm_rc_impl* parent_)
    : e2sm_rc_report_service_style4(std::move(action_def_), f1ap_ue_id_provider_), parent(parent_) {
    // Register with parent
    parent->register_service(this);
  }

  ~e2sm_rc_report_service_style4_managed() override {
    // Unregister from parent
    parent->unregister_service(this);
  }

private:
  e2sm_rc_impl* parent;
};

std::unique_ptr<e2sm_report_service> e2sm_rc_impl::get_e2sm_report_service(const srsran::byte_buffer& action_definition)
{
  fmt::print("========================================\n");
  fmt::print("E2SM-RC: get_e2sm_report_service() called\n");
  fmt::print("  Creating new Style 4 report service instance\n");
  fmt::print("========================================\n");

  // Create a new instance that auto-registers with this e2sm_rc_impl
  e2sm_rc_action_definition_s default_action_def;
  default_action_def.ric_style_type = 4;

  return std::make_unique<e2sm_rc_report_service_style4_managed>(
      std::move(default_action_def), f1ap_ue_id_provider, this);
}

bool e2sm_rc_impl::add_e2sm_control_service(std::unique_ptr<e2sm_control_service> control_service)
{
  control_services.emplace(control_service->get_style_type(), std::move(control_service));
  return true;
}

e2sm_control_service* e2sm_rc_impl::get_e2sm_control_service(const e2sm_ric_control_request& request)
{
  const e2sm_rc_ctrl_hdr_s& ctrl_hdr = std::get<e2sm_rc_ctrl_hdr_s>(request.request_ctrl_hdr);

  int64_t ric_style_type = 0;
  if (ctrl_hdr.ric_ctrl_hdr_formats.type().value ==
      e2sm_rc_ctrl_hdr_s::ric_ctrl_hdr_formats_c_::types_opts::ctrl_hdr_format1) {
    e2sm_rc_ctrl_hdr_format1_s ctrl_hdr_format1 = ctrl_hdr.ric_ctrl_hdr_formats.ctrl_hdr_format1();
    ric_style_type                              = ctrl_hdr_format1.ric_style_type;
  } else if (ctrl_hdr.ric_ctrl_hdr_formats.type().value ==
             e2sm_rc_ctrl_hdr_s::ric_ctrl_hdr_formats_c_::types_opts::ctrl_hdr_format2) {
    ric_style_type = 255;
  } else if (ctrl_hdr.ric_ctrl_hdr_formats.type().value ==
             e2sm_rc_ctrl_hdr_s::ric_ctrl_hdr_formats_c_::types_opts::ctrl_hdr_format3) {
    e2sm_rc_ctrl_hdr_format3_s ctrl_hdr_format3 = ctrl_hdr.ric_ctrl_hdr_formats.ctrl_hdr_format3();
    ric_style_type                              = ctrl_hdr_format3.ric_style_type;
  } else {
    logger.error("RIC control header format not supported");
    return nullptr;
  }

  if (control_services.find(ric_style_type) != control_services.end()) {
    return control_services.at(ric_style_type).get();
  }
  return nullptr;
}

void e2sm_rc_impl::on_ue_context_update(const e2_ue_context_info& ue_ctx)
{
  // DEBUG: Commented out for cleaner logs during Phase 1 validation
  // fmt::print("========================================\n");
  // fmt::print("E2SM-RC IMPL: UE context update received\n");
  // fmt::print("  UE Index: {}, C-RNTI: {:#x}\n", ue_ctx.ue_index, to_value(ue_ctx.crnti));
  // fmt::print("  Slices: {}\n", ue_ctx.slices.size());

  // Forward to ALL active report services
  std::lock_guard<std::mutex> lock(report_services_mutex);
  // fmt::print("  Forwarding to {} active report service(s)\n", active_report_services.size());

  for (auto* service : active_report_services) {
    if (service) {
      service->on_ue_context_update(ue_ctx);
    }
  }
  // fmt::print("========================================\n");
}

void e2sm_rc_impl::on_ue_context_release(du_ue_index_t ue_index)
{
  // DEBUG: Commented out for cleaner logs during Phase 1 validation
  // fmt::print("E2SM-RC IMPL: UE context release - UE Index: {}\n", ue_index);

  // Forward to ALL active report services
  std::lock_guard<std::mutex> lock(report_services_mutex);
  for (auto* service : active_report_services) {
    if (service) {
      service->on_ue_context_release(ue_index);
    }
  }
}

void e2sm_rc_impl::register_service(e2sm_rc_report_service_style4* service)
{
  std::lock_guard<std::mutex> lock(report_services_mutex);
  active_report_services.push_back(service);
  fmt::print("E2SM-RC IMPL: Registered report service. Total active: {}\n", active_report_services.size());
}

void e2sm_rc_impl::unregister_service(e2sm_rc_report_service_style4* service)
{
  std::lock_guard<std::mutex> lock(report_services_mutex);
  auto it = std::find(active_report_services.begin(), active_report_services.end(), service);
  if (it != active_report_services.end()) {
    active_report_services.erase(it);
    fmt::print("E2SM-RC IMPL: Unregistered report service. Total active: {}\n", active_report_services.size());
  }
}
