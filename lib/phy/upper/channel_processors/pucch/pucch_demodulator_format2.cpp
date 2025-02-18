/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
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

/// \file
/// \brief PUCCH Format 2 demodulator definition.

#include "pucch_demodulator_format2.h"
#include "srsran/phy/support/mask_types.h"
#include "srsran/phy/support/resource_grid_reader.h"

using namespace srsran;

/// \brief Control data RE allocation pattern for PUCCH Format 2.
///
/// Indicates the Resource Elements containing control data symbols within a PRB, as per TS38.211 Section 6.4.1.3.2.2.
static const re_prb_mask format2_prb_re_mask =
    {true, false, true, true, false, true, true, false, true, true, false, true};

void pucch_demodulator_format2::demodulate(span<log_likelihood_ratio>                      llr,
                                           const resource_grid_reader&                     grid,
                                           const channel_estimate&                         estimates,
                                           const pucch_demodulator::format2_configuration& config)
{
  // Number of receive antenna ports.
  auto nof_rx_ports = static_cast<unsigned>(config.rx_ports.size());

  // Number of data Resource Elements in a slot for a single Rx port.
  unsigned nof_re_port = pucch_constants::FORMAT2_NOF_DATA_SC * config.nof_prb * config.nof_symbols;

  // Assert that allocations are valid.
  srsran_assert(config.nof_prb && config.nof_prb <= pucch_constants::FORMAT2_MAX_NPRB,
                "Invalid Number of PRB allocated to PUCCH Format 2, i.e., {}. Valid range is 1 to {}.",
                config.nof_prb,
                pucch_constants::FORMAT2_MAX_NPRB);

  srsran_assert((config.first_prb + config.nof_prb) * NRE <= grid.get_nof_subc(),
                "PUCCH Format 2: PRB allocation outside grid (first hop). Requested [{}, {}), grid has {} PRBs.",
                config.first_prb,
                config.first_prb + config.nof_prb,
                grid.get_nof_subc() / NRE);

  srsran_assert(!config.second_hop_prb.has_value() ||
                    ((config.second_hop_prb.value() + config.nof_prb) * NRE <= grid.get_nof_subc()),
                "PUCCH Format 2: PRB allocation outside grid (second hop). Requested [{}, {}), grid has {} PRBs.",
                config.second_hop_prb.value(),
                config.second_hop_prb.value() + config.nof_prb,
                grid.get_nof_subc() / NRE);

  srsran_assert(config.nof_symbols && config.nof_symbols <= pucch_constants::FORMAT2_MAX_NSYMB,
                "Invalid Number of OFDM symbols allocated to PUCCH Format 2, i.e., {}. Valid range is 1 to {}.",
                config.nof_symbols,
                pucch_constants::FORMAT2_MAX_NSYMB);

  // Resize data and channel estimation buffers.
  ch_re.resize(nof_rx_ports, nof_re_port);
  ch_estimates.resize(nof_re_port, nof_rx_ports, pucch_constants::MAX_LAYERS);

  // Resize equalized data and post equalization noise variance buffers.
  eq_re.resize(nof_re_port);
  eq_noise_vars.resize(nof_re_port);

  // Extract data RE and channel estimation coefficients.
  get_data_re_ests(grid, estimates, config);

  // Extract the Rx port noise variances from the channel estimation.
  for (unsigned i_port = 0; i_port != nof_rx_ports; ++i_port) {
    noise_var_estimates[i_port] = estimates.get_noise_variance(i_port, 0);
  }

  // Equalize the data RE.
  equalizer->equalize(
      eq_re, eq_noise_vars, ch_re, ch_estimates, span<float>(noise_var_estimates).first(nof_rx_ports), 1.0F);

  // Assert that the number of RE returned by the channel equalizer matches the expected number of LLR.
  srsran_assert(eq_re.size() == llr.size() / get_bits_per_symbol(modulation_scheme::QPSK),
                "Number of equalized RE (i.e. {}) does not match the expected LLR data length (i.e. {})",
                eq_re.size(),
                llr.size() / get_bits_per_symbol(modulation_scheme::QPSK));

  // Apply soft symbol demodulation. PUCCH Format 2 modulation scheme is always QPSK, as per TS38.211 Section 6.3.2.5.2.
  demapper->demodulate_soft(llr, eq_re, eq_noise_vars, modulation_scheme::QPSK);

  // Descramble, as per TS38.211 Section 6.3.2.5.1.
  unsigned c_init = config.rnti * pow2(15) + config.n_id;
  descrambler->init(c_init);
  descrambler->apply_xor(llr, llr);
}

void pucch_demodulator_format2::get_data_re_ests(const resource_grid_reader&                     resource_grid,
                                                 const channel_estimate&                         channel_ests,
                                                 const pucch_demodulator::format2_configuration& config)
{
  // Prepare RB mask. RB allocation is contiguous for PUCCH Format 2.
  bounded_bitset<MAX_RB> prb_mask;
  prb_mask.resize(config.nof_prb);
  prb_mask.fill(0, config.nof_prb, true);

  // Prepare RE mask.
  bounded_bitset<MAX_RB* NRE> re_mask = prb_mask.kronecker_product<NRE>(format2_prb_re_mask);

  for (unsigned i_port = 0, i_port_end = config.rx_ports.size(); i_port != i_port_end; ++i_port) {
    // Get a view of the data RE destination buffer for a single Rx port.
    span<cbf16_t> re_port_buffer = ch_re.get_slice(i_port);

    // Get a view of the channel estimates destination buffer for a single Rx port and Tx layer.
    span<cbf16_t> ests_port_buffer = ch_estimates.get_channel(i_port, 0);

    // First OFDM subcarrier containing PUCCH Format 2.
    unsigned first_subc = config.first_prb * NRE;

    for (unsigned i_symbol = config.start_symbol_index, i_symbol_end = config.start_symbol_index + config.nof_symbols;
         i_symbol != i_symbol_end;
         ++i_symbol) {
      if ((i_symbol > config.start_symbol_index) && config.second_hop_prb.has_value()) {
        first_subc = config.second_hop_prb.value() * NRE;
      }

      // Extract data RE from the resource grid.
      re_port_buffer = resource_grid.get(re_port_buffer, i_port, i_symbol, first_subc, re_mask);

      // View over the channel estimation coefficients for a single OFDM symbol.
      span<const cbf16_t> ests_symbol = channel_ests.get_symbol_ch_estimate(i_symbol, i_port);

      // Copy channel estimation coefficients of the data RE into the destination buffer.
      re_mask.for_each(0, re_mask.size(), [&ests_port_buffer, &ests_symbol, &first_subc](unsigned bitpos) {
        ests_port_buffer.front() = ests_symbol[first_subc + bitpos];
        ests_port_buffer         = ests_port_buffer.last(ests_port_buffer.size() - 1);
      });
    }

    // Assert that all port data RE buffer elements have been filled.
    srsran_assert(re_port_buffer.empty(),
                  "Number of extracted port data RE does not match destination buffer dimensions: "
                  "{} unused elements.",
                  re_port_buffer.size());

    // Assert that all port channel estimates buffer elements have been filled.
    srsran_assert(
        ests_port_buffer.empty(),
        "Number of extracted port channel estimation coefficients does not match destination buffer dimensions: "
        "{} unused elements.",
        ests_port_buffer.size());
  }
}
