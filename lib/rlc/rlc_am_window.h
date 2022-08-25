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

#include "srsgnb/adt/circular_map.h"
#include "srsgnb/ran/bearer_logger.h"
#include "srsgnb/support/srsgnb_assert.h"
#include <cstdint>

namespace srsgnb {

template <class T>
class rlc_am_window_base
{
public:
  virtual ~rlc_am_window_base()            = default;
  virtual T&     add_sn(size_t sn)         = 0;
  virtual void   remove_sn(size_t sn)      = 0;
  virtual T&     operator[](size_t sn)     = 0;
  virtual size_t size() const              = 0;
  virtual bool   empty() const             = 0;
  virtual bool   full() const              = 0;
  virtual void   clear()                   = 0;
  virtual bool   has_sn(uint32_t sn) const = 0;
};

/// \brief This class provides a container for the Tx/Rx windows holding RLC SDU info objects that are indexed by
/// Sequence Numbers (SN)
/// @tparam T storage type
/// @tparam WINDOW_SIZE size of the RLC AM window
template <class T, std::size_t WINDOW_SIZE>
class rlc_am_window final : public rlc_am_window_base<T>
{
public:
  rlc_am_window(bearer_logger& logger) : logger(logger) {}
  ~rlc_am_window() = default;

  T& add_sn(size_t sn) override
  {
    if (has_sn(sn)) {
      logger.log_error("SN={} already present in window, overwriting.", sn);
      srsgnb_assertion_failure("SN={} already present in window.", sn);
    } else {
      logger.log_debug("Adding SN={} to window", sn);
    }
    window.overwrite(sn, T());
    return window[sn];
  }
  void remove_sn(size_t sn) override
  {
    if (not has_sn(sn)) {
      logger.log_error("Cannot remove SN={} because not contained in the window.", sn);
      srsgnb_assertion_failure("Cannot remove SN={} because not contained in the window.", sn);
      return;
    }
    logger.log_debug("Removing SN={} from window", sn);
    window.erase(sn);
  }
  T&     operator[](size_t sn) override { return window[sn]; }
  size_t size() const override { return window.size(); }
  bool   full() const override { return window.full(); }
  bool   empty() const override { return window.empty(); }
  void   clear() override { window.clear(); }

  bool has_sn(uint32_t sn) const override { return window.contains(sn); }

private:
  bearer_logger&                                 logger;
  srsgnb::circular_map<uint32_t, T, WINDOW_SIZE> window;
};

} // namespace srsgnb