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

#include "srsgnb/mac/mac_cell_result.h"

namespace srsgnb {
namespace unittests {

/// Builds and returns a valid SIB1 information PDU.
sib_information build_valid_sib1_information_pdu();

/// Builds and returns a valid MAC SSB PDU.
dl_ssb_pdu build_valid_dl_ssb_pdu();

/// Builds and returns a valid MAC DL sched result.
mac_dl_sched_result build_valid_mac_dl_sched_result();

/// Builds and returns a valid PRACH occassion.
prach_occasion_info build_valid_prach_occassion();

/// Builds and returns a valid PUSCH PDU.
pusch_information build_valid_pusch_pdu();

/// Build and returns a valid PUCCH format 1 PDU.
pucch_info build_valid_pucch_format_1_pdu();

} // namespace unittests
} // namespace srsgnb
