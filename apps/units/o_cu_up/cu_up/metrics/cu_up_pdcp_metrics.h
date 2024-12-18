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

#pragma once

#include "apps/services/metrics/metrics_consumer.h"
#include "apps/services/metrics/metrics_properties.h"
#include "apps/services/metrics/metrics_set.h"
#include "srsran/adt/span.h"
#include "srsran/pdcp/pdcp_entity.h"

namespace srsran {

/// PDCP metrics properties implementation.
class pdcp_metrics_properties_impl : public app_services::metrics_properties
{
public:
  std::string_view name() const override { return "PDCP metrics"; }
};

class pdcp_metrics_impl : public app_services::metrics_set
{
  pdcp_metrics_properties_impl properties;
  pdcp_metrics_container       metrics;

public:
  explicit pdcp_metrics_impl(const pdcp_metrics_container& metrics_) : metrics(metrics_) {}

  // See interface for documentation.
  const app_services::metrics_properties& get_properties() const override { return properties; }

  const pdcp_metrics_container& get_metrics() const { return metrics; }
};

/// Callback for the RLC metrics.
inline auto pdcp_metrics_callback = [](const app_services::metrics_set&      report,
                                       span<app_services::metrics_consumer*> consumers,
                                       task_executor&                        executor,
                                       srslog::basic_logger&                 logger) {
  const auto& metric = static_cast<const pdcp_metrics_impl&>(report);

  if (!executor.defer([metric, consumers]() {
        for (auto& consumer : consumers) {
          consumer->handle_metric(metric);
        }
      })) {
    logger.error("Failed to dispatch the metric '{}'", metric.get_properties().name());
  }
};

} // namespace srsran