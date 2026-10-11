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

#include "e2sm_rc_asn1_packer.h"
#include "e2sm_rc_control_service_impl.h"

using namespace asn1::e2ap;
using namespace asn1::e2sm;
using namespace srsran;

const std::string e2sm_rc_asn1_packer::short_name       = "ORAN-E2SM-RC";
const std::string e2sm_rc_asn1_packer::oid              = "1.3.6.1.4.1.53148.1.1.2.3";
const std::string e2sm_rc_asn1_packer::func_description = "RAN Control";
const uint32_t    e2sm_rc_asn1_packer::ran_func_id      = 3;
const uint32_t    e2sm_rc_asn1_packer::revision         = 0;

e2sm_rc_asn1_packer::e2sm_rc_asn1_packer() {}

bool e2sm_rc_asn1_packer::add_e2sm_control_service(e2sm_control_service* control_service)
{
  control_services.emplace(control_service->get_style_type(), control_service);
  return true;
}

e2sm_action_definition
e2sm_rc_asn1_packer::handle_packed_e2sm_action_definition(const srsran::byte_buffer& action_definition)
{
  e2sm_action_definition action_def;
  action_def.service_model = e2sm_service_model_t::RC;

  // Handle empty action definition (use defaults for Style 4)
  if (action_definition.empty()) {
    fmt::print("E2SM-RC: Empty action definition - using defaults for Style 4\n");
    // Create default Style 4 action definition
    auto& rc_action_def = std::get<asn1::e2sm::e2sm_rc_action_definition_s>(action_def.action_definition);
    rc_action_def.ric_style_type = 4;  // Style 4: UE Information
    return action_def;
  }

  asn1::cbit_ref bref(action_definition);
  if (std::get<asn1::e2sm::e2sm_rc_action_definition_s>(action_def.action_definition).unpack(bref) !=
      asn1::SRSASN_SUCCESS) {
    fmt::println("Failed to unpack E2SM RC Action Definition - using defaults for Style 4");
    // On failure, try to use defaults for Style 4
    auto& rc_action_def = std::get<asn1::e2sm::e2sm_rc_action_definition_s>(action_def.action_definition);
    rc_action_def.ric_style_type = 4;
  }
  return action_def;
}

e2sm_ric_control_request
e2sm_rc_asn1_packer::handle_packed_ric_control_request(const asn1::e2ap::ric_ctrl_request_s& req)
{
  e2sm_ric_control_request ric_control_request     = {};
  ric_control_request.service_model                = e2sm_service_model_t::RC;
  ric_control_request.ric_call_process_id_present  = req->ric_call_process_id_present;
  ric_control_request.ric_ctrl_ack_request_present = req->ric_ctrl_ack_request_present;

  if (ric_control_request.ric_call_process_id_present) {
    ric_control_request.ric_call_process_id = req->ric_call_process_id.to_number();
  }

  asn1::cbit_ref bref_hdr(req->ric_ctrl_hdr);
  asn1::cbit_ref bref_msg(req->ric_ctrl_msg);
  if (std::get<asn1::e2sm::e2sm_rc_ctrl_hdr_s>(ric_control_request.request_ctrl_hdr).unpack(bref_hdr) !=
      asn1::SRSASN_SUCCESS) {
    fmt::println("Failed to unpack E2SM RC Control Request Header");
  }

  if (std::get<asn1::e2sm::e2sm_rc_ctrl_msg_s>(ric_control_request.request_ctrl_msg).unpack(bref_msg) !=
      asn1::SRSASN_SUCCESS) {
    fmt::println("Failed to unpack E2SM RC Control Request Message");
  }

  if (ric_control_request.ric_ctrl_ack_request_present) {
    ric_control_request.ric_ctrl_ack_request =
        req->ric_ctrl_ack_request.value == asn1::e2ap::ric_ctrl_ack_request_e::ack;
  }
  return ric_control_request;
}

e2_ric_control_response e2sm_rc_asn1_packer::pack_ric_control_response(const e2sm_ric_control_response& e2sm_response)
{
  e2_ric_control_response e2_control_response = {};

  e2_control_response.success = e2sm_response.success;

  if (e2_control_response.success) {
    if (e2sm_response.ric_ctrl_outcome_present) {
      e2_control_response.ack->ric_ctrl_outcome_present = true;
      srsran::byte_buffer buf;
      asn1::bit_ref       bref(buf);
      if (std::get<e2sm_rc_ctrl_outcome_s>(e2sm_response.ric_ctrl_outcome).pack(bref) != asn1::SRSASN_SUCCESS) {
        fmt::println("Failed to pack E2SM RC RIC Control Outcome (Ack)");
      }
      if (!e2_control_response.ack->ric_ctrl_outcome.resize(buf.length())) {
        fmt::println("Failed to resize E2SM RC RIC Control Outcome (Ack)");
        return {};
      }
      std::copy(buf.begin(), buf.end(), e2_control_response.ack->ric_ctrl_outcome.begin());
    }
  } else {
    if (e2sm_response.ric_ctrl_outcome_present) {
      e2_control_response.failure->ric_ctrl_outcome_present = true;
      srsran::byte_buffer buf;
      asn1::bit_ref       bref(buf);
      if (std::get<e2sm_rc_ctrl_outcome_s>(e2sm_response.ric_ctrl_outcome).pack(bref) != asn1::SRSASN_SUCCESS) {
        fmt::println("Failed to pack E2SM RC RIC Control Outcome (Failure)");
      }
      if (!e2_control_response.failure->ric_ctrl_outcome.resize(buf.length())) {
        fmt::println("Failed to resize E2SM RC RIC Control Outcome (Failure)");
        return {};
      }
      std::copy(buf.begin(), buf.end(), e2_control_response.failure->ric_ctrl_outcome.begin());
    }
    e2_control_response.failure->cause = e2sm_response.cause;
  }

  return e2_control_response;
}

e2sm_event_trigger_definition
e2sm_rc_asn1_packer::handle_packed_event_trigger_definition(const srsran::byte_buffer& event_trigger_definition)
{
  e2sm_event_trigger_definition e2sm_event_trigger_def;
  e2sm_event_trigger_def.ric_service_type = e2sm_event_trigger_definition::e2sm_ric_service_type_t::REPORT;
  e2sm_event_trigger_def.report_period = 0;  // 0 = event-triggered mode (no periodic timer)

  // Handle empty event trigger (use event-triggered mode for RC Style 4)
  if (event_trigger_definition.empty()) {
    fmt::print("E2SM-RC: Empty event trigger - using event-triggered mode (report_period=0)\n");
    return e2sm_event_trigger_def;
  }

  e2sm_rc_event_trigger_s e2sm_rc_event_trigger;
  asn1::cbit_ref          bref(event_trigger_definition);

  if (e2sm_rc_event_trigger.unpack(bref) != asn1::SRSASN_SUCCESS) {
    fmt::println("Failed to unpack E2SM-RC Event Trigger Definition - using event-triggered mode");
    return e2sm_event_trigger_def;
  }

  // E2SM-RC is event-driven, not periodic
  // report_period = 0 indicates event-triggered mode
  return e2sm_event_trigger_def;
}

asn1::unbounded_octstring<true> e2sm_rc_asn1_packer::pack_ran_function_description()
{
  e2sm_rc_ran_function_definition_s ran_function_desc;
  ran_function_desc.ran_function_definition_report_present        = true;
  ran_function_desc.ran_function_definition_policy_present        = false;
  ran_function_desc.ran_function_definition_insert_present        = false;
  ran_function_desc.ran_function_definition_event_trigger_present = false;
  ran_function_desc.ran_function_definition_ctrl_present          = false;

  ran_function_desc.ran_function_name.ran_function_short_name.resize(short_name.size());
  ran_function_desc.ran_function_name.ran_function_e2sm_o_id.resize(oid.size());
  ran_function_desc.ran_function_name.ran_function_description.resize(func_description.size());

  ran_function_desc.ran_function_name.ran_function_short_name.from_string(short_name.c_str());
  ran_function_desc.ran_function_name.ran_function_e2sm_o_id.from_string(oid.c_str());
  ran_function_desc.ran_function_name.ran_function_description.from_string(func_description.c_str());
  ran_function_desc.ext = false;

  // Add Report Style 4 (UE Information)
  ran_function_definition_report_item_s report_style_4;
  report_style_4.ric_report_style_type = 4;
  report_style_4.ric_report_style_name.from_string("UE Information");
  report_style_4.ric_supported_event_trigger_style_type = 1;  // Event-triggered
  report_style_4.ric_report_action_format_type          = 4;  // Format 4
  report_style_4.ric_ind_hdr_format_type                = 1;  // Format 1
  report_style_4.ric_ind_msg_format_type                = 2;  // Format 2 (UE-level)

  // Add RAN Parameter: S-NSSAI
  report_ran_param_item_s s_nssai_param;
  s_nssai_param.ran_param_id = 1;
  s_nssai_param.ran_param_name.from_string("S-NSSAI");
  report_style_4.ran_report_params_list.push_back(s_nssai_param);

  ran_function_desc.ran_function_definition_report.ric_report_style_list.push_back(report_style_4);


  // Add Control Styles
  for (auto const& x : control_services) {
    ran_function_desc.ran_function_definition_ctrl_present = true;
    e2sm_control_service*               control_service    = x.second;
    ran_function_definition_ctrl_item_s ran_function_definition_ctrl_item =
        control_service->get_control_style_definition();
    ran_function_desc.ran_function_definition_ctrl.ric_ctrl_style_list.push_back(ran_function_definition_ctrl_item);
  }
  asn1::unbounded_octstring<true> ran_function_description;

  srsran::byte_buffer buf;
  asn1::bit_ref       bref(buf);
  if (ran_function_desc.pack(bref) != asn1::SRSASN_SUCCESS) {
    fmt::println("Failed to pack E2SM RC RAN Function Description");
    return ran_function_description;
  }

  if (!ran_function_description.resize(buf.length())) {
    fmt::println("Failed to resize E2SM RC RAN Function Description");
    return ran_function_description;
  }
  std::copy(buf.begin(), buf.end(), ran_function_description.begin());
  return ran_function_description;
}
