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

#include "lib/cu_cp/ue_manager/ue_manager_impl.h"
#include "lib/ngap/ngap_asn1_helpers.h"
#include "lib/ngap/ngap_error_indication_helper.h"
#include "tests/unittests/ngap/test_helpers.h"
#include "srsran/cu_cp/cu_cp_configuration_helpers.h"
#include "srsran/gateways/sctp_network_gateway_factory.h"
#include "srsran/ngap/ngap_configuration_helpers.h"
#include "srsran/ngap/ngap_factory.h"
#include "srsran/support/async/async_test_utils.h"
#include "srsran/support/executors/manual_task_worker.h"
#include "srsran/support/io/io_broker_factory.h"
#include "srsran/support/timers.h"
#include <gtest/gtest.h>

using namespace srsran;
using namespace srs_cu_cp;

/// This test is an integration test between:
/// * NGAP (including ASN1 packer and NG setup procedure)
/// * SCTP network gateway
/// * IO broker
class ngap_network_adapter : public n2_connection_client,
                             public sctp_network_gateway_control_notifier,
                             public network_gateway_data_notifier
{
public:
  ngap_network_adapter(const sctp_network_connector_config& nw_config_) :
    nw_config(nw_config_),
    epoll_broker(create_io_broker(io_broker_type::epoll)),
    gw(create_sctp_network_gateway({nw_config, *this, *this}))
  {
    report_fatal_error_if_not(gw->create_and_connect(), "Failed to connect NGAP GW");
    if (!gw->subscribe_to(*epoll_broker)) {
      report_fatal_error("Failed to register N2 (SCTP) network gateway at IO broker");
    }
  }

  std::unique_ptr<ngap_message_notifier>
  handle_cu_cp_connection_request(std::unique_ptr<ngap_message_notifier> cu_cp_rx_pdu_notifier) override
  {
    class dummy_ngap_pdu_notifier : public ngap_message_notifier
    {
    public:
      dummy_ngap_pdu_notifier(ngap_network_adapter& parent_) : parent(parent_) {}

      void on_new_message(const ngap_message& msg) override
      {
        byte_buffer pdu;
        {
          asn1::bit_ref bref{pdu};
          if (msg.pdu.pack(bref) != asn1::SRSASN_SUCCESS) {
            parent.test_logger.error("Failed to pack PDU");
            return;
          }
        }
        parent.gw->handle_pdu(std::move(pdu));
      }

    private:
      ngap_network_adapter& parent;
    };

    rx_pdu_notifier = std::move(cu_cp_rx_pdu_notifier);
    return std::make_unique<dummy_ngap_pdu_notifier>(*this);
  }

private:
  // SCTP network gateway calls interface to inject received PDUs (ASN1 packed)
  void on_new_pdu(byte_buffer pdu) override
  {
    ngap_message msg;
    {
      asn1::cbit_ref bref{pdu};
      if (msg.pdu.unpack(bref) != asn1::SRSASN_SUCCESS) {
        test_logger.error("Sending Error Indication. Cause: Could not unpack Rx PDU");
        send_error_indication(*rx_pdu_notifier, test_logger);
      }
    }
    rx_pdu_notifier->on_new_message(msg);
  }

  /// \brief Simply log those events for now
  void on_connection_loss() override { test_logger.info("on_connection_loss"); }
  void on_connection_established() override { test_logger.info("on_connection_established"); }

  /// We require a network gateway and a packer
  const sctp_network_connector_config&  nw_config;
  std::unique_ptr<io_broker>            epoll_broker;
  std::unique_ptr<sctp_network_gateway> gw;

  srslog::basic_logger& test_logger = srslog::fetch_basic_logger("TEST");

  std::unique_ptr<ngap_message_notifier> rx_pdu_notifier;
};

class ngap_integration_test : public ::testing::Test
{
protected:
  ngap_integration_test() :
    cu_cp_cfg([this]() {
      cu_cp_configuration cucfg     = config_helpers::make_default_cu_cp_config();
      cucfg.services.timers         = &timers;
      cucfg.services.cu_cp_executor = &ctrl_worker;
      return cucfg;
    }())
  {
    cfg.gnb_id                    = cu_cp_cfg.node.gnb_id;
    cfg.ran_node_name             = cu_cp_cfg.node.ran_node_name;
    cfg.supported_tas             = cu_cp_cfg.node.supported_tas;
    cfg.pdu_session_setup_timeout = cu_cp_cfg.ue.pdu_session_setup_timeout;
  }

  void SetUp() override
  {
    srslog::fetch_basic_logger("TEST").set_level(srslog::basic_levels::debug);
    srslog::init();

    sctp_network_connector_config nw_config;
    nw_config.dest_name         = "AMF";
    nw_config.if_name           = "N2";
    nw_config.connect_address   = "10.12.1.105";
    nw_config.connect_port      = 38412;
    nw_config.bind_address      = "10.8.1.10";
    nw_config.bind_port         = 0;
    nw_config.non_blocking_mode = true;
    adapter                     = std::make_unique<ngap_network_adapter>(nw_config);

    ngap = create_ngap(cfg, cu_cp_notifier, *adapter, timers, ctrl_worker);
  }

  timer_manager       timers;
  manual_task_worker  ctrl_worker{128};
  cu_cp_configuration cu_cp_cfg;
  ngap_configuration  cfg;

  ue_manager                            ue_mng{cu_cp_cfg};
  dummy_ngap_cu_cp_notifier             cu_cp_notifier{ue_mng};
  std::unique_ptr<ngap_network_adapter> adapter;
  std::unique_ptr<ngap_interface>       ngap;

  srslog::basic_logger& test_logger = srslog::fetch_basic_logger("TEST");
};

ngap_ng_setup_request generate_ng_setup_request(const ngap_configuration& ngap_cfg)
{
  ngap_ng_setup_request request_msg = {};

  ngap_ng_setup_request request;

  // fill global ran node id
  request.global_ran_node_id.gnb_id = ngap_cfg.gnb_id;
  // TODO: Which PLMN do we need to use here?
  request.global_ran_node_id.plmn_id = ngap_cfg.supported_tas.front().plmn;
  // fill ran node name
  request.ran_node_name = ngap_cfg.ran_node_name;
  // fill supported ta list
  for (const auto& supported_ta : ngap_cfg.supported_tas) {
    ngap_supported_ta_item supported_ta_item;

    supported_ta_item.tac = supported_ta.tac;

    ngap_broadcast_plmn_item broadcast_plmn_item;
    broadcast_plmn_item.plmn_id = supported_ta.plmn;

    for (const auto& slice_config : supported_ta.supported_slices) {
      slice_support_item_t slice_support_item;
      slice_support_item.s_nssai.sst = slice_config.sst;
      if (slice_config.sd.has_value()) {
        slice_support_item.s_nssai.sd = slice_config.sd.value();
      }
      broadcast_plmn_item.tai_slice_support_list.push_back(slice_support_item);
    }

    supported_ta_item.broadcast_plmn_list.push_back(broadcast_plmn_item);

    request.supported_ta_list.push_back(supported_ta_item);
  }

  // fill paging drx
  request.default_paging_drx = 256;

  return request_msg;
}

/// Test successful ng setup procedure
TEST_F(ngap_integration_test, when_ng_setup_response_received_then_amf_connected)
{
  // Action 1: Launch NG setup procedure
  ngap_ng_setup_request request_msg = generate_ng_setup_request(cfg);

  test_logger.info("Launching NG setup procedure...");
  async_task<ngap_ng_setup_result>         t = ngap->handle_ng_setup_request(request_msg);
  lazy_task_launcher<ngap_ng_setup_result> t_launcher(t);

  // Status: Procedure not yet ready.
  ASSERT_FALSE(t.ready());

  std::this_thread::sleep_for(std::chrono::seconds(3));
}
