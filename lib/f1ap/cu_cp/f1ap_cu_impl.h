/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "procedures/ue_context_modification_procedure.h"
#include "procedures/ue_context_release_procedure.h"
#include "procedures/ue_context_setup_procedure.h"
#include "ue_context/f1ap_cu_ue_context.h"
#include "srsran/adt/slotted_array.h"
#include "srsran/asn1/f1ap/f1ap.h"
#include "srsran/f1ap/cu_cp/f1ap_cu.h"
#include "srsran/ran/nr_cgi.h"
#include "srsran/support/executors/task_executor.h"
#include <memory>

namespace srsran {
namespace srs_cu_cp {

class f1ap_ue_transaction_manager;

class f1ap_cu_impl final : public f1ap_cu
{
public:
  f1ap_cu_impl(f1ap_message_notifier&       f1ap_pdu_notifier_,
               f1ap_du_processor_notifier&  f1ap_du_processor_notifier_,
               f1ap_du_management_notifier& f1ap_du_management_notifier_,
               timer_manager&               timers_,
               task_executor&               ctrl_exec_);
  ~f1ap_cu_impl();

  // f1ap connection manager functions
  void handle_f1_setup_response(const f1ap_f1_setup_response& msg) override;

  // f1ap rrc message transfer procedure functions
  void handle_dl_rrc_message_transfer(const f1ap_dl_rrc_message& msg) override;

  // f1ap ue context manager functions
  async_task<f1ap_ue_context_setup_response>
  handle_ue_context_setup_request(const f1ap_ue_context_setup_request& request,
                                  bool                                 is_inter_cu_handover = false) override;

  async_task<ue_index_t> handle_ue_context_release_command(const f1ap_ue_context_release_command& msg) override;

  async_task<f1ap_ue_context_modification_response>
  handle_ue_context_modification_request(const f1ap_ue_context_modification_request& request) override;

  // f1ap paging handler functions
  void handle_paging(const cu_cp_paging_message& msg) override;

  // f1ap message handler functions
  void handle_message(const f1ap_message& msg) override;

  void handle_connection_loss() override {}

  // f1ap statistics
  int get_nof_ues() override;

  // f1ap_cu_interface
  f1ap_message_handler&     get_f1ap_message_handler() override { return *this; }
  f1ap_event_handler&       get_f1ap_event_handler() override { return *this; }
  f1ap_rrc_message_handler& get_f1ap_rrc_message_handler() override { return *this; }
  f1ap_connection_manager&  get_f1ap_connection_manager() override { return *this; }
  f1ap_ue_context_manager&  get_f1ap_ue_context_manager() override { return *this; }
  f1ap_statistics_handler&  get_f1ap_statistics_handler() override { return *this; }
  f1ap_paging_manager&      get_f1ap_paging_manager() override { return *this; }

private:
  /// \brief Handle the reception of an initiating message.
  /// \param[in] msg The received initiating message.
  void handle_initiating_message(const asn1::f1ap::init_msg_s& msg);

  /// \brief Handle the reception of an F1 Setup Request.
  /// \param[in] request The F1 Setup Request.
  void handle_f1_setup_request(const asn1::f1ap::f1_setup_request_s& request);

  /// \brief Handle the reception of an Initial UL RRC Message Transfer message.
  /// This starts the UE and SRB creation at the DU processor, F1 and RRC UE.
  /// @see rrc_setup_procedure.
  /// \param[in] msg The F1AP initial UL RRC message.
  void handle_initial_ul_rrc_message(const asn1::f1ap::init_ul_rrc_msg_transfer_s& msg);

  /// \brief Handle the reception of an UL RRC Message Transfer message.
  /// \param[in] msg The F1AP UL RRC message.
  void handle_ul_rrc_message(const asn1::f1ap::ul_rrc_msg_transfer_s& msg);

  /// \brief Handle the reception of an successful outcome.
  /// \param[in] msg The received successful outcome message.
  void handle_successful_outcome(const asn1::f1ap::successful_outcome_s& outcome);

  /// \brief Handle the reception of an unsuccessful outcome.
  /// \param[in] msg The received unsuccessful outcome message.
  void handle_unsuccessful_outcome(const asn1::f1ap::unsuccessful_outcome_s& outcome);

  /// \brief Handle the reception of an F1 Removal Request.
  /// \param[in] msg The F1 Removal Request message.
  void handle_f1_removal_request(const asn1::f1ap::f1_removal_request_s& msg);

  /// \brief Handle the reception of an UE Context Release Request.
  /// \param[in] msg The UE Context Release Request message.
  void handle_ue_context_release_request(const asn1::f1ap::ue_context_release_request_s& msg);

  gnb_cu_ue_f1ap_id_t allocate_f1ap_id(ue_index_t ue_index);

  srslog::basic_logger& logger;

  /// Repository of UE Contexts.
  f1ap_ue_context_list ue_ctxt_list;

  // nofifiers and handles
  f1ap_message_notifier&       pdu_notifier;
  f1ap_du_processor_notifier&  du_processor_notifier;
  f1ap_du_management_notifier& du_management_notifier;
  timer_manager&               timers;
  task_executor&               ctrl_exec;

  unsigned current_transaction_id = 0; // store current F1AP transaction id
};

} // namespace srs_cu_cp
} // namespace srsran
