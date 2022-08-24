/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsgnb/adt/expected.h"
#include "srsgnb/asn1/ngap.h"
#include "srsgnb/support/async/event_signal.h"

namespace srsgnb {
namespace srs_cu_cp {

class ngap_event_manager
{
public:
  /// NG setup procedure outcome
  using ngap_setup_outcome_t = expected<const asn1::ngap::ng_setup_resp_s*, const asn1::ngap::ng_setup_fail_s*>;
  event_signal<ngap_setup_outcome_t> ngap_setup_response;
};

} // namespace srs_cu_cp
} // namespace srsgnb