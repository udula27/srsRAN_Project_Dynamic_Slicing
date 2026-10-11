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

#include "e2_indication_procedure.h"
#include "srsran/asn1/e2ap/e2ap.h"
#include "srsran/support/async/async_timer.h"
#include <iostream>

using namespace srsran;
using namespace asn1::e2ap;

e2_indication_procedure::e2_indication_procedure(e2_message_notifier&    notif_,
                                                 e2_event_manager&       ev_mng_,
                                                 e2_subscription_info_t& subscription_info_,
                                                 srslog::basic_logger&   logger_) :
  notifier(notif_), ev_mng(ev_mng_), subscription(subscription_info_), logger(logger_)
{
}

void e2_indication_procedure::operator()(coro_context<eager_async_task<void>>& ctx)
{
  CORO_BEGIN(ctx);

  logger.info("E2 Indication Procedure: Starting for RIC Requestor ID={}, Instance ID={}, Report Period={}ms",
              subscription.request_id.ric_requestor_id,
              subscription.request_id.ric_instance_id,
              subscription.report_period);

  // Check if any action supports event-triggered reporting (do this once)
  has_event_trigger = false;
  for (const auto& action : subscription.action_list) {
    if (action.report_service && action.report_service->supports_event_trigger()) {
      has_event_trigger = true;
      logger.info("E2 subscription supports event-triggered reporting");
      break;
    }
  }

  while (running) {
    if (!ev_mng.sub_del_reqs.count(
            {subscription.request_id.ric_requestor_id, subscription.request_id.ric_instance_id})) {
      logger.error("No subscription delete request found for RIC request ID (Requestor ID={}, Instance ID={})",
                   subscription.request_id.ric_requestor_id,
                   subscription.request_id.ric_instance_id);
      break;
    }

    // Check event signals before waiting
    event_occurred = false;
    if (has_event_trigger) {
      for (const auto& action : subscription.action_list) {
        if (action.report_service && action.report_service->supports_event_trigger()) {
          if (action.report_service->get_report_event_signal().is_set()) {
            event_occurred = true;
            action.report_service->get_report_event_signal().reset();
            logger.info("E2 indication triggered by event");
            break;
          }
        }
      }
    }

    if (!event_occurred) {
      // Wait for timeout or subscription delete
      transaction_sink.subscribe_to(
          *ev_mng.sub_del_reqs[{subscription.request_id.ric_requestor_id, subscription.request_id.ric_instance_id}].get(),
          (std::chrono::milliseconds)subscription.report_period);
      CORO_AWAIT(transaction_sink);

      if (!transaction_sink.timeout_expired()) {
        logger.info("Subscription deleted");
        running = false;
        continue;
      }

      // After timeout, check if event occurred during wait
      if (has_event_trigger) {
        for (const auto& action : subscription.action_list) {
          if (action.report_service && action.report_service->supports_event_trigger()) {
            if (action.report_service->get_report_event_signal().is_set()) {
              event_occurred = true;
              action.report_service->get_report_event_signal().reset();
              logger.info("E2 indication triggered by event during wait");
              break;
            }
          }
        }
      }

      // Decide whether to send report
      if (has_event_trigger && !event_occurred) {
        // Event-triggered mode: Skip report if no event occurred
        logger.debug("E2 indication: No event occurred, skipping report (event-triggered mode)");
        continue;  // Skip to next iteration
      }

      if (!event_occurred && !has_event_trigger) {
        // Periodic mode - always report on timeout
        logger.info("E2 indication: Timer expired (periodic mode), collecting metrics");
      }
    }
    logger.info("E2 indication: Processing {} actions", subscription.action_list.size());
    for (const auto& action : subscription.action_list) {
      e2_indication_message e2_ind = {};
      //      e2_ind.indication->ric_ind_msg.crit                = asn1::crit_opts::reject;
      e2_ind.request_id                                  = subscription.request_id;
      e2_ind.indication->ran_function_id                 = subscription.ran_function_id;
      e2_ind.indication->ric_action_id                   = action.ric_action_id;
      e2_ind.indication->ric_call_process_id_present     = false;
      e2_ind.indication->ric_ind_sn_present              = false;
      e2_ind.indication->ric_request_id.ric_instance_id  = subscription.request_id.ric_instance_id;
      e2_ind.indication->ric_request_id.ric_requestor_id = subscription.request_id.ric_requestor_id;

      byte_buffer ind_msg_bytes;
      byte_buffer ind_hdr_bytes;
      switch (action.ric_action_type) {
        case ric_action_type_e::ric_action_type_opts::report:
          e2_ind.indication->ric_ind_type.value = ric_ind_type_e::ric_ind_type_opts::report;
          // Trigger measurement collection.
          logger.info("E2 indication: Triggering measurement collection for Action ID={}", action.ric_action_id);
          action.report_service->collect_measurements();
          logger.info("E2 indication: Measurement collection completed, checking if message ready");
          if (action.report_service->is_ind_msg_ready()) {
            logger.info("E2 indication: Message ready, getting indication message");
#ifdef E2_KPM_DEBUG
            printf("[E2 TRACE] Calling report_service->get_indication_message()...\n");
#endif
            // Get RIC indication msg content.
            ind_msg_bytes = action.report_service->get_indication_message();
#ifdef E2_KPM_DEBUG
            printf("[E2 TRACE] Returned from get_indication_message(), size=%zu\n", ind_msg_bytes.length());
#endif
            ind_hdr_bytes = action.report_service->get_indication_header();
            logger.info("E2 indication: Got message (size={}) and header (size={})", ind_msg_bytes.length(), ind_hdr_bytes.length());
          } else {
            logger.warning("E2 indication: Message not ready, skipping this action");
            continue;
          }
          break;
        case ric_action_type_e::ric_action_type_opts::insert:
          e2_ind.indication->ric_ind_type.value = ric_ind_type_e::ric_ind_type_opts::insert;
          break;
        default:
          logger.error("Unknown action type");
          break;
      }

      // Put RIC indication content into message.
#ifdef E2_KPM_DEBUG
      printf("[E2 TRACE] Copying E2SM payload (%zu bytes) into E2AP Indication...\n", ind_msg_bytes.length());
#endif
      if (!e2_ind.indication->ric_ind_msg.resize(ind_msg_bytes.length())) {
        logger.error("Unable to resize byte_buffer, dropping indication");
        continue;
      }
      std::copy(ind_msg_bytes.begin(), ind_msg_bytes.end(), e2_ind.indication->ric_ind_msg.begin());
#ifdef E2_KPM_DEBUG
      printf("[E2 TRACE] E2AP Indication Message populated: hdr=%zu bytes, msg=%zu bytes\n",
             e2_ind.indication->ric_ind_hdr.length(), e2_ind.indication->ric_ind_msg.length());
#endif
      if (!e2_ind.indication->ric_ind_hdr.resize(ind_hdr_bytes.length())) {
        logger.error("Unable to resize byte_buffer, dropping indication");
        continue;
      }
      std::copy(ind_hdr_bytes.begin(), ind_hdr_bytes.end(), e2_ind.indication->ric_ind_hdr.begin());

#ifdef E2_KPM_DEBUG
      // PHASE 3: E2AP Transmission Validation
      logger.warning("[E2 SEND] PHASE 3: E2AP RIC Indication");
      logger.warning("[E2 SEND] RAN Function ID: {}", e2_ind.indication->ran_function_id);
      logger.warning("[E2 SEND] RIC Request ID - Requestor: {}, Instance: {}",
                     e2_ind.indication->ric_request_id.ric_requestor_id,
                     e2_ind.indication->ric_request_id.ric_instance_id);
      logger.warning("[E2 SEND] RIC Action ID: {}", e2_ind.indication->ric_action_id);
      logger.warning("[E2 SEND] Indication Type: {}", e2_ind.indication->ric_ind_type.to_string());
      logger.warning("[E2 SEND] Indication Header size: {} bytes", e2_ind.indication->ric_ind_hdr.length());
      logger.warning("[E2 SEND] Indication Message size: {} bytes", e2_ind.indication->ric_ind_msg.length());
#endif

      logger.info("Sending E2 indication");

      // Real-time console output for E2 transmission
      static uint32_t e2_send_count = 0;
      if (++e2_send_count % 10 == 0) {
        std::cout << " [E2 TX]" << std::endl;
      }

      send_e2_indication(e2_ind);
    }
  }
  CORO_RETURN();
}

void e2_indication_procedure::send_e2_indication(e2_indication_message& e2_ind)
{
#ifdef E2_KPM_DEBUG
  printf("[E2 TRACE] ===== Entered send_e2_indication() =====\n");
  printf("[E2 TRACE] Building E2AP Indication PDU...\n");
#endif

  e2_message e2_msg;
  e2_msg.pdu.set_init_msg();
  e2_msg.pdu.init_msg().load_info_obj(ASN1_E2AP_ID_RIC_IND);
  e2_msg.pdu.init_msg().value.ric_ind() = e2_ind.indication;
  e2_msg.pdu.init_msg().crit            = asn1::crit_opts::ignore;

#ifdef E2_KPM_DEBUG
  printf("[E2 TRACE] E2AP PDU constructed successfully\n");
  printf("[E2 TRACE] Calling notifier.on_new_message() to send via SCTP...\n");
  logger.warning("[E2 SEND] SCTP: Sending E2AP PDU (RIC Indication)");
  logger.warning("[E2 SEND] Total payload size: {} bytes (header + message)",
                 e2_ind.indication->ric_ind_hdr.length() + e2_ind.indication->ric_ind_msg.length());
#endif

  notifier.on_new_message(e2_msg);

#ifdef E2_KPM_DEBUG
  printf("[E2 TRACE] notifier.on_new_message() returned - SCTP send completed\n");
  logger.warning("[E2 SEND] SCTP: Message sent successfully");
#endif
}
