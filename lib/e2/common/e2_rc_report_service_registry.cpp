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

#include "srsran/e2/e2_rc_report_service_registry.h"

using namespace srsran;

e2_rc_report_service_registry& e2_rc_report_service_registry::get_instance()
{
  static e2_rc_report_service_registry instance;
  return instance;
}

void e2_rc_report_service_registry::register_ue_context_notifier(e2_du_ue_context_notifier* notifier_)
{
  notifier = notifier_;
}

void e2_rc_report_service_registry::unregister_ue_context_notifier(e2_du_ue_context_notifier* notifier_)
{
  if (notifier == notifier_) {
    notifier = nullptr;
  }
}

e2_du_ue_context_notifier* e2_rc_report_service_registry::get_ue_context_notifier()
{
  return notifier;
}
