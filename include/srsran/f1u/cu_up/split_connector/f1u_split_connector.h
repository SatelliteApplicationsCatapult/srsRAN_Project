/*
 *
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/f1u/cu_up/f1u_bearer_logger.h"
#include "srsran/f1u/cu_up/f1u_gateway.h"
#include "srsran/gtpu/gtpu_tunnel_nru_factory.h"
#include "srsran/gtpu/ngu_gateway.h"
#include "srsran/srslog/srslog.h"
#include <cstdint>
#include <unordered_map>

namespace srsran {

/// \brief Object used to represent a bearer at the CU F1-U gateway
/// On the co-located case this is done by connecting both entities directly.
///
/// It will keep a notifier to the DU NR-U RX and provide the methods to pass
/// an SDU to it.
class f1u_split_gateway_cu_bearer : public f1u_cu_up_gateway_bearer
{
public:
  f1u_split_gateway_cu_bearer(uint32_t                              ue_index,
                              drb_id_t                              drb_id,
                              const up_transport_layer_info&        ul_tnl_info_,
                              f1u_cu_up_gateway_bearer_rx_notifier& cu_rx_,
                              task_executor&                        ul_exec_,
                              srs_cu_up::f1u_bearer_disconnector&   disconnector_) :
    logger("CU-F1-U", {ue_index, drb_id, ul_tnl_info}),
    disconnector(disconnector_),
    ul_tnl_info(ul_tnl_info_),
    cu_rx(cu_rx_),
    ul_exec(ul_exec_)
  {
    gtpu_tunnel_nru_creation_message msg{};
    tunnel = srsran::create_gtpu_tunnel_nru(msg);
  }

  ~f1u_split_gateway_cu_bearer() override { stop(); }

  void stop() override { disconnector.disconnect_cu_bearer(ul_tnl_info); }

  void on_new_pdu(nru_dl_message msg) override
  {
    // TODO
  }

private:
  srs_cu_up::f1u_bearer_logger        logger;
  srs_cu_up::f1u_bearer_disconnector& disconnector;
  up_transport_layer_info             ul_tnl_info;
  std::unique_ptr<gtpu_tunnel_nru>    tunnel;

public:
  /// Holds notifier that will point to NR-U bearer on the UL path
  f1u_cu_up_gateway_bearer_rx_notifier& cu_rx;

  /// Holds the DL UP TNL info associated with the F1-U bearer.
  optional<up_transport_layer_info> dl_tnl_info;

  /// Holds the DL UP TNL info associated with the F1-U bearer.
  task_executor& ul_exec;
};

/// \brief Object used to connect the DU and CU-UP F1-U bearers
/// On the co-located case this is done by connecting both entities directly.
///
/// Note that CU and DU bearer creation and removal can be performed from different threads and are therefore
/// protected by a common mutex.
class f1u_split_connector final : public f1u_cu_up_gateway
{
public:
  f1u_split_connector() : logger_cu(srslog::fetch_basic_logger("CU-F1-U")) {}

  f1u_cu_up_gateway* get_f1u_cu_up_gateway() { return this; }

  std::unique_ptr<f1u_cu_up_gateway_bearer> create_cu_bearer(uint32_t                              ue_index,
                                                             drb_id_t                              drb_id,
                                                             const srs_cu_up::f1u_config&          config,
                                                             const up_transport_layer_info&        ul_up_tnl_info,
                                                             f1u_cu_up_gateway_bearer_rx_notifier& rx_notifier,
                                                             task_executor&                        ul_exec,
                                                             timer_factory                         ue_dl_timer_factory,
                                                             unique_timer& ue_inactivity_timer) override;

  void attach_dl_teid(const up_transport_layer_info& ul_up_tnl_info,
                      const up_transport_layer_info& dl_up_tnl_info) override;

  void disconnect_cu_bearer(const up_transport_layer_info& ul_up_tnl_info) override;

private:
  srslog::basic_logger& logger_cu;
  // Key is the UL UP TNL Info (CU-CP address and UL TEID reserved by CU-CP)
  std::unordered_map<up_transport_layer_info, f1u_split_gateway_cu_bearer*> cu_map;

  srs_cu_up::ngu_tnl_pdu_session* udp_gw;
  std::mutex                      map_mutex; // shared mutex for access to cu_map
};

} // namespace srsran
