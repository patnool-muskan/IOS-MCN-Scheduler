/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file rrc_gNB.c
 * \brief rrc procedures for gNB
 * \author Navid Nikaein and  Raymond Knopp , WEI-TAI CHEN
 * \date 2011 - 2014 , 2018
 * \version 1.0
 * \company Eurecom, NTUST
 * \email: navid.nikaein@eurecom.fr and raymond.knopp@eurecom.fr, kroempa@gmail.com
 */
#define RRC_GNB_C
#define RRC_GNB_C

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nr_rrc_config.h"
#include "openair2/RRC/NR/nr_rrc_proto.h"
#include "openair2/RRC/NR/rrc_gNB_UE_context.h"
#include "openair3/SECU/key_nas_deriver.h"
#include "openair3/ocp-gtpu/gtp_itf.h"
#include "BIT_STRING.h"
#include "F1AP_CauseProtocol.h"
#include "F1AP_CauseRadioNetwork.h"
#include "NGAP_CauseRadioNetwork.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac.h"
#include "OCTET_STRING.h"
#include "PHY/defs_common.h"
#include "RRC/NR/MESSAGES/asn1_msg.h"
#include "RRC/NR/mac_rrc_dl.h"
#include "RRC/NR/nr_rrc_common.h"
#include "SIMULATION/TOOLS/sim.h"
#include "T.h"
#include "asn_codecs.h"
#include "asn_internal.h"
#include "assertions.h"
#include "byte_array.h"
#include "common/ngran_types.h"
#include "common/openairinterface5g_limits.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common_lib.h"
#include "constr_SEQUENCE.h"
#include "constr_TYPE.h"
#include "cucp_cuup_if.h"
#include "e1ap_messages_types.h"
#include "executables/softmodem-common.h"
#include "f1ap_messages_types.h"
#include "gtpv1_u_messages_types.h"
#include "intertask_interface.h"
#include "linear_alloc.h"
#include "ngap_messages_types.h"
#include "NR_HandoverCommand.h"
#include "NR_HandoverCommand-IEs.h"
#include "nr_pdcp/nr_pdcp_entity.h"
#include "nr_pdcp/nr_pdcp_oai_api.h"
#include "nr_rrc_defs.h"
#include "nr_rrc_extern.h"
#include "oai_asn1.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/E1AP/e1ap_common.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/F1AP/lib/f1ap_lib_extern.h"
#include "E1AP/lib/e1ap_bearer_context_management.h"
#include "lib/f1ap_interface_management.h"
#include "rrc_gNB_NGAP.h"
#include "rrc_gNB_du.h"
#include "rrc_gNB_mobility.h"
#include "rrc_gNB_radio_bearers.h"
#include "rrc_messages_types.h"
#include "seq_arr.h"
#include "tree.h"
#include "uper_decoder.h"
#include "uper_encoder.h"
#include "utils.h"
#include "x2ap_messages_types.h"
#include "xer_encoder.h"
#include "E1AP/lib/e1ap_bearer_context_management.h"
#include "E1AP/lib/e1ap_interface_management.h"
#include "cmake_targets/ran_build/build/openair2/RRC/NR/MESSAGES/NR_HandoverPreparationInformation.h"
#include "cmake_targets/ran_build/build/openair2/RRC/NR/MESSAGES/NR_UERadioAccessCapabilityInformation-IEs.h"
#include "openair2/XNAP/xnap_gNB_management_procedures.h"
#include "openair2/COMMON/xnap_messages_types.h"
#include "openair2/XNAP/xnap_gNB_task.h"
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "NR_HandoverCommand.h"
#include "NR_RRCReconfiguration.h"
#include "NR_RRCReconfiguration-IEs.h"
#include "NR_PhysCellId.h"
#include "common/utils/alg/foreach.h"
#include "common/utils/ds/seq_arr.h"
#include "common/utils/LOG/log.h"
#include "NR_DL-DCCH-Message.h"
#ifdef UNI_RAN
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#endif

#ifdef E2_AGENT
#include "openair2/E2AP/RAN_FUNCTION/O-RAN/ran_func_rc_extern.h"
#endif

mui_t rrc_gNB_mui = 0;

uint64_t my_neighbour_cellid;

int new_rnti;
int  HO_in_progress; 
gNB_RRC_UE_t *tmp_ue_p;
uint8_t rrc_reconfig_buffer[2048];
int rrc_reconfig_buffer_len;

#ifdef UNI_RAN
NR_UE_PF_PO_t NR_UE_PF_PO[NFAPI_CC_MAX][MAX_MOBILES_PER_GNB];
pthread_mutex_t nr_ue_pf_po_mutex;
#endif

///---------------------------------------------------------------------------------------------------------------///
///---------------------------------------------------------------------------------------------------------------///
static int neigh_compare(const nr_rrc_neighcells_container_t *a, const nr_rrc_neighcells_container_t *b)
{
  if (a->assoc_id > b->assoc_id)
    return 1;
  if (a->assoc_id == b->assoc_id)
    return 0;
  return -1; /* a->assoc_id < b->assoc_id */
}

/* Tree management functions */
RB_GENERATE(rrc_neigh_cell_tree, nr_rrc_neighcells_container_t, entries, neigh_compare);

static void clear_nas_pdu(ngap_pdu_t *pdu)
{
  DevAssert(pdu != NULL);
  free(pdu->buffer); // does nothing if NULL
  pdu->buffer = NULL;
  pdu->length = 0;
}

static void freeDRBlist(NR_DRB_ToAddModList_t *list)
{
  //ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToAddModList, list);
  return;
}

const neighbour_cell_configuration_t *get_neighbour_config(int serving_cell_nr_cellid)
{
  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  seq_arr_t *neighbour_cell_configuration = rrc->neighbour_cell_configuration;
  if (!neighbour_cell_configuration)
    return NULL;

  for (int cellIdx = 0; cellIdx < neighbour_cell_configuration->size; cellIdx++) {
    neighbour_cell_configuration_t *neighbour_config =
        (neighbour_cell_configuration_t *)seq_arr_at(neighbour_cell_configuration, cellIdx);
    if (neighbour_config->nr_cell_id == serving_cell_nr_cellid)
      return neighbour_config;
  }
  return NULL;
}

const nr_neighbour_gnb_configuration_t *get_neighbour_cell_information(int serving_cell_nr_cellid, int neighbour_cell_phy_id)
{
  const gNB_RRC_INST *rrc = RC.nrrrc[0];
  seq_arr_t *neighbour_cell_configuration = rrc->neighbour_cell_configuration;
  for (int cellIdx = 0; cellIdx < neighbour_cell_configuration->size; cellIdx++) {
    neighbour_cell_configuration_t *neighbour_config =
        (neighbour_cell_configuration_t *)seq_arr_at(neighbour_cell_configuration, cellIdx);
    if (!neighbour_config)
      continue;

    for (int neighbourIdx = 0; neighbourIdx < neighbour_config->neighbour_cells->size; neighbourIdx++) {
      nr_neighbour_gnb_configuration_t *neighbour =
          (nr_neighbour_gnb_configuration_t *)seq_arr_at(neighbour_config->neighbour_cells, neighbourIdx);
      if (neighbour != NULL && neighbour->physicalCellId == neighbour_cell_phy_id)
        return neighbour;
    }
  }
  return NULL;
}

typedef struct deliver_dl_rrc_message_data_s {
  const gNB_RRC_INST *rrc;
  f1ap_dl_rrc_message_t *dl_rrc;
  sctp_assoc_t assoc_id;
} deliver_dl_rrc_message_data_t;
static void rrc_deliver_dl_rrc_message(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_dl_rrc_message_data_t *data = (deliver_dl_rrc_message_data_t *)deliver_pdu_data;
  data->dl_rrc->rrc_container = (uint8_t *)buf;
  data->dl_rrc->rrc_container_length = size;
  DevAssert(data->dl_rrc->srb_id == srb_id);
  data->rrc->mac_rrc.dl_rrc_message_transfer(data->assoc_id, data->dl_rrc);
}

void nr_rrc_transfer_protected_rrc_message(const gNB_RRC_INST *rrc,
                                           const gNB_RRC_UE_t *ue_p,
                                           uint8_t srb_id,
                                           const uint8_t *buffer,
                                           int size)
{
  DevAssert(size > 0);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {.gNB_CU_ue_id = ue_p->rrc_ue_id, .gNB_DU_ue_id = ue_data.secondary_ue, .srb_id = srb_id};
  deliver_dl_rrc_message_data_t data = {.rrc = rrc, .dl_rrc = &dl_rrc, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(ue_p->rrc_ue_id,
                       srb_id,
                       rrc_gNB_mui++,
                       size,
                       (unsigned char *const)buffer,
                       rrc_deliver_dl_rrc_message,
                       &data);
}

///---------------------------------------------------------------------------------------------------------------///
///---------------------------------------------------------------------------------------------------------------///

static void init_NR_SI(gNB_RRC_INST *rrc)
{
  if (!NODE_IS_DU(rrc->node_type)) {
    rrc->carrier.SIB23 = (uint8_t *) malloc16(100);
    AssertFatal(rrc->carrier.SIB23 != NULL, "cannot allocate memory for SIB");
    rrc->carrier.sizeof_SIB23 = do_SIB23_NR(&rrc->carrier);
    LOG_I(NR_RRC, "do_SIB23_NR, size %d\n", rrc->carrier.sizeof_SIB23);
    AssertFatal(rrc->carrier.sizeof_SIB23 != 255,"FATAL, RC.nrrrc[mod].carrier[CC_id].sizeof_SIB23 == 255");
  }

  if (get_softmodem_params()->phy_test > 0 || get_softmodem_params()->do_ra > 0) {
    AssertFatal(NODE_IS_MONOLITHIC(rrc->node_type), "phy_test and do_ra only work in monolithic\n");
    rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_allocate_new_ue_context(rrc);
    LOG_I(NR_RRC,"Adding new user (%p)\n",ue_context_p);
    if (!NODE_IS_CU(rrc->node_type)) {
      rrc_add_nsa_user(rrc,ue_context_p,NULL);
    }
  }
}

static void rrc_gNB_CU_DU_init(gNB_RRC_INST *rrc)
{
  switch (rrc->node_type) {
    case ngran_gNB_CUCP:
      mac_rrc_dl_f1ap_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_e1ap_init(rrc);
      break;
    case ngran_gNB_CU:
      mac_rrc_dl_f1ap_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_direct_init(rrc);
      break;
    case ngran_gNB:
      mac_rrc_dl_direct_init(&rrc->mac_rrc);
      cucp_cuup_message_transfer_direct_init(rrc);
       break;
    case ngran_gNB_DU:
      /* silently drop this, as we currently still need the RRC at the DU. As
       * soon as this is not the case anymore, we can add the AssertFatal() */
      //AssertFatal(1==0,"nothing to do for DU\n");
      break;
    default:
      AssertFatal(0 == 1, "Unknown node type %d\n", rrc->node_type);
      break;
  }
  cu_init_f1_ue_data();
}

pdusession_level_qos_parameter_t *get_qos_characteristics(const int qfi, rrc_pdu_session_param_t *pduSession);
qos_characteristics_t get_qos_char_from_qos_flow_param(const pdusession_level_qos_parameter_t *qos_param);

void openair_rrc_gNB_configuration(gNB_RRC_INST *rrc, gNB_RrcConfigurationReq *configuration)
{
  AssertFatal(rrc != NULL, "RC.nrrrc not initialized!");
  AssertFatal(MAX_MOBILES_PER_GNB < (module_id_t)0xFFFFFFFFFFFFFFFF, " variable overflow");
  AssertFatal(configuration!=NULL,"configuration input is null\n");
  rrc->module_id = 0;
  rrc_gNB_CU_DU_init(rrc);
  uid_linear_allocator_init(&rrc->uid_allocator);
  RB_INIT(&rrc->rrc_ue_head);
  RB_INIT(&rrc->cuups);
  RB_INIT(&rrc->dus);
  rrc->configuration = *configuration;
   /// System Information INIT
  init_NR_SI(rrc);
  return;
} // END openair_rrc_gNB_configuration

void nr_HO_Xn_trigger(ue_id_t ue_id, const nr_neighbour_gnb_configuration_t *neighbour)
{
  instance_t instance = 0;
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p;
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);

  /* If there are no associated gNBs */
  if (instance_p[instance].xn_target_gnb_associated_nb == 0) {
   // TODO : Inform GNB_APP ???
    LOG_W(NR_RRC, "[XNAP] Cannot trigger Xn HO : Number of connected gNBS : %d \n", instance_p[instance].xn_target_gnb_associated_nb);
   } else {
      LOG_I(NR_RRC, "[XNAP] Number of connected gNBS : %d \n", instance_p[instance].xn_target_gnb_associated_nb);
	      
  MessageDef *msg = itti_alloc_new_message(TASK_RRC_GNB, 0, XNAP_HANDOVER_REQ);
  xnap_handover_req_t *req = &XNAP_HANDOVER_REQ(msg);
  req->plmn_id.mcc = instance_p->mcc;
  req->plmn_id.mnc = instance_p->mnc;
  req->plmn_id.mnc_digit_length = instance_p->mnc_len;
  req->ue_id = ue_id;

  req->s_ng_node_ue_xnap_id = ue_id;

  // get ue context to fill handover request structure
  ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[0], ue_id);
  RB_INSERT(rrc_nr_ue_tree_s, &rrc->rrc_ue_head, ue_context_p);
  gNB_RRC_UE_t *ue_ctxt = &ue_context_p->ue_context;
  req->ue_context.ngc_ue_sig_ref = ue_ctxt->amf_ue_ngap_id;
  req->guami.amf_region_id = ue_ctxt->ue_guami.amf_region_id;
  req->guami.amf_set_id = ue_ctxt->ue_guami.amf_set_id;
  req->guami.amf_pointer = ue_ctxt->ue_guami.amf_pointer;
  req->ue_context.as_security_ncc = ue_ctxt->nh_ncc; // ue_ctxt->kgnb_ncc;
  req->ue_context.security_capabilities.nRencryption_algorithms = ue_ctxt->security_capabilities.nRencryption_algorithms;
  req->ue_context.security_capabilities.nRintegrity_algorithms = ue_ctxt->security_capabilities.nRintegrity_algorithms;
  req->ue_context.security_capabilities.eUTRAencryption_algorithms = ue_ctxt->security_capabilities.eUTRAencryption_algorithms;
  req->ue_context.security_capabilities.eUTRAintegrity_algorithms = ue_ctxt->security_capabilities.eUTRAintegrity_algorithms;
  //memcpy(req->ue_context.as_security_key_ranstar, ue_ctxt->kgnb, 32);
  nr_derive_key_ng_ran_star(neighbour->physicalCellId, neighbour->absoluteFrequencySSB, ue_ctxt->nh_ncc >= 0 ? ue_ctxt->nh : ue_ctxt->kgnb, req->ue_context.as_security_key_ranstar);
  req->ue_context.pdusession_tobe_setup_list.num_pdu = ue_ctxt->nb_of_pdusessions;
  if (ue_ctxt->nb_of_pdusessions < 0) {
    LOG_E(NR_RRC, "UE has no PDU Sessions, breaking !! \n");
  } else {
    for (int i = 0; i < ue_ctxt->nb_of_pdusessions; i++) {
      req->ue_context.pdusession_tobe_setup_list.pdu[i].pdusession_id = ue_ctxt->pduSession[i].param.pdusession_id;
      req->ue_context.pdusession_tobe_setup_list.pdu[i].snssai.sst = ue_ctxt->pduSession[i].param.nssai.sst;
      req->ue_context.pdusession_tobe_setup_list.pdu[i].pdu_session_type = ue_ctxt->pduSession[i].param.pdu_session_type;
      req->ue_context.pdusession_tobe_setup_list.pdu[i].up_ngu_tnl_teid_upf = ue_ctxt->pduSession[i].param.gtp_teid;
      if (ue_ctxt->pduSession[i].param.upf_addr.buffer != NULL) {
        LOG_D(NR_RRC, "ue_ctxt->pduSession[i].param.upf_addr.buffer not null\n");
      } else {
        LOG_D(NR_RRC, "ue_ctxt->pduSession[i].param.upf_addr.buffer is null\n");
      }
      memcpy(&req->ue_context.tnl_ip_source, &ue_ctxt->pduSession[i].param.upf_addr.buffer, sizeof(uint8_t) * 4);

      ue_ctxt->initial_pdus = calloc(ue_ctxt->nb_of_pdusessions, sizeof(*ue_ctxt->initial_pdus));

      req->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.num_qos = ue_ctxt->pduSession[i].param.nb_qos;
      for (int j = 0; j < ue_ctxt->pduSession[i].param.nb_qos; j++) {
        req->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qfi = ue_ctxt->pduSession[i].param.qos[j].qfi;
        req->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.non_dynamic.fiveqi =
            ue_ctxt->pduSession[i].param.qos[j].fiveQI;
        req->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.dynamic.fiveqi =
            ue_ctxt->pduSession[i].param.qos[j].fiveQI;
        req->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.dynamic.qos_priority_level =
            ue_ctxt->pduSession[i].param.qos[j].qos_priority;
      }
    }
  }
  uint8_t buf[NR_RRC_BUF_SIZE];
  int size = do_NR_HandoverPreparationInformation(ue_context_p->ue_context.ue_cap_buffer.buf,
                                                  ue_context_p->ue_context.ue_cap_buffer.len,
                                                  buf,
                                                  sizeof buf,
                                                  NULL,0);
  memcpy(req->ue_context.rrc_buffer, buf, size);
  req->ue_context.rrc_buffer_size = size;
  req->target_cgi.cgi = neighbour->nrcell_id;//my_neighbour_cellid;
  req->uehistory_info.last_visited_cgi.cgi = rrc->nr_cellid;
  itti_send_msg_to_task(TASK_XNAP, 0, msg);
  }
}

static void rrc_gNB_process_AdditionRequestInformation(const module_id_t gnb_mod_idP, x2ap_ENDC_sgnb_addition_req_t *m)
{
  struct NR_CG_ConfigInfo *cg_configinfo = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                            &asn_DEF_NR_CG_ConfigInfo,
                            (void **)&cg_configinfo,
                            (uint8_t *)m->rrc_buffer,
                            (int) m->rrc_buffer_size);//m->rrc_buffer_size);
  gNB_RRC_INST         *rrc=RC.nrrrc[gnb_mod_idP];

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
    AssertFatal(1==0,"NR_UL_DCCH_MESSAGE decode error\n");
    // free the memory
    SEQUENCE_free(&asn_DEF_NR_CG_ConfigInfo, cg_configinfo, 1);
    return;
  }

  xer_fprint(stdout,&asn_DEF_NR_CG_ConfigInfo, cg_configinfo);
  // recreate enough of X2 EN-DC Container
  AssertFatal(cg_configinfo->criticalExtensions.choice.c1->present == NR_CG_ConfigInfo__criticalExtensions__c1_PR_cg_ConfigInfo,
              "ueCapabilityInformation not present\n");
  parse_CG_ConfigInfo(rrc,cg_configinfo,m);
  LOG_I(NR_RRC, "Successfully parsed CG_ConfigInfo of size %zu bits. (%zu bytes)\n",
        dec_rval.consumed, (dec_rval.consumed +7/8));
}

//-----------------------------------------------------------------------------
unsigned int rrc_gNB_get_next_transaction_identifier(module_id_t gnb_mod_idP)
//-----------------------------------------------------------------------------
{
  static unsigned int transaction_id[NUMBER_OF_gNB_MAX] = {0};
  // used also in NGAP thread, so need thread safe operation
  unsigned int tmp = __atomic_add_fetch(&transaction_id[gnb_mod_idP], 1, __ATOMIC_SEQ_CST);
  tmp %= NR_RRC_TRANSACTION_IDENTIFIER_NUMBER;
  LOG_T(NR_RRC, "generated xid is %d\n", tmp);
  return tmp;
}

/**
 * @brief Create srb-ToAddModList for RRCSetup and RRCReconfiguration messages
*/
static NR_SRB_ToAddModList_t *createSRBlist(gNB_RRC_UE_t *ue, bool reestablish)
{
  if (!ue->Srb[1].Active) {
    LOG_E(NR_RRC, "Call SRB list while SRB1 doesn't exist\n");
    return NULL;
  }
  NR_SRB_ToAddModList_t *list = CALLOC(sizeof(*list), 1);
  for (int i = 0; i < NR_NUM_SRB; i++)
    if (ue->Srb[i].Active) {
      asn1cSequenceAdd(list->list, NR_SRB_ToAddMod_t, srb);
      srb->srb_Identity = i;
      /* Set reestablishPDCP only for SRB2 */
      if (reestablish && i == 2) {
        asn1cCallocOne(srb->reestablishPDCP, NR_SRB_ToAddMod__reestablishPDCP_true);
      }
    }
  return list;
}

static NR_DRB_ToAddModList_t *createDRBlist(gNB_RRC_UE_t *ue, bool reestablish)
{
  NR_DRB_ToAddMod_t *DRB_config = NULL;
  NR_DRB_ToAddModList_t *DRB_configList = CALLOC(sizeof(*DRB_configList), 1);

  for (int i = 0; i < MAX_DRBS_PER_UE; i++) {
    if (ue->established_drbs[i].status != DRB_INACTIVE) {
      DRB_config = generateDRB_ASN1(&ue->established_drbs[i]);
      if (reestablish) {
        asn1cCallocOne(DRB_config->reestablishPDCP, NR_DRB_ToAddMod__reestablishPDCP_true);
      }
      asn1cSeqAdd(&DRB_configList->list, DRB_config);
    }
  }
  if (DRB_configList->list.count == 0) {
    free(DRB_configList);
    return NULL;
  }
  return DRB_configList;
}

static void freeSRBlist(NR_SRB_ToAddModList_t *l)
{
  if (l) {
    for (int i = 0; i < l->list.count; i++)
      free(l->list.array[i]);
    free(l);
  } else
    LOG_E(NR_RRC, "Call free SRB list on NULL pointer\n");
}

static void activate_srb(gNB_RRC_UE_t *UE, int srb_id)
{
  AssertFatal(srb_id == 1 || srb_id == 2, "handling only SRB 1 or 2\n");
  if (UE->Srb[srb_id].Active == 1) {
    LOG_W(RRC, "UE %d SRB %d already activated\n", UE->rrc_ue_id, srb_id);
    return;
  }
  LOG_I(RRC, "activate SRB %d of UE %d\n", srb_id, UE->rrc_ue_id);
  UE->Srb[srb_id].Active = 1;

  NR_SRB_ToAddModList_t *list = CALLOC(sizeof(*list), 1);
  asn1cSequenceAdd(list->list, NR_SRB_ToAddMod_t, srb);
  srb->srb_Identity = srb_id;

  if (srb_id == 1) {
    nr_pdcp_entity_security_keys_and_algos_t null_security_parameters = {0};
  #ifdef UNI_RAN
    nr_pdcp_add_srbs(true, UE->rrc_ue_id, list, &null_security_parameters, false);
  #else
    nr_pdcp_add_srbs(true, UE->rrc_ue_id, list, &null_security_parameters);
  #endif
  } else {
    nr_pdcp_entity_security_keys_and_algos_t security_parameters;
    security_parameters.ciphering_algorithm = UE->ciphering_algorithm;
    security_parameters.integrity_algorithm = UE->integrity_algorithm;
    nr_derive_key(RRC_ENC_ALG, UE->ciphering_algorithm, UE->kgnb, security_parameters.ciphering_key);
    nr_derive_key(RRC_INT_ALG, UE->integrity_algorithm, UE->kgnb, security_parameters.integrity_key);

#ifdef UNI_RAN
    nr_pdcp_add_srbs(true,
                     UE->rrc_ue_id,
                     list,
                     &security_parameters, false);
#else
    nr_pdcp_add_srbs(true,
                     UE->rrc_ue_id,
                     list,
                     &security_parameters);
#endif
  }
  freeSRBlist(list);
}

//-----------------------------------------------------------------------------
static void rrc_gNB_generate_RRCSetup(instance_t instance,
                                      rnti_t rnti,
                                      rrc_gNB_ue_context_t *const ue_context_pP,
                                      const uint8_t *masterCellGroup,
                                      int masterCellGroup_len)
//-----------------------------------------------------------------------------
{
  LOG_UE_DL_EVENT(&ue_context_pP->ue_context, "Send RRC Setup\n");

  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  unsigned char buf[1024];
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(instance);
  ue_p->xids[xid] = RRC_SETUP;
  NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue_p, false);

  int size = do_RRCSetup(ue_context_pP, buf, xid, masterCellGroup, masterCellGroup_len, &rrc->configuration, SRBs);
  AssertFatal(size > 0, "do_RRCSetup failed\n");
  AssertFatal(size <= 1024, "memory corruption\n");

  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buf, size, "[MSG] RRC Setup\n");
  freeSRBlist(SRBs);
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {
    .gNB_CU_ue_id = ue_p->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .rrc_container = buf,
    .rrc_container_length = size,
    .srb_id = CCCH
  };
  rrc->mac_rrc.dl_rrc_message_transfer(ue_data.du_assoc_id, &dl_rrc);
}

static void rrc_gNB_generate_RRCReject(module_id_t module_id, rrc_gNB_ue_context_t *const ue_context_pP)
//-----------------------------------------------------------------------------
{
  gNB_RRC_INST *rrc = RC.nrrrc[module_id];
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  LOG_A(NR_RRC, "Send RRCReject to RNTI %04x\n", ue_p->rnti);

  unsigned char buf[1024];
  int size = do_RRCReject(buf);
  AssertFatal(size > 0, "do_RRCReject failed\n");
  AssertFatal(size <= 1024, "memory corruption\n");

  LOG_DUMPMSG(NR_RRC, DEBUG_RRC,
              (char *)buf,
              size,
              "[MSG] RRCReject \n");
  LOG_I(NR_RRC, " [RAPROC] ue %04x Logical Channel DL-CCCH, Generating NR_RRCReject (bytes %d)\n", ue_p->rnti, size);

  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_dl_rrc_message_t dl_rrc = {
    .gNB_CU_ue_id = ue_p->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .rrc_container = buf,
    .rrc_container_length = size,
    .srb_id = CCCH,
    .execute_duplication  = 1,
    .RAT_frequency_priority_information.en_dc = 0
  };
  rrc->mac_rrc.dl_rrc_message_transfer(ue_data.du_assoc_id, &dl_rrc);
}

//-----------------------------------------------------------------------------
/*
* Process the rrc setup complete message from UE (SRB1 Active)
*/
static void rrc_gNB_process_RRCSetupComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, NR_RRCSetupComplete_IEs_t *rrcSetupComplete)
//-----------------------------------------------------------------------------
{
  UE->Srb[1].Active = 1;
  UE->Srb[2].Active = 0;

#ifdef UNI_RAN
  UE->ue_rrc_inactivity_timer = 1;
  UE->ue_rrc_inactivity_threshold = rrc->inactivity_timer_threshold;
  UE->ue_rrc_srb_inactivity_timer = 1;

  for (int i = 0; i < NR_NUM_SRB; i++) {
    UE->last_txsdu_pkts_srb[i] = 0;
    UE->last_rxsdu_pkts_srb[i] = 0;
  }

  for (int i = 0; i < MAX_DRBS_PER_UE; i++) {
    UE->last_txsdu_pkts[i] = 0;
    UE->last_rxsdu_pkts[i] = 0;
  }
#endif
  rrc_gNB_send_NGAP_NAS_FIRST_REQ(rrc, UE, rrcSetupComplete);
}

void rrc_gNB_Handover_Required(gNB_RRC_INST *rrc,
                              gNB_RRC_UE_t *ue,
                              const nr_neighbour_gnb_configuration_t *neighbour)
{
      NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue, false);
      NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue, false);
      uint8_t rrc_buffer[NR_RRC_BUF_SIZE] = {0};

#ifdef UNI_RAN
      uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
      LOG_I(RRC, "HO: Assigned XID %d for UE %d\n", xid, ue->rrc_ue_id);

      for (int i = 0; i < ue->nb_of_pdusessions; i++) {
        LOG_D(RRC, "During HO REQ: UE->pdusession %d -> status = %d\n", i + 1, ue->pduSession[i].status);
        LOG_D(RRC, "During HO REQ: UE->pdusession %d -> cause type = %d\n", i + 1, ue->pduSession[i].cause.type);

        if (ue->pduSession[i].status == PDU_SESSION_STATUS_ESTABLISHED &&
            ue->pduSession[i].cause.type == NGAP_CAUSE_NOTHING) {
            ue->pduSession[i].xid = xid;
            LOG_D(RRC, "During HO REQ: Assigning XID %d to PDU session %d\n", xid, ue->pduSession[i].param.pdusession_id);
        }
      }
      int rrc_size = do_HO_RRCReconfiguration(ue,
                                   rrc_buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   SRBs,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   ue->measConfig,
                                   NULL,
                                   ue->masterCellGroup,
                                   false);
#else
      int rrc_size = do_HO_RRCReconfiguration(ue,
                                   rrc_buffer,
                                   NR_RRC_BUF_SIZE,
                                   0,
                                   SRBs,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   ue->measConfig,
                                   NULL,
                                   ue->masterCellGroup,
                                   false);
#endif
      freeSRBlist(SRBs);
      freeDRBlist(DRBs);
      DevAssert(rrc_size > 0 && rrc_size <= sizeof(rrc_buffer));
      uint8_t buf[NR_RRC_BUF_SIZE];
      int size = do_NR_HandoverPreparationInformation(ue->ue_cap_buffer.buf, ue->ue_cap_buffer.len, buf, sizeof buf,rrc_buffer,rrc_size);
      protocol_ctxt_t ctxt = {0};
      PROTOCOL_CTXT_SET_BY_INSTANCE(&ctxt, 0, GNB_FLAG_YES, ue->rrc_ue_id, 0, 0);
      rrc_gNB_send_NGAP_HANDOVER_REQUIRED(rrc, ue, neighbour, buf, size);
}

static int rrc_gNB_encode_RRCReconfiguration(gNB_RRC_INST *rrc,
                                             gNB_RRC_UE_t *UE,
                                             uint8_t xid,
                                             struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *nas_messages,
                                             uint8_t *buf,
                                             int max_len,
                                             bool reestablish)
{
  NR_CellGroupConfig_t *cellGroupConfig = UE->masterCellGroup;
  nr_rrc_du_container_t *du = get_du_for_ue(rrc, UE->rrc_ue_id);
  DevAssert(du != NULL);
  f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  NR_MeasConfig_t *measconfig = NULL;
  if (du->mtc != NULL) {
    int scs = get_ssb_scs(cell_info);
    int band = get_dl_band(cell_info);
    const NR_MeasTimingList_t *mtlist = du->mtc->criticalExtensions.choice.c1->choice.measTimingConf->measTiming;
    const NR_MeasTiming_t *mt = mtlist->list.array[0];
    const neighbour_cell_configuration_t *neighbour_config = get_neighbour_config(cell_info->nr_cellid);
    seq_arr_t *neighbour_cells = NULL;
    if (neighbour_config)
      neighbour_cells = neighbour_config->neighbour_cells;

    measconfig = get_MeasConfig(mt, band, scs, &rrc->measurementConfiguration, neighbour_cells);
  }

  if (UE->measConfig)
    free_MeasConfig(UE->measConfig);

  UE->measConfig = measconfig;

  NR_SRB_ToAddModList_t *SRBs = createSRBlist(UE, reestablish);
  NR_DRB_ToAddModList_t *DRBs = createDRBlist(UE, reestablish);

  int size = do_RRCReconfiguration(UE,
                                   buf,
                                   max_len,
                                   xid,
                                   SRBs,
                                   DRBs,
                                   UE->DRB_ReleaseList,
                                   NULL,
                                   measconfig,
                                   nas_messages,
                                   cellGroupConfig);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buf, size, "[MSG] RRC Reconfiguration\n");
  freeSRBlist(SRBs);
  freeDRBlist(DRBs);
  ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToReleaseList, UE->DRB_ReleaseList);
  UE->DRB_ReleaseList = NULL;

  return size;
}

static void rrc_gNB_generate_dedicatedRRCReconfiguration(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[xid] = RRC_PDUSESSION_ESTABLISH;
  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList = CALLOC(1, sizeof(*dedicatedNAS_MessageList));

  for (int i = 0; i < ue_p->nb_of_pdusessions; i++) {
    if (ue_p->pduSession[i].param.nas_pdu.buffer != NULL) {
      asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, msg);
      OCTET_STRING_fromBuf(msg,
                           (char *)ue_p->pduSession[i].param.nas_pdu.buffer,
                           ue_p->pduSession[i].param.nas_pdu.length);

      LOG_D(NR_RRC, "add NAS info with size %d (pdusession idx %d)\n", ue_p->pduSession[i].param.nas_pdu.length, i);
      ue_p->pduSession[i].xid = xid;
    }
    if (ue_p->pduSession[i].status == PDU_SESSION_STATUS_NEW || ue_p->pduSession[i].status == PDU_SESSION_STATUS_RELEASED) {
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_DONE;
    }
  }

  if (ue_p->nas_pdu.length) {
    asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, msg);
    OCTET_STRING_fromBuf(msg, (char *)ue_p->nas_pdu.buffer, ue_p->nas_pdu.length);
  }

  /* If list is empty free the list and reset the address */
  if (dedicatedNAS_MessageList->list.count == 0) {
    free(dedicatedNAS_MessageList);
    dedicatedNAS_MessageList = NULL;
  }

  /* Free all NAS PDUs */
  for (int i = 0; i < ue_p->nb_of_pdusessions; i++)
    clear_nas_pdu(&ue_p->pduSession[i].param.nas_pdu);

  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  // TODO refactor dedicatedNAS_MessageList
  int size = rrc_gNB_encode_RRCReconfiguration(rrc, ue_p, xid, dedicatedNAS_MessageList, buffer, sizeof(buffer), false);
  DevAssert(size > 0 && size <= sizeof(buffer));

  LOG_UE_DL_EVENT(ue_p, "Generate RRCReconfiguration (bytes %d, xid %d)\n", size, xid);

  uint8_t SRB = ue_p->ho_context_xn ? DL_SCH_LCID_DCCH1 : DL_SCH_LCID_DCCH; /* Added Apr 25 */

  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, SRB, buffer, size);
}

void rrc_gNB_modify_dedicatedRRCReconfiguration(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  int qos_flow_index = 0;
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[xid] = RRC_PDUSESSION_MODIFY;

  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList =
      CALLOC(1, sizeof(*dedicatedNAS_MessageList));

  for (int i = 0; i < ue_p->nb_of_pdusessions; i++) {
    // bypass the new and already configured pdu sessions
    if (ue_p->pduSession[i].status >= PDU_SESSION_STATUS_DONE) {
      ue_p->pduSession[i].xid = xid;
      continue;
    }

    if (ue_p->pduSession[i].cause.type != NGAP_CAUSE_NOTHING) {
      // set xid of failure pdu session
      ue_p->pduSession[i].xid = xid;
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
      continue;
    }

    // search exist DRB_config
    int j;
    for (j = 0; i < MAX_DRBS_PER_UE; j++) {
      if (ue_p->established_drbs[j].status != DRB_INACTIVE
          && ue_p->established_drbs[j].cnAssociation.sdap_config.pdusession_id == ue_p->pduSession[i].param.pdusession_id)
        break;
    }

    if (j == MAX_DRBS_PER_UE) {
      ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CauseRadioNetwork_unspecified};
      ue_p->pduSession[i].xid = xid;
      ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
      ue_p->pduSession[i].cause = cause;
      continue;
    }

    // Reference TS23501 Table 5.7.4-1: Standardized 5QI to QoS characteristics mapping
    for (qos_flow_index = 0; qos_flow_index < ue_p->pduSession[i].param.nb_qos; qos_flow_index++) {
      switch (ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI) {
        case 1: //100ms
#ifdef UNI_RAN
          LOG_I(NR_RRC, "Applied 5QI = 1 on PDU session = %d\n", ue_p->pduSession[i].param.pdusession_id);
          break;
#endif
        case 2: //150ms
        case 3: //50ms
        case 4: //300ms
        case 5: //100ms
        case 6: //300ms
        case 7: //100ms
        case 8: //300ms
        case 9: //300ms Video (Buffered Streaming)TCP-based (e.g., www, e-mail, chat, ftp, p2p file sharing, progressive video, etc.)
          // TODO
          break;

        default:
          LOG_E(NR_RRC, "not supported 5qi %lu\n", ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI);
          ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CauseRadioNetwork_not_supported_5QI_value};
          ue_p->pduSession[i].status = PDU_SESSION_STATUS_FAILED;
          ue_p->pduSession[i].xid = xid;
          ue_p->pduSession[i].cause = cause;
          continue;
      }
        LOG_I(NR_RRC,
              "index %d, QOS flow %d, 5QI %ld \n",
              i,
              qos_flow_index,
              ue_p->pduSession[i].param.qos[qos_flow_index].fiveQI);
    }

    ue_p->pduSession[i].status = PDU_SESSION_STATUS_DONE;
    ue_p->pduSession[i].xid = xid;

    if (ue_p->pduSession[i].param.nas_pdu.buffer != NULL) {
      asn1cSequenceAdd(dedicatedNAS_MessageList->list,NR_DedicatedNAS_Message_t, dedicatedNAS_Message);
      OCTET_STRING_fromBuf(dedicatedNAS_Message, (char *)ue_p->pduSession[i].param.nas_pdu.buffer, ue_p->pduSession[i].param.nas_pdu.length);
      LOG_I(NR_RRC, "add NAS info with size %d (pdusession id %d)\n", ue_p->pduSession[i].param.nas_pdu.length, ue_p->pduSession[i].param.pdusession_id);
    }
  }

  /* If list is empty free the list and reset the address */
  if (dedicatedNAS_MessageList->list.count == 0) {
    free(dedicatedNAS_MessageList);
    dedicatedNAS_MessageList = NULL;
  }

  NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue_p, false);
  uint8_t buffer[NR_RRC_BUF_SIZE];
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   NULL,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   NULL,
                                   dedicatedNAS_MessageList,
                                   NULL);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");
  freeDRBlist(DRBs);

  /* Free all NAS PDUs */
  for (int i = 0; i < ue_p->nb_of_pdusessions; i++)
    clear_nas_pdu(&ue_p->pduSession[i].param.nas_pdu);

  LOG_I(NR_RRC, "UE %d: Generate RRCReconfiguration (bytes %d)\n", ue_p->rrc_ue_id, size);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DL_SCH_LCID_DCCH, buffer, size);
}

typedef struct deliver_ue_ctxt_modification_data_t {
  gNB_RRC_INST *rrc;
  f1ap_ue_context_modif_req_t *modification_req;
  sctp_assoc_t assoc_id;
} deliver_ue_ctxt_modification_data_t;

static void rrc_deliver_ue_ctxt_modif_req(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_ue_ctxt_modification_data_t *data = deliver_pdu_data;
  data->modification_req->rrc_container = (uint8_t *)buf;
  data->modification_req->rrc_container_length = size;
  data->rrc->mac_rrc.ue_context_modification_request(data->assoc_id, data->modification_req);
}
void rrc_gNB_generate_dedicatedRRCReconfiguration_release(gNB_RRC_INST *rrc,
                                                          gNB_RRC_UE_t *ue_p,
                                                          uint8_t xid,
                                                          uint32_t nas_length,
                                                          uint8_t *nas_buffer)
{
  f1ap_drb_to_be_released_t rel_drbs[32];
  int n_rel_drbs = 0;

  if (!ue_p->DRB_ReleaseList)
    ue_p->DRB_ReleaseList = calloc(1, sizeof(*ue_p->DRB_ReleaseList));

  for (int i = 0; i < NB_RB_MAX; i++) {
    if ((ue_p->pduSession[i].status == PDU_SESSION_STATUS_TORELEASE) && ue_p->pduSession[i].xid == xid) {
      asn1cSequenceAdd(ue_p->DRB_ReleaseList->list, NR_DRB_Identity_t, DRB_release);
      *DRB_release = i + 1;
      rel_drbs[n_rel_drbs].rb_id = *DRB_release;
      n_rel_drbs++;
      memset(&ue_p->established_drbs[i], 0, sizeof(drb_t));
    }
  }

  /* If list is empty free the list and reset the address */
  struct NR_RRCReconfiguration_v1530_IEs__dedicatedNAS_MessageList *dedicatedNAS_MessageList = NULL;
  if (nas_length > 0) {
    dedicatedNAS_MessageList = CALLOC(1, sizeof(*dedicatedNAS_MessageList));
    asn1cSequenceAdd(dedicatedNAS_MessageList->list, NR_DedicatedNAS_Message_t, dedicatedNAS_Message);
    OCTET_STRING_fromBuf(dedicatedNAS_Message, (char *)nas_buffer, nas_length);
    LOG_I(NR_RRC, "add NAS info with size %d\n", nas_length);
  } else {
    LOG_W(NR_RRC, "dedlicated NAS list is empty\n");
  }

  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   xid,
                                   NULL,
                                   NULL,
                                   ue_p->DRB_ReleaseList,
                                   NULL,
                                   NULL,
                                   dedicatedNAS_MessageList,
                                   NULL);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");

  /* Free all NAS PDUs */
  if (nas_length > 0) {
    /* Free the NAS PDU buffer and invalidate it */
    free(nas_buffer);
  }

  LOG_I(NR_RRC, "UE %d: Generate NR_RRCReconfiguration (bytes %d)\n", ue_p->rrc_ue_id, size);

  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  f1ap_ue_context_modif_req_t ue_context_modif_req = {
      .gNB_CU_ue_id = ue_p->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid,
      .servCellId = 0,
      .drbs_to_be_released_length = n_rel_drbs,
      .drbs_to_be_released = (f1ap_drb_to_be_released_t *)rel_drbs,
  };
  deliver_ue_ctxt_modification_data_t data = {.rrc = rrc,
                                              .modification_req = &ue_context_modif_req,
                                              .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(ue_p->rrc_ue_id,
                       DL_SCH_LCID_DCCH,
                       rrc_gNB_mui++,
                       size,
                       (unsigned char *const)buffer,
                       rrc_deliver_ue_ctxt_modif_req,
                       &data);
}

static void fill_security_info(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, security_information_t *secInfo)
{
  secInfo->cipheringAlgorithm = rrc->security.do_drb_ciphering ? UE->ciphering_algorithm : 0;
  secInfo->integrityProtectionAlgorithm = rrc->security.do_drb_integrity ? UE->integrity_algorithm : 0;
  nr_derive_key(UP_ENC_ALG, secInfo->cipheringAlgorithm, UE->kgnb, (uint8_t *)secInfo->encryptionKey);
  nr_derive_key(UP_INT_ALG, secInfo->integrityProtectionAlgorithm, UE->kgnb, (uint8_t *)secInfo->integrityProtectionKey);
}

/* \brief find existing PDU session inside E1AP Bearer Modif message, or
 * point to new one.
 * \param bearer_modif E1AP Bearer Modification Message
 * \param pdu_id PDU session ID
 * \return pointer to existing PDU session, or to new/unused one. */
static pdu_session_to_mod_t *find_or_next_pdu_session(e1ap_bearer_mod_req_t *bearer_modif, int pdu_id)
{
  for (int i = 0; i < bearer_modif->numPDUSessionsMod; ++i) {
    if (bearer_modif->pduSessionMod[i].sessionId == pdu_id)
      return &bearer_modif->pduSessionMod[i];
  }
  /* E1AP Bearer Modification has no PDU session to modify with that ID, create
   * new entry */
  DevAssert(bearer_modif->numPDUSessionsMod < E1AP_MAX_NUM_PDU_SESSIONS - 1);
  bearer_modif->numPDUSessionsMod += 1;
  return &bearer_modif->pduSessionMod[bearer_modif->numPDUSessionsMod - 1];
}

/**
 * @brief Notify E1 re-establishment to CU-UP
 */
static void cuup_notify_reestablishment(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
  e1ap_bearer_mod_req_t req = {
      .gNB_cu_cp_ue_id = ue_p->rrc_ue_id,
      .gNB_cu_up_ue_id = ue_p->rrc_ue_id,
  };
  // Quit re-establishment notification if no CU-UP is associated
  if (!is_cuup_associated(rrc)) {
    return;
  }
  if (!ue_associated_to_cuup(rrc, ue_p))
    return;
  /* loop through active DRBs */
  for (int drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    drb_t *drb = get_drb(ue_p, drb_id);
    if (drb->status == DRB_INACTIVE)
      continue;
    /* fetch an existing PDU session for this DRB */
    rrc_pdu_session_param_t *pdu = find_pduSession_from_drbId(ue_p, drb_id);
    if (pdu == NULL) {
      LOG_E(RRC, "UE %d: E1 Bearer Context Modification: no PDU session for DRB ID %d\n", ue_p->rrc_ue_id, drb_id);
      continue;
    }
    /* Get pointer to existing (or new one) PDU session to modify in E1 */
    pdu_session_to_mod_t *pdu_e1 = find_or_next_pdu_session(&req, pdu->param.pdusession_id);
    AssertError(pdu != NULL,
                continue,
                "UE %u: E1 Bearer Context Modification: PDU session %d to setup is null\n",
                ue_p->rrc_ue_id,
                pdu->param.pdusession_id);
    /* Prepare PDU for E1 Bearear Context Modification Request */
    pdu_e1->sessionId = pdu->param.pdusession_id;
    /* Fill DRB to setup with ID, DL TL and DL TEID */
    DRB_nGRAN_to_mod_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    drb_e1->id = drb_id;
    drb_e1->numDlUpParam = 1;
    memcpy(&drb_e1->DlUpParamList[0].tl_info.tlAddress, &drb->du_tunnel_config.addr.buffer, sizeof(uint8_t) * 4);
    drb_e1->DlUpParamList[0].tl_info.teId = drb->du_tunnel_config.teid;
    /* PDCP configuration */
    if (!drb_e1->pdcp_config)
      drb_e1->pdcp_config = malloc_or_fail(sizeof(*drb_e1->pdcp_config));
    bearer_context_pdcp_config_t *pdcp_config = drb_e1->pdcp_config;
    set_bearer_context_pdcp_config(pdcp_config, drb, rrc->configuration.um_on_default_drb);
    pdcp_config->pDCP_Reestablishment = true;

    
    /* Xn code for E1AP SN Status transfer procedures*/
    drb_e1->pdcp_sn_status_requested = false;
    /* Xn code for E1AP SN Status transfer procedures*/

    /* increase DRB to modify counter */
    pdu_e1->numDRB2Modify += 1;
  }

#if 0
  /* According to current understanding of E1 specifications, it is not needed
   * to send security information because this does not change.
   * But let's keep the code here in case it's needed.
   */
  // Always send security information
  req.secInfo = malloc_or_fail(sizeof(*req.secInfo));
  fill_security_info(rrc, ue_p, req.secInfo);
#endif

  /* Send E1 Bearer Context Modification Request (3GPP TS 38.463) */
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, ue_p);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
}


/* @brief Requesting pdcp sn status after receiving HO ack from target during Xn handover calls */

static void cuup_request_pdcp_sn_status(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
{
printf("1st line inside cuup_request_pdcp_sn_status called in processing HO ack\n");
e1ap_bearer_mod_req_t req = {
      .gNB_cu_cp_ue_id = ue_p->rrc_ue_id,
      .gNB_cu_up_ue_id = ue_p->rrc_ue_id,
  };
  if (!ue_associated_to_cuup(rrc, ue_p))
    return;
  /* loop through active DRBs */
  for (int drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    drb_t *drb = get_drb(ue_p, drb_id);
    if (drb->status == DRB_INACTIVE)
      continue;
    /* fetch an existing PDU session for this DRB */
    rrc_pdu_session_param_t *pdu = find_pduSession_from_drbId(ue_p, drb_id);
    if (pdu == NULL) {
      LOG_E(RRC, "UE %d: E1 Bearer Context Modification: no PDU session for DRB ID %d\n", ue_p->rrc_ue_id, drb_id);
      continue;
    }
    /* Get pointer to existing (or new one) PDU session to modify in E1 */
    pdu_session_to_mod_t *pdu_e1 = find_or_next_pdu_session(&req, pdu->param.pdusession_id);
    AssertError(pdu != NULL,
                continue,
                "UE %u: E1 Bearer Context Modification: PDU session %d to setup is null\n",
                ue_p->rrc_ue_id,
                pdu->param.pdusession_id);
    /* Prepare PDU for E1 Bearear Context Modification Request */
    pdu_e1->sessionId = pdu->param.pdusession_id;
    /* Fill DRB to setup with ID, DL TL and DL TEID */
    DRB_nGRAN_to_mod_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    drb_e1->id = drb_id;
    /*drb_e1->numDlUpParam = 1;
    memcpy(&drb_e1->DlUpParamList[0].tlAddress, &drb->du_tunnel_config.addr.buffer, sizeof(uint8_t) * 4);
    drb_e1->DlUpParamList[0].teId = drb->du_tunnel_config.teid; */
    /* PDCP configuration */
    /* bearer_context_pdcp_config_t *pdcp_config = &drb_e1->pdcp_config;
    set_bearer_context_pdcp_config(pdcp_config, drb, rrc->configuration.um_on_default_drb);
    pdcp_config->pDCP_Reestablishment = true; */

    /* Xn code for E1AP SN Status transfer procedures*/
    drb_e1->pdcp_sn_status_requested = true;
    /* Xn code for E1AP SN Status transfer procedures*/

    /* increase DRB to modify counter */
    pdu_e1->numDRB2Modify += 1;
    printf("exiting loop through DRBs in cuup_request_pdcp_sn_status\n");
  }

  /* Send E1 Bearer Context Modification Request (3GPP TS 38.463) */
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, ue_p);
printf ("assoc_id is %d\n", assoc_id);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
    printf("last line cuup_request_pdcp_sn_status after calling rrc->cucp_cuup.bearer_context_mod\n");
}


/* @brief Requesting pdcp sn status after receiving HO ack from target during Xn handover calls */

static void cuup_info_pdcp_sn_status(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, drbs_subject_to_transfer_list_t *drb_list) /* Feb 23 2025 */
{
printf("1st line in cuup_info_pdcp_sn_status \n");
e1ap_bearer_mod_req_t req = {
      .gNB_cu_cp_ue_id = ue_p->rrc_ue_id,
      .gNB_cu_up_ue_id = ue_p->rrc_ue_id,
  };
  /* if (!ue_associated_to_cuup(rrc, ue_p))
    return; */
  /* loop through active DRBs */
  for (int drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    drb_t *drb = get_drb(ue_p, drb_id);
    if (drb->status == DRB_INACTIVE)
      continue;
    /* fetch an existing PDU session for this DRB */
    rrc_pdu_session_param_t *pdu = find_pduSession_from_drbId(ue_p, drb_id);
    if (pdu == NULL) {
      LOG_E(RRC, "UE %d: E1 Bearer Context Modification: no PDU session for DRB ID %d\n", ue_p->rrc_ue_id, drb_id);
      continue;
    }
    /* Get pointer to existing (or new one) PDU session to modify in E1 */
    pdu_session_to_mod_t *pdu_e1 = find_or_next_pdu_session(&req, pdu->param.pdusession_id);
    AssertError(pdu != NULL,
                continue,
                "UE %u: E1 Bearer Context Modification: PDU session %d to setup is null\n",
                ue_p->rrc_ue_id,
                pdu->param.pdusession_id);
    /* Prepare PDU for E1 Bearear Context Modification Request */
    pdu_e1->sessionId = pdu->param.pdusession_id;
    /* Fill DRB to setup with ID, DL TL and DL TEID */
    DRB_nGRAN_to_mod_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    drb_e1->id = drb_id;
    /*drb_e1->numDlUpParam = 1;
    memcpy(&drb_e1->DlUpParamList[0].tlAddress, &drb->du_tunnel_config.addr.buffer, sizeof(uint8_t) * 4);
    drb_e1->DlUpParamList[0].teId = drb->du_tunnel_config.teid; */
    /* PDCP configuration */
    /* bearer_context_pdcp_config_t *pdcp_config = &drb_e1->pdcp_config;
    set_bearer_context_pdcp_config(pdcp_config, drb, rrc->configuration.um_on_default_drb);
    pdcp_config->pDCP_Reestablishment = true; */

    /* Xn code for E1AP SN Status transfer procedures*/
    drb_e1->pdcp_sn_status_requested = false;
    /* Xn code for E1AP SN Status transfer procedures*/

    /* Xn code for E1AP SN status transfer procedures */
    // bearer_context_pdcp_sn_status_info_t pdcp_sn_status_info = drb_e1->pdcp_sn_status_info;

    /* Feb 23 2025 */
    /* 12bit */
    /*
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.pdcp_sn = drb_list->drb[0].pdcp_status_ul.pdcp_sn_12bit.count_value.pdcp_sn12;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.hfn = drb_list->drb[0].pdcp_status_ul.pdcp_sn_12bit.count_value.hfn_pdcp_sn12;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_dl.pdcp_sn = drb_list->drb[0].pdcp_status_dl.pdcp_sn_12bit.count_value.pdcp_sn12;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_dl.hfn = drb_list->drb[0].pdcp_status_dl.pdcp_sn_12bit.count_value.hfn_pdcp_sn12;
    */
    /* 12bit */

    /* Apr 10 2025 */
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.pdcp_sn = drb_list->drb[0].pdcp_status_ul.pdcp_sn_18bit.count_value.pdcp_sn18;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.hfn = drb_list->drb[0].pdcp_status_ul.pdcp_sn_18bit.count_value.hfn_pdcp_sn18;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_dl.pdcp_sn = drb_list->drb[0].pdcp_status_dl.pdcp_sn_18bit.count_value.pdcp_sn18;
    drb_e1->pdcp_sn_status_info.pdcp_status_transfer_dl.hfn = drb_list->drb[0].pdcp_status_dl.pdcp_sn_18bit.count_value.hfn_pdcp_sn18;

    /* increase DRB to modify counter */
    pdu_e1->numDRB2Modify += 1;
printf("exiting loop through DRBs in cuup_info_pdcp_sn_status\n");
  }

  /* Send E1 Bearer Context Modification Request (3GPP TS 38.463) */
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, ue_p);
printf("assoc_id is %d \n", assoc_id);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
printf("last line after rrc->cucp_cuup.bearer_context_mod in cuup_info_pdcp_sn_status \n");

}

#ifdef UNI_RAN
/** @brief Send NG Uplink RAN Status Transfer message (8.4.6 3GPP TS 38.413)
 * Direction: source NG-RAN node -> AMF */
int rrc_gNB_send_NGAP_ul_ran_status_transfer(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int nDRBmod, int *drb_id_list, e1_pdcp_status_info_t *pdcp_status_list)
{
  AssertFatal(UE != NULL, "UE context is NULL\n");
  AssertFatal(pdcp_status_list != NULL, "pdcp_status_list is NULL\n");

  LOG_I(NR_RRC,
        "Sending NGAP Uplink RAN Status Transfer (AMF_UE_NGAP_ID=%" PRIu64 ", GNB_UE_NGAP_ID=%u)\n",
        UE->amf_ue_ngap_id,
        UE->rrc_ue_id);

  ngap_ran_status_transfer_t msg = {0};
  msg.amf_ue_ngap_id = UE->amf_ue_ngap_id;
  msg.gnb_ue_ngap_id = UE->rrc_ue_id;
  msg.ran_status.nb_drb = 0;

  // Loop through DRBs and extract COUNT values
  for (int idx = 0; idx < nDRBmod; idx++) {
    int drb_id = drb_id_list[idx];
    const e1_pdcp_status_info_t *pdcp_status = &pdcp_status_list[idx];

    bool sn_length_18 = rrc->pdcp_config.drb.sn_size == 18;

    ngap_drb_status_t *item = &msg.ran_status.drb_status_list[msg.ran_status.nb_drb++];
    item->drb_id = drb_id;

    e1_pdcp_count_t *ul_pdcp = &pdcp_status->ul_count;
    e1_pdcp_count_t *dl_pdcp = &pdcp_status->dl_count;

    item->ul_count.pdcp_sn = ul_pdcp->sn;
    item->ul_count.hfn = ul_pdcp->hfn;
    item->ul_count.sn_len = sn_length_18 ? NGAP_SN_LENGTH_18 : NGAP_SN_LENGTH_12;

    item->dl_count.pdcp_sn = dl_pdcp->sn;

#ifdef UNI_RAN
    /* N2 HO Proprietary Change: interop TBD */
    if (idx == 0)
       item->dl_count.pdcp_sn = rrc->dl_srb_count_source + 1;
#endif

    item->dl_count.hfn = dl_pdcp->hfn;
    item->dl_count.sn_len = sn_length_18 ? NGAP_SN_LENGTH_18 : NGAP_SN_LENGTH_12;
  }

  MessageDef *msg_p = itti_alloc_new_message(TASK_RRC_GNB, rrc->module_id, NGAP_UL_RAN_STATUS_TRANSFER);
  NGAP_UL_RAN_STATUS_TRANSFER(msg_p) = msg;
  itti_send_msg_to_task(TASK_NGAP, rrc->module_id, msg_p);

  return 0;
}
#endif

static void rrc_gNB_process_HandoverCommand(MessageDef* msg_p, instance_t instance)
{
  LOG_I(NR_RRC, "Handover Command has came to the source gNB RRC!\n");
  uint32_t gNB_ue_ngap_id = 0;
  ngap_handover_command_t* handover_command_msg = &NGAP_HANDOVER_COMMAND(msg_p);
  gNB_ue_ngap_id = handover_command_msg->gNB_ue_ngap_id;

  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, gNB_ue_ngap_id);
  

  if (ue_context_p == NULL) {
    /* Can not associate this message to an UE index */
    LOG_W(NR_RRC, "[gNB %ld] In HANDOVER_COMMAND: unknown UE from gNB_ue_ngap_id (%u)\n",
          instance,
          gNB_ue_ngap_id);
    return;
  }
  gNB_RRC_UE_t* UE = &ue_context_p->ue_context;

  if (UE->ho_context == NULL) {
    LOG_E(NR_RRC, "handover for UE %u aborted , UE HO context does not exist \n",UE->rrc_ue_id);
    return;
  }

  /* if (UE->StatusRrc != NR_RRC_HO_PREPARATION)
  {
    LOG_E(NR_RRC, "HO LOG: UE is not in HO Status!\n");
    return ;
  } */

  NR_HandoverCommand_t* hoCommand = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                   &asn_DEF_NR_HandoverCommand,
                                                   (void **)&hoCommand,
                                                   (uint8_t *)handover_command_msg->handoverCommand.buffer,
                                                   handover_command_msg->handoverCommand.length);
  if (dec_rval.code != RC_OK && dec_rval.consumed == 0) {
    LOG_E(NR_RRC, "Can not decode Handover Command!\n");
    return;
  }
  //ue_context_p->ue_context.StatusRrc = NR_RRC_HO_EXECUTION;
  uint8_t* buffer = (uint8_t*)calloc(1, NR_RRC_BUF_SIZE);
  int encoded_size = prepare_DL_DCCH_for_HO_Command(&ue_context_p->ue_context, &(hoCommand->criticalExtensions.choice.c1->choice.handoverCommand->handoverCommandMessage.buf),
                                                     hoCommand->criticalExtensions.choice.c1->choice.handoverCommand->handoverCommandMessage.size, buffer, NR_RRC_BUF_SIZE);
  DevAssert(encoded_size > 0);
  
  rrc_gNB_trigger_reconfiguration_for_handover(rrc, UE, buffer, encoded_size);
  LOG_A(NR_RRC, "HO acknowledged: Send reconfiguration for UE %u/RNTI %04x...\n", UE->rrc_ue_id, UE->rnti);
}

/**
 * @brief RRCReestablishment message
 *        Direction: Network to UE
 */
static void rrc_gNB_generate_RRCReestablishment(rrc_gNB_ue_context_t *ue_context_pP,
                                                const uint8_t *masterCellGroup_from_DU,
                                                const rnti_t old_rnti,
                                                const nr_rrc_du_container_t *du)
{
  module_id_t module_id = 0;
  gNB_RRC_INST *rrc = RC.nrrrc[module_id];
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(module_id);
  ue_p->xids[xid] = RRC_REESTABLISH;
  const f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  uint32_t ssb_arfcn = get_ssb_arfcn(du);
  int size = do_RRCReestablishment(ue_context_pP, buffer, NR_RRC_BUF_SIZE, xid, cell_info->nr_pci, ssb_arfcn);

  LOG_A(NR_RRC, "Send RRCReestablishment [%d bytes] to RNTI %04x\n", size, ue_p->rnti);

  /* Ciphering and Integrity according to TS 33.501 */
  nr_pdcp_entity_security_keys_and_algos_t security_parameters;
  /* Derive the keys from kgnb */
  nr_derive_key(RRC_ENC_ALG, ue_p->ciphering_algorithm, ue_p->kgnb, security_parameters.ciphering_key);
  nr_derive_key(RRC_INT_ALG, ue_p->integrity_algorithm, ue_p->kgnb, security_parameters.integrity_key);
  LOG_I(NR_RRC,
        "Set PDCP security UE %d RNTI %04x nea %ld nia %d in RRCReestablishment\n",
        ue_p->rrc_ue_id,
        ue_p->rnti,
        ue_p->ciphering_algorithm,
        ue_p->integrity_algorithm);
  /* RRCReestablishment is integrity protected but not ciphered,
   * so let's configure only integrity protection right now.
   * Ciphering is enabled below, after generating RRCReestablishment.
   */
  security_parameters.integrity_algorithm = ue_p->integrity_algorithm;
  security_parameters.ciphering_algorithm = 0;

  /* SRBs */
  for (int srb_id = 1; srb_id < NR_NUM_SRB; srb_id++) {
    if (ue_p->Srb[srb_id].Active)
      nr_pdcp_config_set_security(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
  /* Re-establish PDCP for SRB1, according to 5.3.7.4 of 3GPP TS 38.331 */
  nr_pdcp_reestablishment(ue_p->rrc_ue_id,
                          1,
                          true,
                          &security_parameters);
  /* F1AP DL RRC Message Transfer */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  uint32_t old_gNB_DU_ue_id = old_rnti;
  f1ap_dl_rrc_message_t dl_rrc = {.gNB_CU_ue_id = ue_p->rrc_ue_id,
                                  .gNB_DU_ue_id = ue_data.secondary_ue,
                                  .srb_id = DL_SCH_LCID_DCCH,
                                  .old_gNB_DU_ue_id = &old_gNB_DU_ue_id};
  deliver_dl_rrc_message_data_t data = {.rrc = rrc, .dl_rrc = &dl_rrc, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(ue_p->rrc_ue_id, DL_SCH_LCID_DCCH, rrc_gNB_mui++, size, (unsigned char *const)buffer, rrc_deliver_dl_rrc_message, &data);

  /* RRCReestablishment has been generated, let's enable ciphering now. */
  security_parameters.ciphering_algorithm = ue_p->ciphering_algorithm;
  /* SRBs */
  for (int srb_id = 1; srb_id < NR_NUM_SRB; srb_id++) {
    if (ue_p->Srb[srb_id].Active)
      nr_pdcp_config_set_security(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
}

/// @brief Function tha processes RRCReestablishmentComplete message sent by the UE, after RRCReestasblishment request.
static void rrc_gNB_process_RRCReestablishmentComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, const uint8_t xid)
{
  LOG_I(NR_RRC, "UE %d Processing NR_RRCReestablishmentComplete from UE\n", ue_p->rrc_ue_id);

  int i = 0;

  ue_p->xids[xid] = RRC_ACTION_NONE;

  ue_p->Srb[1].Active = 1;

#ifdef UNI_RAN
  ue_p->ue_rrc_inactivity_timer = 1;
  ue_p->ue_rrc_inactivity_threshold = rrc->inactivity_timer_threshold;

  for (int i = 1; i < NR_NUM_SRB; i++) {
    nr_pdcp_statistics_t stats = {0};
    bool rc = nr_pdcp_get_statistics(ue_p->rrc_ue_id, 1, i, &stats);
    if (!rc) {
      ue_p->last_txsdu_pkts_srb[i] = 0;
      ue_p->last_rxsdu_pkts_srb[i] = 0;
    }
    else {
    ue_p->last_txsdu_pkts_srb[i] = stats.txsdu_pkts;
    ue_p->last_rxsdu_pkts_srb[i] = stats.rxsdu_pkts;
    }
  }

  for (int i = 1; i < MAX_DRBS_PER_UE; i++) {
    nr_pdcp_statistics_t stats = {0};
    bool rc = nr_pdcp_get_statistics(ue_p->rrc_ue_id, 0, i, &stats);
    if (!rc) {
      ue_p->last_txsdu_pkts[i] = 0;
      ue_p->last_rxsdu_pkts[i] = 0;
    }
    else {
    ue_p->last_txsdu_pkts[i] = stats.txsdu_pkts;
    ue_p->last_rxsdu_pkts[i] = stats.rxsdu_pkts;
    }
  }
#endif
  NR_CellGroupConfig_t *cellGroupConfig = calloc(1, sizeof(NR_CellGroupConfig_t));

  cellGroupConfig->spCellConfig = ue_p->masterCellGroup->spCellConfig;
  cellGroupConfig->mac_CellGroupConfig = ue_p->masterCellGroup->mac_CellGroupConfig;
  cellGroupConfig->physicalCellGroupConfig = ue_p->masterCellGroup->physicalCellGroupConfig;
  cellGroupConfig->rlc_BearerToReleaseList = NULL;
  cellGroupConfig->rlc_BearerToAddModList = calloc(1, sizeof(*cellGroupConfig->rlc_BearerToAddModList));

  /*
   * Get SRB2, DRB configuration from the existing UE context,
   * also start from SRB2 (i=1) and not from SRB1 (i=0).
   */
  for (i = 1; i < ue_p->masterCellGroup->rlc_BearerToAddModList->list.count; ++i)
    asn1cSeqAdd(&cellGroupConfig->rlc_BearerToAddModList->list, ue_p->masterCellGroup->rlc_BearerToAddModList->list.array[i]);

  /*
   * At this stage, PDCP entity are re-established and reestablishRLC is flagged
   * with RRCReconfiguration to complete RLC re-establishment of remaining bearers
   */
  for (i = 0; i < cellGroupConfig->rlc_BearerToAddModList->list.count; i++) {
    asn1cCallocOne(cellGroupConfig->rlc_BearerToAddModList->list.array[i]->reestablishRLC,
                   NR_RLC_BearerConfig__reestablishRLC_true);
  }

  /* TODO: remove a reconfigurationWithSync, we don't need it for
   * reestablishment. The whole reason why this might be here is that we store
   * the CellGroupConfig (after handover), and simply reuse it for
   * reestablishment, instead of re-requesting the CellGroupConfig from the DU.
   * Hence, add below hack; the solution would be to request the
   * CellGroupConfig from the DU when doing reestablishment. */
  if (cellGroupConfig->spCellConfig && cellGroupConfig->spCellConfig->reconfigurationWithSync) {
    ASN_STRUCT_FREE(asn_DEF_NR_ReconfigurationWithSync, cellGroupConfig->spCellConfig->reconfigurationWithSync);
    cellGroupConfig->spCellConfig->reconfigurationWithSync = NULL;
  }

  /* Re-establish SRB2 according to clause 5.3.5.6.3 of 3GPP TS 38.331
   * (SRB1 is re-established with RRCReestablishment message)
   */
  int srb_id = 2;
  if (ue_p->Srb[srb_id].Active) {
    nr_pdcp_entity_security_keys_and_algos_t security_parameters;
    security_parameters.ciphering_algorithm = ue_p->ciphering_algorithm;
    security_parameters.integrity_algorithm = ue_p->integrity_algorithm;
    nr_derive_key(RRC_ENC_ALG, ue_p->ciphering_algorithm, ue_p->kgnb, security_parameters.ciphering_key);
    nr_derive_key(RRC_INT_ALG, ue_p->integrity_algorithm, ue_p->kgnb, security_parameters.integrity_key);

    nr_pdcp_reestablishment(ue_p->rrc_ue_id, srb_id, true, &security_parameters);
  }
  /* PDCP Reestablishment of DRBs according to 5.3.5.6.5 of 3GPP TS 38.331 (over E1) */
  cuup_notify_reestablishment(rrc, ue_p);

  /* Create srb-ToAddModList */
  NR_SRB_ToAddModList_t *SRBs = createSRBlist(ue_p, true);
  /* Create drb-ToAddModList */
  NR_DRB_ToAddModList_t *DRBs = createDRBlist(ue_p, true);

  uint8_t new_xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue_p->xids[new_xid] = RRC_REESTABLISH_COMPLETE;
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_RRCReconfiguration(ue_p,
                                   buffer,
                                   NR_RRC_BUF_SIZE,
                                   new_xid,
                                   SRBs,
                                   DRBs,
                                   NULL,
                                   NULL,
                                   NULL, // MeasObj_list,
                                   NULL,
                                   cellGroupConfig);
  freeSRBlist(SRBs);
  freeDRBlist(DRBs);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Reconfiguration\n");

  AssertFatal(size > 0, "cannot encode RRC Reconfiguration\n");
  LOG_I(NR_RRC,
        "UE %d RNTI %04x: Generate NR_RRCReconfiguration after reestablishment complete (bytes %d)\n",
        ue_p->rrc_ue_id,
        ue_p->rnti,
        size);
  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DL_SCH_LCID_DCCH, buffer, size);
}
//-----------------------------------------------------------------------------

int nr_rrc_reconfiguration_req(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p, const int dl_bwp_id, const int ul_bwp_id)
{
  LOG_I(NR_RRC, " Reconfiguration triggered !\n");
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  uint8_t buffer[NR_RRC_BUF_SIZE];
  ue_p->xids[xid] = RRC_DEDICATED_RECONF;

  int size = rrc_gNB_encode_RRCReconfiguration(rrc, ue_p, xid, NULL, buffer, NR_RRC_BUF_SIZE, true);

  rrc_gNB_trigger_reconfiguration_for_xn_handover(rrc, ue_p, buffer, size);

  return 0;
}

static void rrc_handle_RRCSetupRequest(gNB_RRC_INST *rrc,
                                       sctp_assoc_t assoc_id,
                                       const NR_RRCSetupRequest_IEs_t *rrcSetupRequest,
                                       const f1ap_initial_ul_rrc_message_t *msg)
{
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  if (NR_InitialUE_Identity_PR_randomValue == rrcSetupRequest->ue_Identity.present) {
    /* randomValue                         BIT STRING (SIZE (39)) */
    if (rrcSetupRequest->ue_Identity.choice.randomValue.size != 5) { // 39-bit random value
      LOG_E(NR_RRC,
            "wrong InitialUE-Identity randomValue size, expected 5, provided %lu",
            (long unsigned int)rrcSetupRequest->ue_Identity.choice.randomValue.size);
      return;
    }
    uint64_t random_value = 0;
    memcpy(((uint8_t *)&random_value) + 3,
           rrcSetupRequest->ue_Identity.choice.randomValue.buf,
           rrcSetupRequest->ue_Identity.choice.randomValue.size);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
  } else if (NR_InitialUE_Identity_PR_ng_5G_S_TMSI_Part1 == rrcSetupRequest->ue_Identity.present) {
    /* <5G-S-TMSI> = <AMF Set ID><AMF Pointer><5G-TMSI> 48-bit */
    /* ng-5G-S-TMSI-Part1                  BIT STRING (SIZE (39)) */
    if (rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1.size != 5) {
      LOG_E(NR_RRC,
            "wrong ng_5G_S_TMSI_Part1 size, expected 5, provided %lu \n",
            (long unsigned int)rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1.size);
      return;
    }

    uint64_t s_tmsi_part1 = BIT_STRING_to_uint64(&rrcSetupRequest->ue_Identity.choice.ng_5G_S_TMSI_Part1);
    LOG_I(NR_RRC, "Received UE 5G-S-TMSI-Part1 %ld\n", s_tmsi_part1);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, s_tmsi_part1, msg->gNB_DU_ue_id);
    AssertFatal(ue_context_p != NULL, "out of memory\n");
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    UE->Initialue_identity_5g_s_TMSI.presence = true;
    UE->ng_5G_S_TMSI_Part1 = s_tmsi_part1;
  } else {
    uint64_t random_value = 0;
    memcpy(((uint8_t *)&random_value) + 3,
           rrcSetupRequest->ue_Identity.choice.randomValue.buf,
           rrcSetupRequest->ue_Identity.choice.randomValue.size);

    ue_context_p = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
    LOG_E(NR_RRC, "RRCSetupRequest without random UE identity or S-TMSI not supported, let's reject the UE %04x\n", msg->crnti);
    rrc_gNB_generate_RRCReject(0, ue_context_p);
    return;
  }

  // If the DU to CU RRC Container IE is not included in the INITIAL UL RRC MESSAGE TRANSFER,
  // the gNB-CU should reject the UE under the assumption that the gNB-DU is not able to serve such UE
  if (msg->du2cu_rrc_container == NULL) {
    // TODO shouldn't we remove ue context when rejecting the UE
    rrc_gNB_generate_RRCReject(0, ue_context_p);
    return;
  }
  NR_CellGroupConfig_t *cellGroupConfig = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                 &asn_DEF_NR_CellGroupConfig,
                                                 (void **)&cellGroupConfig,
                                                 msg->du2cu_rrc_container,
                                                 msg->du2cu_rrc_container_length);
  AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed > 0, "Cell group config decode error\n");

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE = &ue_context_p->ue_context;
  UE->establishment_cause = rrcSetupRequest->establishmentCause;
  UE->nr_cellid = msg->nr_cellid;
  UE->masterCellGroup = cellGroupConfig;
  activate_srb(UE, 1);
  rrc_gNB_generate_RRCSetup(0, msg->crnti, ue_context_p, msg->du2cu_rrc_container, msg->du2cu_rrc_container_length);
}

static const char *get_reestab_cause(NR_ReestablishmentCause_t c)
{
  switch (c) {
    case NR_ReestablishmentCause_otherFailure:
      return "Other Failure";
    case NR_ReestablishmentCause_handoverFailure:
      return "Handover Failure";
    case NR_ReestablishmentCause_reconfigurationFailure:
      return "Reconfiguration Failure";
    default:
      break;
  }
  return "UNKNOWN Failure (ASN.1 decoder error?)";
}

static rrc_gNB_ue_context_t *rrc_gNB_get_ue_context_source_cell(gNB_RRC_INST *rrc_instance_pP, long pci, rnti_t rntiP)
{
  rrc_gNB_ue_context_t *ue_context_p;
  RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &rrc_instance_pP->rrc_ue_head) {
    gNB_RRC_UE_t *ue = &ue_context_p->ue_context;
    if (!ue->ho_context || !ue->ho_context->source)
      continue;
    nr_ho_source_cu_t *source_ctx = ue->ho_context->source;
    if (source_ctx->old_rnti == rntiP && source_ctx->du->setup_req->cell[0].info.nr_pci == pci)
      return ue_context_p;
  }
  return NULL;
}

static void rrc_handle_RRCReestablishmentRequest(gNB_RRC_INST *rrc,
                                                 sctp_assoc_t assoc_id,
                                                 const NR_RRCReestablishmentRequest_IEs_t *req,
                                                 const f1ap_initial_ul_rrc_message_t *msg)
{
  uint64_t random_value = 0;
  const char *scause = get_reestab_cause(req->reestablishmentCause);
  const long physCellId = req->ue_Identity.physCellId;
  long ngap_cause = NGAP_CAUSE_RADIO_NETWORK_UNSPECIFIED; /* cause in case of NGAP release req */
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  LOG_I(NR_RRC,
        "Reestablishment RNTI %04x req C-RNTI %04lx physCellId %ld cause %s\n",
        msg->crnti,
        req->ue_Identity.c_RNTI,
        physCellId,
        scause);

  const nr_rrc_du_container_t *du = get_du_by_assoc_id(rrc, assoc_id);
  if (du == NULL) {
    LOG_E(RRC, "received CCCH message, but no corresponding DU found\n");
    return;
  }

  // 3GPP TS 38.321 version 15.13.0 Section 7.1 Table 7.1-1: RNTI values
  if (req->ue_Identity.c_RNTI < 0x1 || req->ue_Identity.c_RNTI > 0xffef) {
    /* c_RNTI range error should not happen */
    LOG_E(NR_RRC, "NR_RRCReestablishmentRequest c_RNTI %04lx range error, fallback to RRC setup\n", req->ue_Identity.c_RNTI);
    goto fallback_rrc_setup;
  }

  if (du->mib == NULL || du->sib1 == NULL) {
    /* we don't have MIB/SIB1 of the DU, and therefore cannot generate the
     * Reestablishment (as we would need the SSB's ARFCN, which we cannot
     * compute). So generate RRC Setup instead */
    LOG_E(NR_RRC, "Reestablishment request: no MIB/SIB1 of DU present, cannot do reestablishment, force setup request\n");
    goto fallback_rrc_setup;
  }

  if (du->mtc == NULL) {
    // some UEs don't send MeasurementTimingConfiguration, so we don't know the
    // SSB ARFCN and can't do reestablishment. handle it gracefully by doing
    // RRC setup procedure instead
    LOG_E(NR_RRC, "no MeasurementTimingConfiguration for this cell, cannot perform reestablishment\n");
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    goto fallback_rrc_setup;
  }

  rnti_t old_rnti = req->ue_Identity.c_RNTI;
  ue_context_p = rrc_gNB_get_ue_context_by_rnti(rrc, assoc_id, old_rnti);
  if (ue_context_p == NULL) {
    ue_context_p = rrc_gNB_get_ue_context_source_cell(rrc, physCellId, old_rnti);
    if (ue_context_p == NULL) {
      LOG_E(NR_RRC, "NR_RRCReestablishmentRequest without UE context, fallback to RRC setup\n");
      goto fallback_rrc_setup;
    }
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  /* should check phys cell ID to identify the correct cell */
  const f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  const f1ap_served_cell_info_t *previous_cell_info = get_cell_information_by_phycellId(physCellId);

  nr_ho_source_cu_t *source_ctx = UE->ho_context ? UE->ho_context->source : NULL;
  DevAssert(!source_ctx || source_ctx->du->setup_req->num_cells_available == 1);
  bool ho_reestab_on_source = source_ctx && previous_cell_info->nr_cellid == source_ctx->du->setup_req->cell[0].info.nr_cellid;

  if (ho_reestab_on_source) {
    /* the UE came back on the source DU while doing handover, release at
     * target DU and and update the association to the initial DU one */
    LOG_W(NR_RRC, "handover for UE %d/RNTI %04x failed, rollback to original cell\n", UE->rrc_ue_id, UE->rnti);

    // find the transaction of handover (the corresponding reconfig) and abort it
    for (int i = 0; i < NR_RRC_TRANSACTION_IDENTIFIER_NUMBER; ++i) {
      if (UE->xids[i] == RRC_DEDICATED_RECONF)
        UE->xids[i] = RRC_ACTION_NONE;
    }

    source_ctx->ho_cancel(rrc, UE);

    /* we need the original CellGroupConfig */
    ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
    UE->masterCellGroup = source_ctx->old_cellGroupConfig;
    source_ctx->old_cellGroupConfig = NULL;

    /* update to old DU assoc id -- RNTI + secondary DU UE ID further below */
    f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
    ue_data.du_assoc_id = source_ctx->du->assoc_id;
    bool success = cu_update_f1_ue_data(UE->rrc_ue_id, &ue_data);
    DevAssert(success);
    nr_rrc_finalize_ho(UE);
  } else if (physCellId != cell_info->nr_pci) {
    /* UE was moving from previous cell so quickly that RRCReestablishment for previous cell was received in this cell */
    LOG_I(NR_RRC,
          "RRC Reestablishment Request from different physCellId (%ld) than current physCellId (%d), fallback to RRC setup\n",
          physCellId,
          cell_info->nr_pci);
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    /* 38.401 8.7: "If the UE accessed from a gNB-DU other than the original
     * one, the gNB-CU should trigger the UE Context Setup procedure". Let's
     * assume that the old DU will trigger a release request, also freeing the
     * ongoing context at the CU. Hence, create new below */
    goto fallback_rrc_setup;
  }

  if (!UE->as_security_active) {
    /* no active security context, need to restart entire connection */
    LOG_E(NR_RRC, "UE requested Reestablishment without activated AS security\n");
    ngap_cause = NGAP_CAUSE_RADIO_NETWORK_RELEASE_DUE_TO_NGRAN_GENERATED_REASON;
    goto fallback_rrc_setup;
  }

  /* TODO: start timer in ITTI and drop UE if it does not come back */

  // update with new RNTI, and update secondary UE association
  UE->rnti = msg->crnti;
  UE->nr_cellid = msg->nr_cellid;
  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  ue_data.secondary_ue = msg->gNB_DU_ue_id;
  bool success = cu_update_f1_ue_data(UE->rrc_ue_id, &ue_data);
  DevAssert(success);

  rrc_gNB_generate_RRCReestablishment(ue_context_p, msg->du2cu_rrc_container, old_rnti, du);
  return;

fallback_rrc_setup:
  fill_random(&random_value, sizeof(random_value));
  random_value = random_value & 0x7fffffffff; /* random value is 39 bits */

  ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = ngap_cause};
  /* request release of the "old" UE in case it exists */
  if (ue_context_p != NULL)
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(0, ue_context_p, cause);

  rrc_gNB_ue_context_t *new = rrc_gNB_create_ue_context(assoc_id, msg->crnti, rrc, random_value, msg->gNB_DU_ue_id);
  activate_srb(&new->ue_context, 1);
  rrc_gNB_generate_RRCSetup(0, msg->crnti, new, msg->du2cu_rrc_container, msg->du2cu_rrc_container_length);
  return;
}

static void process_Periodical_Measurement_Report(gNB_RRC_UE_t *ue_ctxt, NR_MeasurementReport_t *measurementReport)
{
  ASN_STRUCT_FREE(asn_DEF_NR_MeasResults, ue_ctxt->measResults);
  ue_ctxt->measResults = NULL;

  const NR_MeasId_t id = measurementReport->criticalExtensions.choice.measurementReport->measResults.measId;
  AssertFatal(id, "unexpected MeasResult for MeasurementId %ld received\n", id);
  asn1cCallocOne(ue_ctxt->measResults, measurementReport->criticalExtensions.choice.measurementReport->measResults);
  /* we "keep" the measurement report, so set to 0 */
  free(measurementReport->criticalExtensions.choice.measurementReport);
  measurementReport->criticalExtensions.choice.measurementReport = NULL;
}

static void process_Event_Based_Measurement_Report(gNB_RRC_UE_t *ue, NR_ReportConfigNR_t *report, NR_MeasurementReport_t *measurementReport)
{
  if (!report) {
    LOG_E(NR_RRC, "process_Event_Based_Measurement_Report: report is null\n");
    return;
  }
  NR_EventTriggerConfig_t *event_triggered = report->reportType.choice.eventTriggered;

  int servingCellRSRP = 0;
  int neighbourCellRSRP = 0;
  int servingCellId = -1;

  switch (event_triggered->eventId.present) {
    case NR_EventTriggerConfig__eventId_PR_eventA1:
      LOG_I(NR_RRC, "Event A1 (Serving becomes better than threshold)\n");
      break;

    case NR_EventTriggerConfig__eventId_PR_eventA2:
      LOG_W(NR_RRC, "HO LOG: Event A2 (Serving becomes worse than threshold)\n");
      break;

    case NR_EventTriggerConfig__eventId_PR_eventA3: {
      LOG_W(NR_RRC, "HO LOG: Event A3 Report for UE %d - Neighbour Becomes Better than Serving!\n", ue->rrc_ue_id);
      if (!measurementReport->criticalExtensions.choice.measurementReport) {
        LOG_E(NR_RRC, "HO LOG: Event A3 Report: measurementReport is null\n");
        break;
      }
      const NR_MeasResults_t *measResults = &measurementReport->criticalExtensions.choice.measurementReport->measResults;

      for (int serving_cell_idx = 0; serving_cell_idx < measResults->measResultServingMOList.list.count; serving_cell_idx++) {
        const NR_MeasResultServMO_t *meas_result_serv_MO = measResults->measResultServingMOList.list.array[serving_cell_idx];
        servingCellId = *(meas_result_serv_MO->measResultServingCell.physCellId);
        if (meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsSSB_Cell) {
          servingCellRSRP = *(meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsSSB_Cell->rsrp) - 157;
        } else {
          servingCellRSRP = *(meas_result_serv_MO->measResultServingCell.measResult.cellResults.resultsCSI_RS_Cell->rsrp) - 157;
        }
        LOG_D(NR_RRC, "Serving Cell RSRP: %d\n", servingCellRSRP);
      }

      if (measResults->measResultNeighCells == NULL ||
          measResults->measResultNeighCells->present != NR_MeasResults__measResultNeighCells_PR_measResultListNR) {
        LOG_D(NR_RRC, "HO LOG: No neighbor cell measurements available\n");
        break;
      }

      const NR_MeasResultListNR_t *measResultListNR = measResults->measResultNeighCells->choice.measResultListNR;
      for (int neigh_meas_idx = 0; neigh_meas_idx < measResultListNR->list.count; neigh_meas_idx++) {
        const NR_MeasResultNR_t *meas_result_neigh_cell = (measResultListNR->list.array[neigh_meas_idx]);
        const int neighbourCellId = *(meas_result_neigh_cell->physCellId);
        gNB_RRC_INST *rrc = RC.nrrrc[0];

        // TS 138 133 Table 10.1.6.1-1: SS-RSRP and CSI-RSRP measurement report mapping
        const struct NR_MeasResultNR__measResult__cellResults *cellResults = &(meas_result_neigh_cell->measResult.cellResults);

        if (cellResults->resultsSSB_Cell) {
          neighbourCellRSRP = *(cellResults->resultsSSB_Cell->rsrp) - 157;
        } else {
          neighbourCellRSRP = *(cellResults->resultsCSI_RS_Cell->rsrp) - 157;
        }

        LOG_I(NR_RRC,
              "HO LOG: Measurement Report for the neighbour %d with RSRP: %d\n",
              neighbourCellId,
              neighbourCellRSRP);

        const f1ap_served_cell_info_t *neigh_cell = get_cell_information_by_phycellId(neighbourCellId);
        const f1ap_served_cell_info_t *serving_cell = get_cell_information_by_phycellId(servingCellId);
        const nr_neighbour_gnb_configuration_t *neighbour =
            get_neighbour_cell_information(serving_cell->nr_cellid, neighbourCellId);
        // CU does not have f1 connection with neighbour cell context. So  check does serving cell has this phyCellId as a
        // neighbour.
        if (!neigh_cell && neighbour) {
          // No F1 connection but static neighbour configuration is available
          const nr_a3_event_t *a3_event_configuration = get_a3_configuration(neighbour->physicalCellId);
          // Additional check - This part can be modified according to additional cell specific Handover Margin
          if (a3_event_configuration
              && ((a3_event_configuration->a3_offset + a3_event_configuration->hysteresis)
                  < (neighbourCellRSRP - servingCellRSRP))) {
             // if (NODE_IS_MONOLITHIC(RC.nrrrc[0]->node_type)) {
             //    LOG_I(NR_RRC, "HO LOG: Trigger N2 HO for the neighbour gnb: %u cell: %lu\n", neighbour->gNB_ID, neighbour->nrcell_id);
             //    nr_rrc_trigger_n2_ho(RC.nrrrc[0], ue->rrc_ue_id, neighbour);
	     // } else {
	     //   LOG_A(NR_RRC, "HO LOG: Trigger Xn HO for UE id : %d \n", ue->rrc_ue_id);
	     //   nr_HO_Xn_trigger(ue->rrc_ue_id, neighbour);
	     // }
                bool xn_setup_established = is_valid_nr_cell_id(rrc, neighbour->nrcell_id);
                if(xn_setup_established){
                    LOG_I(NR_RRC, "HO LOG: Trigger Xn HO for UE : %d towards Target gNB : %u cell : %lu \n", ue->rrc_ue_id, neighbour->gNB_ID, neighbour->nrcell_id);
                    nr_HO_Xn_trigger(ue->rrc_ue_id, neighbour);
                    // nr_rrc_trigger_xn_ho(rrc, ue, scell_pci, neighbour);
                }else{
                    LOG_I(NR_RRC, "HO LOG: Trigger N2 HO for the neighbour gnb: %u cell: %lu\n", neighbour->gNB_ID, neighbour->nrcell_id);  
                    nr_rrc_trigger_n2_ho(rrc, ue->rrc_ue_id, neighbour);
                } 
          }
        } else if (neigh_cell && neighbour) {
          /* we know the cell and are connected to the DU! */
          nr_rrc_du_container_t *source_du = get_du_by_cell_id(rrc, serving_cell->nr_cellid);
          DevAssert(source_du);
          nr_rrc_du_container_t *target_du = get_du_by_cell_id(rrc, neigh_cell->nr_cellid);
          nr_rrc_trigger_f1_ho(rrc, ue, source_du, target_du);
        } else {
          LOG_W(NR_RRC, "UE %d: received A3 event for stronger neighbor PCI %d, but no such neighbour in configuration\n", ue->rrc_ue_id, neighbourCellId);
        }
      }

    } break;

    case NR_EventTriggerConfig__eventId_PR_eventA4:
      LOG_I(NR_RRC, "Event A4 (Neighbour becomes better than threshold)\n");
      break;

    case NR_EventTriggerConfig__eventId_PR_eventA5:
      LOG_I(NR_RRC, "Event A5 (SpCell becomes worse than threshold1 and neighbour becomes better than threshold2)\n");
      break;

    case NR_EventTriggerConfig__eventId_PR_eventA6:
      LOG_I(NR_RRC, "Event A6 (Neighbour becomes offset better than SCell)\n");
      break;

    default:
      LOG_D(NR_RRC, "NR_EventTriggerConfig__eventId_PR_NOTHING or Other event report\n");
      break;
  }
}

static void rrc_gNB_process_MeasurementReport(gNB_RRC_UE_t *UE, NR_MeasurementReport_t *measurementReport)
{
  NR_MeasurementReport__criticalExtensions_PR p = measurementReport->criticalExtensions.present;
  if (p != NR_MeasurementReport__criticalExtensions_PR_measurementReport
      || measurementReport->criticalExtensions.choice.measurementReport == NULL) {
    LOG_E(NR_RRC, "UE %d: expected presence of MeasurementReport, but has %d (%p)\n", UE->rrc_ue_id, p, measurementReport->criticalExtensions.choice.measurementReport);
    return;
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_MeasurementReport, (void *)measurementReport);

  NR_MeasConfig_t *meas_config = UE->measConfig;
  if (meas_config == NULL) {
    LOG_I(NR_RRC, "Unexpected Measurement Report from UE with id: %d\n", UE->rrc_ue_id);
    return;
  }

  NR_MeasurementReport_IEs_t *measurementReport_IEs = measurementReport->criticalExtensions.choice.measurementReport;
  const NR_MeasId_t measId = measurementReport_IEs->measResults.measId;

  NR_MeasIdToAddMod_t *meas_id_s = NULL;
  for (int meas_idx = 0; meas_idx < meas_config->measIdToAddModList->list.count; meas_idx++) {
    if (measId == meas_config->measIdToAddModList->list.array[meas_idx]->measId) {
      meas_id_s = meas_config->measIdToAddModList->list.array[meas_idx];
      break;
    }
  }

  if (meas_id_s == NULL) {
    LOG_E(NR_RRC, "Incoming Meas ID with id: %d Can not Found!\n", (int)measId);
    return;
  }

  LOG_D(NR_RRC, "HO LOG: Meas Id is found: %d\n", (int)meas_id_s->measId);

  struct NR_ReportConfigToAddMod__reportConfig *report_config = NULL;
  for (int rep_id = 0; rep_id < meas_config->reportConfigToAddModList->list.count; rep_id++) {
    if (meas_id_s->reportConfigId == meas_config->reportConfigToAddModList->list.array[rep_id]->reportConfigId) {
      report_config = &meas_config->reportConfigToAddModList->list.array[rep_id]->reportConfig;
    }
  }

  if (report_config == NULL || report_config->choice.reportConfigNR == NULL) {
    LOG_E(NR_RRC, "There is no related report configuration for this measId!\n");
    return;
  }

  if (report_config->choice.reportConfigNR->reportType.present == NR_ReportConfigNR__reportType_PR_periodical)
    return process_Periodical_Measurement_Report(UE, measurementReport);

  if (report_config->choice.reportConfigNR->reportType.present == NR_ReportConfigNR__reportType_PR_eventTriggered)
    return process_Event_Based_Measurement_Report(UE, report_config->choice.reportConfigNR, measurementReport);

  LOG_E(NR_RRC, "Incoming Report Type: %d is not supported! \n", report_config->choice.reportConfigNR->reportType.present);
}

static int handle_rrcReestablishmentComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCReestablishmentComplete_t *cplt)
{
  NR_RRCReestablishmentComplete__criticalExtensions_PR p = cplt->criticalExtensions.present;
  if (p != NR_RRCReestablishmentComplete__criticalExtensions_PR_rrcReestablishmentComplete) {
    LOG_E(NR_RRC, "UE %d: expected presence of rrcReestablishmentComplete, but message has %d\n", UE->rrc_ue_id, p);
    return -1;
  }
  rrc_gNB_process_RRCReestablishmentComplete(rrc, UE, cplt->rrc_TransactionIdentifier);

  UE->ue_reestablishment_counter++;
  return 0;
}

/**
 * @brief Forward stored NAS PDU to UE (3GPP TS 38.413)
 *        - 8.2.1.2: If the NAS-PDU IE is included in the PDU SESSION RESOURCE SETUP REQUEST message,
 *        the NG-RAN node shall pass it to the UE.
 *        - 8.3.1.2: If the NAS-PDU IE is included in the INITIAL CONTEXT SETUP REQUEST message,
 *        the NG-RAN node shall pass it transparently towards the UE.
 *        - 8.6.2: The NAS-PDU IE contains an AMF–UE message that is transferred without interpretation in the NG-RAN node.
 */
void rrc_forward_ue_nas_message(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE)
{
  if (UE->nas_pdu.buffer == NULL || UE->nas_pdu.length == 0)
    return; // no problem: the UE will re-request a NAS PDU

  LOG_UE_DL_EVENT(UE, "Send DL Information Transfer [%d bytes]\n", UE->nas_pdu.length);

  uint8_t buffer[4096];
  unsigned int xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  uint32_t length = do_NR_DLInformationTransfer(buffer, sizeof(buffer), xid, UE->nas_pdu.length, UE->nas_pdu.buffer);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, buffer, length, "[MSG] RRC DL Information Transfer\n");
  rb_id_t srb_id = UE->Srb[2].Active ? DL_SCH_LCID_DCCH1 : DL_SCH_LCID_DCCH;
  nr_rrc_transfer_protected_rrc_message(rrc, UE, srb_id, buffer, length);
  // no need to free UE->nas_pdu.buffer, do_NR_DLInformationTransfer() did that
  UE->nas_pdu.buffer = NULL;
  UE->nas_pdu.length = 0;
}

static void handle_ueCapabilityInformation(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_UECapabilityInformation_t *ue_cap_info)
{
  int xid = ue_cap_info->rrc_TransactionIdentifier;
  rrc_action_t a = UE->xids[xid];
  UE->xids[xid] = RRC_ACTION_NONE;
  if (a != RRC_UECAPABILITY_ENQUIRY) {
    LOG_E(NR_RRC, "UE %d: received unsolicited UE Capability Information, aborting procedure\n", UE->rrc_ue_id);
    return;
  }

  int eutra_index = -1;

  if (ue_cap_info->criticalExtensions.present == NR_UECapabilityInformation__criticalExtensions_PR_ueCapabilityInformation) {
    const NR_UE_CapabilityRAT_ContainerList_t *ue_CapabilityRAT_ContainerList =
        ue_cap_info->criticalExtensions.choice.ueCapabilityInformation->ue_CapabilityRAT_ContainerList;

    /* Encode UE-CapabilityRAT-ContainerList for sending to the DU */
    free(UE->ue_cap_buffer.buf);
    UE->ue_cap_buffer.len = uper_encode_to_new_buffer(&asn_DEF_NR_UE_CapabilityRAT_ContainerList,
                                                      NULL,
                                                      ue_CapabilityRAT_ContainerList,
                                                      (void **)&UE->ue_cap_buffer.buf);
    if (UE->ue_cap_buffer.len <= 0) {
      LOG_E(RRC, "could not encode UE-CapabilityRAT-ContainerList, abort handling capabilities\n");
      return;
    }
    LOG_UE_UL_EVENT(UE, "Received UE capabilities\n");

    for (int i = 0; i < ue_CapabilityRAT_ContainerList->list.count; i++) {
      const NR_UE_CapabilityRAT_Container_t *ue_cap_container = ue_CapabilityRAT_ContainerList->list.array[i];
      if (ue_cap_container->rat_Type == NR_RAT_Type_nr) {
        if (UE->UE_Capability_nr) {
          ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
          UE->UE_Capability_nr = 0;
        }

        asn_dec_rval_t dec_rval = uper_decode(NULL,
                                              &asn_DEF_NR_UE_NR_Capability,
                                              (void **)&UE->UE_Capability_nr,
                                              ue_cap_container->ue_CapabilityRAT_Container.buf,
                                              ue_cap_container->ue_CapabilityRAT_Container.size,
                                              0,
                                              0);
        if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
          xer_fprint(stdout, &asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
        }

        if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
          LOG_E(NR_RRC, "UE %d: Failed to decode nr UE capabilities (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
          ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
          UE->UE_Capability_nr = 0;
        }

        UE->UE_Capability_size = ue_cap_container->ue_CapabilityRAT_Container.size;
        if (eutra_index != -1) {
          LOG_E(NR_RRC, "fatal: more than 1 eutra capability\n");
          return;
        }
        eutra_index = i;
      }

      if (ue_cap_container->rat_Type == NR_RAT_Type_eutra_nr) {
        if (UE->UE_Capability_MRDC) {
          ASN_STRUCT_FREE(asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
          UE->UE_Capability_MRDC = 0;
        }
        asn_dec_rval_t dec_rval = uper_decode(NULL,
                                              &asn_DEF_NR_UE_MRDC_Capability,
                                              (void **)&UE->UE_Capability_MRDC,
                                              ue_cap_container->ue_CapabilityRAT_Container.buf,
                                              ue_cap_container->ue_CapabilityRAT_Container.size,
                                              0,
                                              0);

        if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
          xer_fprint(stdout, &asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
        }

        if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
          LOG_E(NR_RRC, "UE %d: Failed to decode nr UE capabilities (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
          ASN_STRUCT_FREE(asn_DEF_NR_UE_MRDC_Capability, UE->UE_Capability_MRDC);
          UE->UE_Capability_MRDC = 0;
        }
        UE->UE_MRDC_Capability_size = ue_cap_container->ue_CapabilityRAT_Container.size;
      }

      if (ue_cap_container->rat_Type == NR_RAT_Type_eutra) {
        // TODO
      }
    }

    if (eutra_index == -1)
      return;
  }

  rrc_gNB_send_NGAP_UE_CAPABILITIES_IND(rrc, UE, ue_cap_info);

  if (UE->n_initial_pdu > 0) {
    /* there were PDU sessions with the NG UE Context setup, but we had to set
     * up security and request capabilities, so trigger PDU sessions now. The
     * UE NAS message will be forwarded in the corresponding reconfiguration,
     * the Initial context setup response after reconfiguration complete. */
    if (!trigger_bearer_setup(rrc, UE, UE->n_initial_pdu, UE->initial_pdus, 0)) {
      LOG_W(NR_RRC, "Failed to setup bearers for UE %d: send Initial Context Setup Response\n", UE->rrc_ue_id);
      rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
      rrc_forward_ue_nas_message(rrc, UE);
      return;
    }
  } else {
    rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
    rrc_forward_ue_nas_message(rrc, UE);
  }

  return;
}

static void handle_rrcSetupComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCSetupComplete_t *setup_complete)
{
  uint8_t xid = setup_complete->rrc_TransactionIdentifier;
  UE->xids[xid] = RRC_ACTION_NONE;

  if (setup_complete->criticalExtensions.present != NR_RRCSetupComplete__criticalExtensions_PR_rrcSetupComplete) {
    LOG_E(NR_RRC, "malformed RRCSetupComplete received from UE %d\n", UE->rrc_ue_id);
    return;
  }

  NR_RRCSetupComplete_IEs_t *setup_complete_ies = setup_complete->criticalExtensions.choice.rrcSetupComplete;

  if (setup_complete_ies->ng_5G_S_TMSI_Value != NULL) {
    uint64_t fiveg_s_TMSI = 0;
    if (setup_complete_ies->ng_5G_S_TMSI_Value->present == NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI_Part2) {
      const BIT_STRING_t *part2 = &setup_complete_ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI_Part2;
      if (part2->size != 2) {
        LOG_E(NR_RRC, "wrong ng_5G_S_TMSI_Part2 size, expected 2, provided %lu", part2->size);
        return;
      }

      if (UE->Initialue_identity_5g_s_TMSI.presence) {
        uint16_t stmsi_part2 = BIT_STRING_to_uint16(part2);
        LOG_I(RRC, "s_tmsi part2 %d (%02x %02x)\n", stmsi_part2, part2->buf[0], part2->buf[1]);
        // Part2 is leftmost 9, Part1 is rightmost 39 bits of 5G-S-TMSI
        fiveg_s_TMSI = ((uint64_t) stmsi_part2) << 39 | UE->ng_5G_S_TMSI_Part1;
      } else {
        LOG_W(RRC, "UE %d received 5G-S-TMSI-Part2, but no 5G-S-TMSI-Part1 present, won't send 5G-S-TMSI to core\n", UE->rrc_ue_id);
        UE->Initialue_identity_5g_s_TMSI.presence = false;
      }
    } else if (setup_complete_ies->ng_5G_S_TMSI_Value->present == NR_RRCSetupComplete_IEs__ng_5G_S_TMSI_Value_PR_ng_5G_S_TMSI) {
      const NR_NG_5G_S_TMSI_t *bs_stmsi = &setup_complete_ies->ng_5G_S_TMSI_Value->choice.ng_5G_S_TMSI;
      if (bs_stmsi->size != 6) {
        LOG_E(NR_RRC, "wrong ng_5G_S_TMSI size, expected 6, provided %lu", bs_stmsi->size);
        return;
      }

      fiveg_s_TMSI = BIT_STRING_to_uint64(bs_stmsi);
      UE->Initialue_identity_5g_s_TMSI.presence = true;
    }

    if (UE->Initialue_identity_5g_s_TMSI.presence) {
      uint16_t amf_set_id = fiveg_s_TMSI >> 38;
      uint8_t amf_pointer = (fiveg_s_TMSI >> 32) & 0x3F;
      uint32_t fiveg_tmsi = (uint32_t) fiveg_s_TMSI;
      LOG_I(NR_RRC,
            "5g_s_TMSI: 0x%lX, amf_set_id: 0x%X (%d), amf_pointer: 0x%X (%d), 5g TMSI: 0x%X \n",
            fiveg_s_TMSI,
            amf_set_id,
            amf_set_id,
            amf_pointer,
            amf_pointer,
            fiveg_tmsi);
      UE->Initialue_identity_5g_s_TMSI.amf_set_id = amf_set_id;
      UE->Initialue_identity_5g_s_TMSI.amf_pointer = amf_pointer;
      UE->Initialue_identity_5g_s_TMSI.fiveg_tmsi = fiveg_tmsi;

      // update random identity with 5G-S-TMSI, which only contained Part1 of it
      UE->random_ue_identity = fiveg_s_TMSI;
    }
  }

  rrc_gNB_process_RRCSetupComplete(rrc, UE, setup_complete->criticalExtensions.choice.rrcSetupComplete);
  return;
}

static int fill_pdusession_id_release(pdusessionid_t *pdusession_id, gNB_RRC_UE_t *UE, int xid)
{
  int k = 0;
  for (int i = 0; i < UE->nb_of_pdusessions; i++) {
    rrc_pdu_session_param_t *session = &UE->pduSession[i];
    if (session->status == PDU_SESSION_STATUS_TORELEASE && session->xid == xid) {
      pdusession_id[k] = session->param.pdusession_id;
      k++;
    }
  }
  return k;
}

static void handle_rrcReconfigurationComplete(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const NR_RRCReconfigurationComplete_t *reconfig_complete)
{
  uint8_t xid = reconfig_complete->rrc_TransactionIdentifier;
  UE->ue_reconfiguration_counter++;

  switch (UE->xids[xid]) {
    case RRC_PDUSESSION_RELEASE:
#ifdef UNI_RAN
      for (int i = 0; i < UE->nb_of_pdusessions; i++) {
        UE->pduSession[i].xid = xid;
      }
#endif
      // NGAP_PDUSESSION_RELEASE_RESPONSE
      rrc_gNB_send_NGAP_PDUSESSION_RELEASE_RESPONSE(rrc, UE, xid);
      break;
    case RRC_PDUSESSION_ESTABLISH:
      if (UE->n_initial_pdu > 0) {
#ifdef UNI_RAN
        for (int i = 0; i < UE->nb_of_pdusessions; i++) {
          UE->pduSession[i].xid = xid;
        }
#endif
        /* PDU sessions through initial UE context setup */
        rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
        UE->n_initial_pdu = 0;
        free(UE->initial_pdus);
        UE->initial_pdus = NULL;
      } else if (UE->nb_of_pdusessions > 0) {
#ifdef UNI_RAN
        for (int i = 0; i < UE->nb_of_pdusessions; i++) {
          UE->pduSession[i].xid = xid;
        }
#endif
        rrc_gNB_send_NGAP_PDUSESSION_SETUP_RESP(rrc, UE, xid);
      }
      break;
    case RRC_PDUSESSION_MODIFY:
      rrc_gNB_send_NGAP_PDUSESSION_MODIFY_RESP(rrc, UE, xid);
      break;
    case RRC_REESTABLISH_COMPLETE:
    case RRC_DEDICATED_RECONF:
    if ((UE->ho_context != NULL) &&  (UE->ho_context->target != NULL)) {
      LOG_I(NR_RRC, "HO LOG: RRC Reconfig Complete Came for HO UE w Id: %u\n", UE->rrc_ue_id);
#ifdef UNI_RAN
      nr_pdcp_entity_security_keys_and_algos_t security_parameters;
      security_parameters.ciphering_algorithm = UE->ciphering_algorithm;
      security_parameters.integrity_algorithm = UE->integrity_algorithm;
      nr_derive_key(UP_ENC_ALG, UE->ciphering_algorithm, UE->kgnb, security_parameters.ciphering_key);
      nr_derive_key(UP_INT_ALG, UE->integrity_algorithm, UE->kgnb, security_parameters.integrity_key);

      nr_pdcp_entity_security_keys_and_algos_t null_security_parameters = {0};

      UE->Srb[1].Active = 1;
      UE->Srb[2].Active = 1;
      NR_SRB_ToAddModList_t *SRBs = createSRBlist(UE, false);

      nr_pdcp_add_srbs(true, UE->rrc_ue_id, SRBs,&security_parameters,true);
      freeSRBlist(SRBs);
      uint8_t enableCiphering = 0;
      UE->as_security_active = true;
      nr_rrc_pdcp_config_security(UE, enableCiphering);
#endif
      rrc_gNB_send_NGAP_HANDOVER_NOTIFY(rrc, UE, 0);
#ifdef UNI_RAN
      for (int i = 0; i < UE->nb_of_pdusessions; i++) {
        UE->pduSession[i].xid = xid;
        if (UE->pduSession[i].status == PDU_SESSION_STATUS_NEW || UE->pduSession[i].status == PDU_SESSION_STATUS_REESTABLISHED) {
          LOG_I(RRC, "UE %d: Marking PDU session %d as ESTABLISHED after HO\n", UE->rrc_ue_id, UE->pduSession[i].param.pdusession_id);
          UE->pduSession[i].status = PDU_SESSION_STATUS_ESTABLISHED;
        }
      }
#endif
    }
      break;
    case RRC_ACTION_NONE:
      LOG_E(RRC, "UE %d: Received RRC Reconfiguration Complete with xid %d while no transaction is ongoing\n", UE->rrc_ue_id, xid);
      break;
    default:
      LOG_E(RRC, "UE %d: Received unexpected transaction type %d for xid %d\n", UE->rrc_ue_id, UE->xids[xid], xid);
      break;
  }

  if (UE->xids[xid] == RRC_PDUSESSION_ESTABLISH) {
    UE->ongoing_pdusession_setup_request = false;
  }

  UE->xids[xid] = RRC_ACTION_NONE;
  for (int i = 0; i < NR_RRC_TRANSACTION_IDENTIFIER_NUMBER; ++i) {
    if (UE->xids[i] != RRC_ACTION_NONE) {
      LOG_I(RRC, "UE %d: transaction %d still ongoing for action %d\n", UE->rrc_ue_id, i, UE->xids[i]);
    }
  }

  if (UE->ho_context != NULL) {
    LOG_A(NR_RRC, "handover for UE %d/RNTI %04x complete!\n", UE->rrc_ue_id, UE->rnti);
    DevAssert(UE->ho_context->target != NULL);

   if(UE->ho_context->source)
     UE->ho_context->target->ho_success(rrc, UE);

    nr_rrc_finalize_ho(UE);
  }
}

static void rrc_gNB_generate_UECapabilityEnquiry(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue)
{
  uint8_t buffer[100];

  T(T_ENB_RRC_UE_CAPABILITY_ENQUIRY, T_INT(rrc->module_id), T_INT(0), T_INT(0), T_INT(ue->rrc_ue_id));
  uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
  ue->xids[xid] = RRC_UECAPABILITY_ENQUIRY;
  int size = do_NR_SA_UECapabilityEnquiry(buffer, xid);
  LOG_I(NR_RRC, "UE %d: Logical Channel DL-DCCH, Generate NR UECapabilityEnquiry (bytes %d, xid %d)\n", ue->rrc_ue_id, size, xid);

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  nr_rrc_transfer_protected_rrc_message(rrc, ue, DL_SCH_LCID_DCCH, buffer, size);
}

static int rrc_gNB_decode_dcch(gNB_RRC_INST *rrc, const f1ap_ul_rrc_message_t *msg)
{
  /* we look up by CU UE ID! Do NOT change back to RNTI! */
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, msg->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", msg->gNB_CU_ue_id);
    return -1;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  if (msg->srb_id < 1 || msg->srb_id > 2) {
    LOG_E(NR_RRC, "Received message on SRB %d, discarding message\n", msg->srb_id);
    return -1;
  }

  LOG_D(NR_RRC, "UE %d: Decoding DCCH %d size %d\n", UE->rrc_ue_id, msg->srb_id, msg->rrc_container_length);
  LOG_DUMPMSG(RRC, DEBUG_RRC, (char *)msg->rrc_container, msg->rrc_container_length, "[MSG] RRC UL Information Transfer \n");
  NR_UL_DCCH_Message_t *ul_dcch_msg = NULL;
  asn_dec_rval_t dec_rval =
      uper_decode(NULL, &asn_DEF_NR_UL_DCCH_Message, (void **)&ul_dcch_msg, msg->rrc_container, msg->rrc_container_length, 0, 0);

  if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
    LOG_E(NR_RRC, "UE %d: Failed to decode UL-DCCH (%zu bytes)\n", UE->rrc_ue_id, dec_rval.consumed);
    return -1;
  }

  if (LOG_DEBUGFLAG(DEBUG_ASN1)) {
    xer_fprint(stdout, &asn_DEF_NR_UL_DCCH_Message, (void *)ul_dcch_msg);
  }

  if (ul_dcch_msg->message.present == NR_UL_DCCH_MessageType_PR_c1) {
    switch (ul_dcch_msg->message.choice.c1->present) {
      case NR_UL_DCCH_MessageType__c1_PR_NOTHING:
        LOG_I(NR_RRC, "Received PR_NOTHING on UL-DCCH-Message\n");
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcReconfigurationComplete:
        LOG_UE_UL_EVENT(UE, "Received RRCReconfigurationComplete\n");
        handle_rrcReconfigurationComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcReconfigurationComplete);
        if (UE->ho_context_xn != NULL) {
          uint8_t xid = rrc_gNB_get_next_transaction_identifier(rrc->module_id);
          UE->xids[xid] = RRC_PATH_SWITCH;
          rrc_gNB_send_NGAP_PATH_SWITCH_REQ(rrc, UE, xid);
        }
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcSetupComplete:
        LOG_UE_UL_EVENT(UE, "Received RRCSetupComplete (RRC_CONNECTED reached)\n");
        handle_rrcSetupComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcSetupComplete);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_measurementReport:
        if (ul_dcch_msg->message.choice.c1->choice.measurementReport != NULL) {
          rrc_gNB_process_MeasurementReport(UE, ul_dcch_msg->message.choice.c1->choice.measurementReport);
        } else {
          LOG_E(NR_RRC, "UE %d: No measurementReport CHOICE is given\n", ue_context_p->ue_context.rrc_ue_id);
        }
        break;

      case NR_UL_DCCH_MessageType__c1_PR_ulInformationTransfer:
        LOG_UE_UL_EVENT(UE, "Received RRC UL Information Transfer [%d bytes]\n", msg->rrc_container_length);
        rrc_gNB_send_NGAP_UPLINK_NAS(rrc, UE, ul_dcch_msg);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_securityModeComplete:
        LOG_UE_UL_EVENT(UE, "Received Security Mode Complete\n");
        /* configure ciphering */
        nr_rrc_pdcp_config_security(UE, true);
        UE->as_security_active = true;

        /* trigger UE capability enquiry if we don't have them yet */
        if (UE->ue_cap_buffer.len == 0) {
          rrc_gNB_generate_UECapabilityEnquiry(rrc, UE);
          /* else blocks are executed after receiving UE capability info */
        } else if (UE->n_initial_pdu > 0) {
          /* there were PDU sessions with the NG UE Context setup, but we had
           * to set up security, so trigger PDU sessions now. The UE NAS
           * message will be forwarded in the corresponding reconfiguration,
           * the Initial context setup response after reconfiguration complete. */
          if (!trigger_bearer_setup(rrc, UE, UE->n_initial_pdu, UE->initial_pdus, 0)) {
            LOG_W(NR_RRC, "Failed to setup bearers for UE %d: send Initial Context Setup Response\n", UE->rrc_ue_id);
            rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
            rrc_forward_ue_nas_message(rrc, UE);
          }
        } else {
          /* we already have capabilities, and no PDU sessions to setup, ack
           * this UE */
          rrc_gNB_send_NGAP_INITIAL_CONTEXT_SETUP_RESP(rrc, UE);
          rrc_forward_ue_nas_message(rrc, UE);
        }
        break;

      case NR_UL_DCCH_MessageType__c1_PR_securityModeFailure:
        LOG_E(NR_RRC, "UE %d: received securityModeFailure\n", ue_context_p->ue_context.rrc_ue_id);
        LOG_W(NR_RRC, "Cannot continue as no AS security is activated (implementation missing)\n");
        break;

      case NR_UL_DCCH_MessageType__c1_PR_ueCapabilityInformation:
        handle_ueCapabilityInformation(rrc, UE, ul_dcch_msg->message.choice.c1->choice.ueCapabilityInformation);
        break;

      case NR_UL_DCCH_MessageType__c1_PR_rrcReestablishmentComplete:
        LOG_UE_UL_EVENT(UE, "Received RRCReestablishmentComplete\n");
        handle_rrcReestablishmentComplete(rrc, UE, ul_dcch_msg->message.choice.c1->choice.rrcReestablishmentComplete);
        break;

      default:
        break;
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NR_UL_DCCH_Message, ul_dcch_msg);
  return 0;
}

static void rrc_CU_process_ue_context_setup_failure(MessageDef *msg_p, instance_t instance)
{
  f1ap_ue_context_setup_t *fail = &F1AP_UE_CONTEXT_SETUP_FAILURE(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, fail->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", fail->gNB_CU_ue_id);
    return;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  if(ue_data.secondary_ue) {
    cu_remove_f1_ue_data(ue_data.secondary_ue);
    LOG_I(NR_RRC, "Received UE context setup failure from DU, removing(F1AP DU UE ID:%d)from rrc_du_tree\n",fail->gNB_DU_ue_id);
  } else {
    LOG_I(NR_RRC, "Could not find UE context(F1AP DU UE ID:%d) in DU \n",fail->gNB_DU_ue_id);
  }
  UE->f1_ue_context_active = false;
  rrc_gNB_remove_ue_context(rrc, ue_context_p);
  LOG_I(NR_RRC, "UE context(CU UE ID:%d) removed at CU successfully\n", fail->gNB_CU_ue_id);
  LOG_E(F1AP, "Bearer Context Setup Release not implemented for UE Context Setup Failure\n");
  return;
}

void rrc_gNB_process_initial_ul_rrc_message(sctp_assoc_t assoc_id, const f1ap_initial_ul_rrc_message_t *ul_rrc)
{
  AssertFatal(assoc_id != 0, "illegal assoc_id == 0: should be -1 (monolithic) or >0 (split)\n");

  gNB_RRC_INST *rrc = RC.nrrrc[0];
  LOG_I(NR_RRC, "Decoding CCCH: RNTI %04x, payload_size %d\n", ul_rrc->crnti, ul_rrc->rrc_container_length);
  NR_UL_CCCH_Message_t *ul_ccch_msg = NULL;
  asn_dec_rval_t dec_rval = uper_decode(NULL,
                                        &asn_DEF_NR_UL_CCCH_Message,
                                        (void **)&ul_ccch_msg,
                                        ul_rrc->rrc_container,
                                        ul_rrc->rrc_container_length,
                                        0,
                                        0);
  if (dec_rval.code != RC_OK || dec_rval.consumed == 0) {
    LOG_E(NR_RRC, " FATAL Error in receiving CCCH\n");
    return;
  }

  if (ul_ccch_msg->message.present == NR_UL_CCCH_MessageType_PR_c1) {
    switch (ul_ccch_msg->message.choice.c1->present) {
      case NR_UL_CCCH_MessageType__c1_PR_NOTHING:
        LOG_W(NR_RRC, "Received PR_NOTHING on UL-CCCH-Message, ignoring message\n");
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcSetupRequest:
        LOG_D(NR_RRC, "Received RRCSetupRequest on UL-CCCH-Message (UE rnti %04x)\n", ul_rrc->crnti);
        rrc_handle_RRCSetupRequest(rrc, assoc_id, &ul_ccch_msg->message.choice.c1->choice.rrcSetupRequest->rrcSetupRequest, ul_rrc);
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcResumeRequest:
        LOG_E(NR_RRC, "Received rrcResumeRequest message, but handling is not implemented\n");
        break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcReestablishmentRequest: {
        LOG_D(NR_RRC, "Received RRCReestablishmentRequest on UL-CCCH-Message (UE RNTI %04x)\n", ul_rrc->crnti);
        rrc_handle_RRCReestablishmentRequest(
            rrc,
            assoc_id,
            &ul_ccch_msg->message.choice.c1->choice.rrcReestablishmentRequest->rrcReestablishmentRequest,
            ul_rrc);
      } break;

      case NR_UL_CCCH_MessageType__c1_PR_rrcSystemInfoRequest:
        LOG_I(NR_RRC, "UE %04x receive rrcSystemInfoRequest message \n", ul_rrc->crnti);
        /* TODO */
        break;

      default:
        LOG_E(NR_RRC, "UE %04x Unknown message\n", ul_rrc->crnti);
        break;
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NR_UL_CCCH_Message, ul_ccch_msg);
}

void rrc_gNB_process_release_request(const module_id_t gnb_mod_idP, x2ap_ENDC_sgnb_release_request_t *m)
{
  gNB_RRC_INST *rrc = RC.nrrrc[gnb_mod_idP];
  rrc_remove_nsa_user(rrc, m->rnti);
}

void rrc_gNB_process_dc_overall_timeout(const module_id_t gnb_mod_idP, x2ap_ENDC_dc_overall_timeout_t *m)
{
  gNB_RRC_INST *rrc = RC.nrrrc[gnb_mod_idP];
  rrc_remove_nsa_user(rrc, m->rnti);
}

/* \brief fill E1 bearer modification's DRB from F1 DRB
 * \param drb_e1 pointer to a DRB inside an E1 bearer modification message
 * \param drb_f1 pointer to a DRB inside an F1 UE Ctxt modification Response */
static void fill_e1_bearer_modif(DRB_nGRAN_to_mod_t *drb_e1, const f1ap_drb_to_be_setup_t *drb_f1)
{
  drb_e1->id = drb_f1->drb_id;
  drb_e1->numDlUpParam = drb_f1->up_dl_tnl_length;
  drb_e1->DlUpParamList[0].tl_info.tlAddress = drb_f1->up_dl_tnl[0].tl_address;
  drb_e1->DlUpParamList[0].tl_info.teId = drb_f1->up_dl_tnl[0].teid;
}

/**
 * @brief Store F1-U DL TL and TEID in RRC
*/
static void f1u_dl_gtp_update(f1u_tunnel_t *f1u, const f1ap_up_tnl_t *p)
{
  f1u->teid = p->teid;
  memcpy(&f1u->addr.buffer, &p->tl_address, sizeof(uint8_t) * 4);
  f1u->addr.length = sizeof(in_addr_t);
}

/**
 * @brief Update DRB TEID information in RRC storage from received DRB list
 */
static void store_du_f1u_tunnel(const f1ap_drb_to_be_setup_t *drbs, int n, gNB_RRC_UE_t *ue)
{
  for (int i = 0; i < n; i++) {
    const f1ap_drb_to_be_setup_t *drb_f1 = &drbs[i];
    AssertFatal(drb_f1->up_dl_tnl_length == 1, "can handle only one UP param\n");
    AssertFatal(drb_f1->drb_id < MAX_DRBS_PER_UE, "illegal DRB ID %ld\n", drb_f1->drb_id);
    drb_t *drb = get_drb(ue, drb_f1->drb_id);
    f1u_dl_gtp_update(&drb->du_tunnel_config, &drb_f1->up_dl_tnl[0]);
  }
}

/*
 * @brief Store F1-U UL TEID and address in RRC
 */
static void f1u_ul_gtp_update(f1u_tunnel_t *f1u, const up_params_t *p)
{
  f1u->teid = p->tl_info.teId;
  memcpy(&f1u->addr.buffer, &p->tl_info.tlAddress, sizeof(uint8_t) * 4);
  f1u->addr.length = sizeof(in_addr_t);
}

#ifdef UNI_RAN
/* \brief use list of DRBs and send the corresponding bearer update message via
 * E1 to the CU of this UE. Also updates TEID info internally */
void e1_send_bearer_updates(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, f1ap_drb_to_be_setup_t *drbs, const ngap_drb_status_t *drb_status)
{
  // Quit bearer updates if no CU-UP is associated
  if (!is_cuup_associated(rrc)) {
    return;
  }

  // we assume the same UE ID in CU-UP and CU-CP
  e1ap_bearer_mod_req_t req = {
    .gNB_cu_cp_ue_id = UE->rrc_ue_id,
    .gNB_cu_up_ue_id = UE->rrc_ue_id,
  };

  for (int i = 0; i < MAX_DRBS_PER_UE; i++) {
    int drb_id = i + 1;
    DRB_nGRAN_to_mod_t drb_to_mod = {0};
    if (n > 0) {
      /* DRBs to Setup in F1 UE Context Modifcation Response */
      if (i == n)
        break;
      const f1ap_drb_to_be_setup_t *drb_f1 = &drbs[i];
      drb_id = drb_f1->drb_id;
      fill_e1_bearer_modif(&drb_to_mod, drb_f1);
    } else if ((UE->ho_context && UE->ho_context->source && !UE->ho_context->target) || drb_status) {
      /* On-going handover: send PDCP Status Request */
      if (UE->established_drbs[i].status == DRB_INACTIVE)
        continue;
      drb_to_mod.id = drb_id;
      // PDCP SN Status Request
      drb_to_mod.pdcp_sn_status_requested = (UE->ho_context && UE->ho_context->source && !UE->ho_context->target) ? true : false;
      // PDCP SN Status Information
      if (drb_status) {
        // If a specific drb_status was passed, only attach it to the matching DRB
        if (drb_status->drb_id != drb_id) {
          // This incoming status belongs to another DRB — skip adding status for this local DRB
          continue;
        }
        drb_to_mod.pdcp_config = calloc_or_fail(1, sizeof(*drb_to_mod.pdcp_config));
        set_bearer_context_pdcp_config(drb_to_mod.pdcp_config, get_drb(UE, drb_id), rrc->configuration.um_on_default_drb);
        drb_to_mod.pdcp_status = malloc_or_fail(sizeof(*drb_to_mod.pdcp_status));
        drb_to_mod.pdcp_status->dl_count.hfn = drb_status->dl_count.hfn;
        drb_to_mod.pdcp_status->dl_count.sn = drb_status->dl_count.pdcp_sn;
        drb_to_mod.pdcp_status->ul_count.hfn = drb_status->ul_count.hfn;
        drb_to_mod.pdcp_status->ul_count.sn = drb_status->ul_count.pdcp_sn;
      }
    }

    rrc_pdu_session_param_t *pdu = find_pduSession_from_drbId(UE, drb_id);
    if (!pdu) {
      LOG_W(RRC, "e1_send_bearer_updates: no PDU session for DRB %d, skipping\n", drb_id);
      continue;
    }
    pdu_session_to_mod_t *to_mod = find_or_next_pdu_session(&req, pdu->param.pdusession_id);
    DevAssert(to_mod);
    to_mod->sessionId = pdu->param.pdusession_id;
    // Fill E1 DRB to Modify item
    to_mod->DRBnGRanModList[to_mod->numDRB2Modify++] = drb_to_mod;
  }
  DevAssert(req.numPDUSessionsMod > 0);
  DevAssert(req.numPDUSessions == 0);

  // Always send security information
  req.secInfo = malloc_or_fail(sizeof(*req.secInfo));
  fill_security_info(rrc, UE, req.secInfo);

  // send the E1 bearer modification request message to update F1-U tunnel info
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, UE);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
}
#else
/* \brief use list of DRBs and send the corresponding bearer update message via
 * E1 to the CU of this UE. Also updates TEID info internally */
static void e1_send_bearer_updates(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, f1ap_drb_to_be_setup_t *drbs)
{
  // Quit bearer updates if no CU-UP is associated
  if (!is_cuup_associated(rrc)) {
    return;
  }

  // we assume the same UE ID in CU-UP and CU-CP
  e1ap_bearer_mod_req_t req = {
    .gNB_cu_cp_ue_id = UE->rrc_ue_id,
    .gNB_cu_up_ue_id = UE->rrc_ue_id,
  };

  for (int i = 0; i < n; i++) {
    const f1ap_drb_to_be_setup_t *drb_f1 = &drbs[i];
    rrc_pdu_session_param_t *pdu_ue = find_pduSession_from_drbId(UE, drb_f1->drb_id);
    if (pdu_ue == NULL) {
      LOG_E(RRC, "UE %d: UE Context Modif Response: no PDU session for DRB ID %ld\n", UE->rrc_ue_id, drb_f1->drb_id);
      continue;
    }
    pdu_session_to_mod_t *pdu_e1 = find_or_next_pdu_session(&req, pdu_ue->param.pdusession_id);
    DevAssert(pdu_e1 != NULL);
    pdu_e1->sessionId = pdu_ue->param.pdusession_id;
    DRB_nGRAN_to_mod_t *drb_e1 = &pdu_e1->DRBnGRanModList[pdu_e1->numDRB2Modify];
    /* Fill E1 bearer context modification */
    fill_e1_bearer_modif(drb_e1, drb_f1);
    pdu_e1->numDRB2Modify += 1;
  }
  DevAssert(req.numPDUSessionsMod > 0);
  DevAssert(req.numPDUSessions == 0);

  // Always send security information
  req.secInfo = malloc_or_fail(sizeof(*req.secInfo));
  fill_security_info(rrc, UE, req.secInfo);

  // send the E1 bearer modification request message to update F1-U tunnel info
  sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, UE);
  rrc->cucp_cuup.bearer_context_mod(assoc_id, &req);
}
#endif

static void get_measurement_configuration(instance_t instance, rrc_gNB_ue_context_t* ue_p) {
   if (ue_p->ue_context.measConfig != NULL) {
    free(ue_p->ue_context.measConfig);
    ue_p->ue_context.measConfig = NULL;
  }

  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  const nr_rrc_du_container_t *du = get_du_for_ue(rrc, ue_p->ue_context.rrc_ue_id);
  DevAssert(du != NULL);
  f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  if (du->mib != NULL && du->sib1 != NULL) {
    int scs = get_ssb_scs(cell_info);
    int band = get_dl_band(cell_info);
    const NR_MeasTimingList_t *mtlist = du->mtc->criticalExtensions.choice.c1->choice.measTimingConf->measTiming;
    const NR_MeasTiming_t *mt = mtlist->list.array[0];
    const neighbour_cell_configuration_t *neighbour_config = get_neighbour_config(cell_info->nr_cellid);
    NR_MeasConfig_t * measconfig = NULL;
    seq_arr_t *neighbour_cells = NULL;
    if (neighbour_config)
      neighbour_cells = neighbour_config->neighbour_cells;
    measconfig = get_MeasConfig(mt, band, scs, &rrc->measurementConfiguration, neighbour_cells);
    if (ue_p->ue_context.measConfig)
      free_MeasConfig(ue_p->ue_context.measConfig);
    ue_p->ue_context.measConfig = measconfig;
  }

  remove_source_gnb_measurement_configuration(ue_p);
}

static void rrc_CU_process_ue_context_setup_response(MessageDef *msg_p, instance_t instance)
{
  LOG_I(RRC, " inside cu process ue context setup response \n");
  f1ap_ue_context_setup_t *resp = &F1AP_UE_CONTEXT_SETUP_RESP(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", resp->gNB_CU_ue_id);
    return;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  UE->f1_ue_context_active = true;

  NR_CellGroupConfig_t *cellGroupConfig = NULL;
  asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                 &asn_DEF_NR_CellGroupConfig,
                                                 (void **)&cellGroupConfig,
                                                 (uint8_t *)resp->du_to_cu_rrc_information->cellGroupConfig,
                                                 resp->du_to_cu_rrc_information->cellGroupConfig_length);
  AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed > 0, "Cell group config decode error\n");

  /* if (UE->masterCellGroup) {
    ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
    LOG_I(RRC, "UE %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rnti);
  } */ // commented for Xn HO
	
  UE->masterCellGroup = cellGroupConfig;
  if (LOG_DEBUGFLAG(DEBUG_ASN1))
    xer_fprint(stdout, &asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);

  if (resp->drbs_to_be_setup_length > 0) {
    int num_drb = get_number_active_drbs(UE);
    DevAssert(num_drb == 0 || num_drb == resp->drbs_to_be_setup_length);

    /* Note: we would ideally check that SRB2 is acked, but at least LiteOn DU
     * seems buggy and does not ack, so simply check that locally we activated */
    AssertFatal(UE->Srb[1].Active && UE->Srb[2].Active, "SRBs 1 and 2 must be active during DRB Establishment");
    store_du_f1u_tunnel(resp->drbs_to_be_setup, resp->drbs_to_be_setup_length, UE);
    if (num_drb == 0) {
#ifdef UNI_RAN
      e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup, NULL);
#else
      e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup);
#endif
    }
    else
      cuup_notify_reestablishment(rrc, UE);
  }

  if (UE->ho_context == NULL && UE->ho_context_xn == NULL) {
    rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, UE);
  } else if (UE->ho_context != NULL) {
    // case of handover
    // handling of "target CU" information
    DevAssert(UE->ho_context->target != NULL);
    DevAssert(resp->crnti != NULL);
    UE->ho_context->target->du_ue_id = resp->gNB_DU_ue_id;
    UE->ho_context->target->new_rnti = *resp->crnti;
    uint8_t xid = rrc_gNB_get_next_transaction_identifier(0);
    UE->xids[xid] = RRC_DEDICATED_RECONF;
    if ( UE->ho_context->source ) {
      uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
      int size = rrc_gNB_encode_RRCReconfiguration(rrc, UE, xid, NULL, buffer, sizeof(buffer), true);
      DevAssert(size > 0 && size <= sizeof(buffer));

      // TODO N2 38.413 sec 9.3.1.21: admitted PDU sessions (in UE), Target to
      // Source Transparent Container (9.3.1.21) which encodes the RRC
      // reconfiguration above
      UE->ho_context->target->ho_req_ack(rrc, UE, buffer, size);
    } else {
      if (cu_exists_f1_ue_data(UE->rrc_ue_id)) {
        f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
	ue_data.secondary_ue = resp->gNB_DU_ue_id;
	bool success = cu_update_f1_ue_data(UE->rrc_ue_id, &ue_data);
        DevAssert(success);
      } 
        LOG_I(NR_RRC, "HO LOG: CU F1AP Context is updated for the UE with CU Id : %u DU Id : %u!\n", resp->gNB_CU_ue_id, resp->gNB_DU_ue_id);
      UE->rnti = resp->gNB_DU_ue_id; 
      uint8_t* buffer = (uint8_t*)calloc(1,NR_RRC_BUF_SIZE);
      NR_SRB_ToAddModList_t *SRBs = createSRBlist(UE, true);
      NR_DRB_ToAddModList_t *DRBs = createDRBlist(UE, true);
      get_measurement_configuration(instance, ue_context_p);
      size_t hoCommandSize = get_HandoverCommandMessage(UE, &SRBs, &DRBs, &buffer, NR_RRC_BUF_SIZE, xid);
      freeSRBlist(SRBs);
      freeDRBlist(DRBs);
      rrc_gNB_send_NGAP_HANDOVER_REQUEST_ACKNOWLEDGE(rrc, UE,&buffer, hoCommandSize);
    } 
  }else if (UE->ho_context_xn != NULL) {
    // case of xn handover
      if (cu_exists_f1_ue_data(UE->rrc_ue_id)) {
        f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
	ue_data.secondary_ue = resp->gNB_DU_ue_id;
	bool success = cu_update_f1_ue_data(UE->rrc_ue_id, &ue_data);
        DevAssert(success);
      } 
    uint8_t xid = rrc_gNB_get_next_transaction_identifier(0);
    UE->xids[xid] = RRC_DEDICATED_RECONF;
    uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
    int size = rrc_gNB_encode_RRCReconfiguration(rrc, UE, xid, NULL, buffer, sizeof(buffer), true);
    DevAssert(size > 0 && size <= sizeof(buffer));

    MessageDef *message_p = itti_alloc_new_message(TASK_XNAP, 0, XNAP_HANDOVER_REQ_ACK);
    xnap_handover_req_ack_t *ack = &XNAP_HANDOVER_REQ_ACK(message_p);
    xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(0);
    message_p->ittiMsgHeader.originInstance = instance_p->assoc_id_temp;
    ack->s_ng_node_ue_xnap_id = UE->ho_context_xn->source_xn->s_ue_xnap_id;
    ack->t_ng_node_ue_xnap_id = UE->ho_context_xn->target_xn->t_ue_xnap_id;
    ack->ue_context.pdusession_admitted_list.num_pdu = UE->nb_of_pdusessions;
    for (int i = 0; i < UE->nb_of_pdusessions; i++) {
      ack->ue_context.pdusession_admitted_list.pdu[i].pdusession_id = UE->pduSession[i].param.pdusession_id;
      ack->ue_context.pdusession_admitted_list.pdu[i].qos_list.num_qos = UE->pduSession[i].param.nb_qos;
      for (int j = 0; j < UE->pduSession[i].param.nb_qos; j++) {
        ack->ue_context.pdusession_admitted_list.pdu[i].qos_list.qos[j].qfi = UE->pduSession[i].param.qos[j].qfi;
      }
    }
    memcpy(ack->rrc_buffer, buffer, size);
    ack->rrc_buffer_size = size;
    itti_send_msg_to_task(TASK_XNAP, instance_p->instance, message_p);
  }
}

typedef struct deliver_ue_ctxt_release_data_t {
  gNB_RRC_INST *rrc;
  f1ap_ue_context_release_cmd_t *release_cmd;
  sctp_assoc_t assoc_id;
} deliver_ue_ctxt_release_data_t;
static void rrc_deliver_ue_ctxt_release_cmd(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_ue_ctxt_release_data_t *data = deliver_pdu_data;
  data->release_cmd->rrc_container = (uint8_t*) buf;
  data->release_cmd->rrc_container_length = size;
  data->rrc->mac_rrc.ue_context_release_command(data->assoc_id, data->release_cmd);
}

static void rrc_CU_process_ue_context_release_request(MessageDef *msg_p, sctp_assoc_t assoc_id)
{
  const int instance = 0;
  f1ap_ue_context_release_req_t *req = &F1AP_UE_CONTEXT_RELEASE_REQ(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, req->gNB_CU_ue_id);
  // TODO what happens if no AMF connected? should also handle, set an_release true
  if (!ue_context_p) {
    LOG_W(RRC, "could not find UE context for CU UE ID %u: auto-generate release command\n", req->gNB_CU_ue_id);
    uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
    int size = do_NR_RRCRelease(buffer, NR_RRC_BUF_SIZE, rrc_gNB_get_next_transaction_identifier(0));
    f1ap_ue_context_release_cmd_t ue_context_release_cmd = {
        .gNB_CU_ue_id = req->gNB_CU_ue_id,
        .gNB_DU_ue_id = req->gNB_DU_ue_id,
        .cause = F1AP_CAUSE_RADIO_NETWORK,
        .cause_value = 10, // 10 = F1AP_CauseRadioNetwork_normal_release
        .srb_id = DCCH,
    };
    deliver_ue_ctxt_release_data_t data = {.rrc = rrc, .release_cmd = &ue_context_release_cmd};
    nr_pdcp_data_req_srb(req->gNB_CU_ue_id, DCCH, rrc_gNB_mui++, size, buffer, rrc_deliver_ue_ctxt_release_cmd, &data);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  if (UE->ho_context != NULL) {
    nr_ho_source_cu_t *source_ctx = UE->ho_context->source;
    bool from_source_du = source_ctx && source_ctx->du->assoc_id == assoc_id;
    if (from_source_du) {
      // we received release request from the source DU, but HO is still
      // ongoing; free the UE, and remove the HO context.
      LOG_W(NR_RRC, "UE %d: release request from source DU ID %ld during HO, marking HO as complete\n", UE->rrc_ue_id, source_ctx->du->setup_req->gNB_DU_id);
      RETURN_IF_INVALID_ASSOC_ID(source_ctx->du->assoc_id);
      f1ap_ue_context_release_cmd_t cmd = {
          .gNB_CU_ue_id = UE->rrc_ue_id,
          .gNB_DU_ue_id = source_ctx->du_ue_id,
          .cause = F1AP_CAUSE_RADIO_NETWORK,
          .cause_value = 5, // 5 = F1AP_CauseRadioNetwork_interaction_with_other_procedure
          .srb_id = DL_SCH_LCID_DCCH,
      };
      rrc->mac_rrc.ue_context_release_command(assoc_id, &cmd);
      nr_rrc_finalize_ho(UE);
      return;
    }
    // if we receive the release request from the target DU (regardless if
    // successful), we assume it is "genuine" and ask the AMF to release
    nr_rrc_finalize_ho(UE);
  }

  /* TODO: marshall types correctly */
  LOG_I(NR_RRC, "received UE Context Release Request for UE %u, forwarding to AMF\n", req->gNB_CU_ue_id);
  ngap_cause_t cause = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_RADIO_CONNECTION_WITH_UE_LOST};
  rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(instance, ue_context_p, cause);
}

static void rrc_delete_ue_data(gNB_RRC_UE_t *UE)
{
  ASN_STRUCT_FREE(asn_DEF_NR_UE_NR_Capability, UE->UE_Capability_nr);
  ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
  ASN_STRUCT_FREE(asn_DEF_NR_MeasResults, UE->measResults);
  free_MeasConfig(UE->measConfig);
}

void rrc_remove_ue(gNB_RRC_INST *rrc, rrc_gNB_ue_context_t *ue_context_p)
{
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  /* we call nr_pdcp_remove_UE() in the handler of E1 bearer release, but if we
   * are in E1, we also need to free the UE in the CU-CP, so call it twice to
   * cover all cases */
  nr_pdcp_remove_UE(UE->rrc_ue_id);
  LOG_I(NR_RRC, "removed UE CU UE ID %u/RNTI %04x \n", UE->rrc_ue_id, UE->rnti);
  rrc_delete_ue_data(UE);
  rrc_gNB_remove_ue_context(rrc, ue_context_p);
}

static void rrc_CU_process_ue_context_release_complete(MessageDef *msg_p)
{
  const int instance = 0;
  f1ap_ue_context_release_complete_t *complete = &F1AP_UE_CONTEXT_RELEASE_COMPLETE(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, complete->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", complete->gNB_CU_ue_id);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  if (UE->an_release) {
    /* only trigger release if it has been requested by core
     * otherwise, it might be CU that requested release on a DU during normal
     * operation (i.e, handover) */
    uint32_t pdu_sessions[NGAP_MAX_PDU_SESSION];
    get_pduSession_array(UE, pdu_sessions);
    rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_COMPLETE(0, UE->rrc_ue_id, UE->nb_of_pdusessions, pdu_sessions);
    rrc_remove_ue(RC.nrrrc[0], ue_context_p);
  }
}

static void rrc_CU_process_ue_context_modification_response(MessageDef *msg_p, instance_t instance)
{
  f1ap_ue_context_modif_resp_t *resp = &F1AP_UE_CONTEXT_MODIFICATION_RESP(msg_p);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_CU_ue_id);
  if (!ue_context_p) {
    LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", resp->gNB_CU_ue_id);
    return;
  }
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  if (resp->drbs_to_be_setup_length > 0 || (UE->ho_context && UE->ho_context->source && !UE->ho_context->target)) {
    store_du_f1u_tunnel(resp->drbs_to_be_setup, resp->drbs_to_be_setup_length, UE);
#ifdef UNI_RAN
    e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup, NULL);
#else
    e1_send_bearer_updates(rrc, UE, resp->drbs_to_be_setup_length, resp->drbs_to_be_setup);
#endif
  }

  if (resp->du_to_cu_rrc_information != NULL && resp->du_to_cu_rrc_information->cellGroupConfig != NULL) {
    NR_CellGroupConfig_t *cellGroupConfig = NULL;
    asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                   &asn_DEF_NR_CellGroupConfig,
                                                   (void **)&cellGroupConfig,
                                                   (uint8_t *)resp->du_to_cu_rrc_information->cellGroupConfig,
                                                   resp->du_to_cu_rrc_information->cellGroupConfig_length);
    AssertFatal(dec_rval.code == RC_OK && dec_rval.consumed > 0, "Cell group config decode error\n");

    if (UE->masterCellGroup) {
      ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
      LOG_I(RRC, "UE %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rnti);
    }
    UE->masterCellGroup = cellGroupConfig;

    rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, UE);
  }

  // Reconfiguration should have been sent to the UE, so it will attempt the
  // handover. In the F1 case, update with new RNTI, and update secondary UE
  // association, so we can receive the new UE from the target DU (in N2/Xn,
  // nothing is to be done, we wait for confirmation to release the UE in the
  // CU/DU)
  if (UE->ho_context && UE->ho_context->target && UE->ho_context->source) {
    nr_ho_target_cu_t *target_ctx = UE->ho_context->target;
    f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
    ue_data.secondary_ue = target_ctx->du_ue_id;
    ue_data.du_assoc_id = target_ctx->du->assoc_id;
    bool success = cu_update_f1_ue_data(UE->rrc_ue_id, &ue_data);
    DevAssert(success);
    LOG_I(NR_RRC, "UE %d handover: update RNTI from %04x to %04x\n", UE->rrc_ue_id, UE->rnti, target_ctx->new_rnti);
    nr_ho_source_cu_t *source_ctx = UE->ho_context->source;
    DevAssert(source_ctx->old_rnti == UE->rnti);
    UE->rnti = target_ctx->new_rnti;
    UE->nr_cellid = target_ctx->du->setup_req->cell[0].info.nr_cellid;
  }
}

static void rrc_CU_process_ue_modification_required(MessageDef *msg_p,instance_t instance, sctp_assoc_t assoc_id)
{
  LOG_I(RRC, " modification required triggered \n");
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  f1ap_ue_context_modif_required_t *required = &F1AP_UE_CONTEXT_MODIFICATION_REQUIRED(msg_p);
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, required->gNB_CU_ue_id);
  if (ue_context_p == NULL) {
    LOG_E(RRC, "Could not find UE context for CU UE ID %d, cannot handle UE context modification request\n", required->gNB_CU_ue_id);
    f1ap_ue_context_modif_refuse_t refuse = {
      .gNB_CU_ue_id = required->gNB_CU_ue_id,
      .gNB_DU_ue_id = required->gNB_DU_ue_id,
      .cause = F1AP_CAUSE_RADIO_NETWORK,
      .cause_value = F1AP_CauseRadioNetwork_unknown_or_already_allocated_gnb_cu_ue_f1ap_id,
    };
    rrc->mac_rrc.ue_context_modification_refuse(msg_p->ittiMsgHeader.originInstance, &refuse);
    return;
  }

  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  if (UE->ho_context && UE->ho_context->source && UE->ho_context->source->du && UE->ho_context->source->du->assoc_id == assoc_id) {
    LOG_W(NR_RRC, "UE %d: UE Context Modification Required during handover, ignoring message\n", UE->rrc_ue_id);
    return;
  }

  if (required->du_to_cu_rrc_information && required->du_to_cu_rrc_information->cellGroupConfig) {
    gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
    LOG_I(RRC,
          "UE Context Modification Required: new CellGroupConfig for UE ID %d/RNTI %04x, triggering reconfiguration\n",
          UE->rrc_ue_id,
          UE->rnti);

    NR_CellGroupConfig_t *cellGroupConfig = NULL;
    asn_dec_rval_t dec_rval = uper_decode_complete(NULL,
                                                   &asn_DEF_NR_CellGroupConfig,
                                                   (void **)&cellGroupConfig,
                                                   (uint8_t *)required->du_to_cu_rrc_information->cellGroupConfig,
                                                   required->du_to_cu_rrc_information->cellGroupConfig_length);
    if (dec_rval.code != RC_OK && dec_rval.consumed == 0) {
      LOG_E(RRC, "Cell group config decode error, refusing reconfiguration\n");
      f1ap_ue_context_modif_refuse_t refuse = {
        .gNB_CU_ue_id = required->gNB_CU_ue_id,
        .gNB_DU_ue_id = required->gNB_DU_ue_id,
        .cause = F1AP_CAUSE_PROTOCOL,
        .cause_value = F1AP_CauseProtocol_transfer_syntax_error,
      };
      rrc->mac_rrc.ue_context_modification_refuse(msg_p->ittiMsgHeader.originInstance, &refuse);
      return;
    }

    if (UE->masterCellGroup) {
      ASN_STRUCT_FREE(asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);
      LOG_I(RRC, "UE %d/RNTI %04x replacing existing CellGroupConfig with new one received from DU\n", UE->rrc_ue_id, UE->rnti);
    }
    UE->masterCellGroup = cellGroupConfig;
    if (LOG_DEBUGFLAG(DEBUG_ASN1))
      xer_fprint(stdout, &asn_DEF_NR_CellGroupConfig, UE->masterCellGroup);

    /* trigger reconfiguration */
    nr_rrc_reconfiguration_req(rrc, UE, 0, 0);
    return;
  }
  LOG_W(RRC,
        "nothing to be done after UE Context Modification Required for UE ID %d/RNTI %04x\n",
        required->gNB_CU_ue_id,
        required->gNB_DU_ue_id);
}

unsigned int mask_flip(unsigned int x) {
  return((((x>>8) + (x<<8))&0xffff)>>6);
}

pdusession_level_qos_parameter_t *get_qos_characteristics(const int qfi, rrc_pdu_session_param_t *pduSession)
{
  pdusession_t *pdu = &pduSession->param;
  for (int i = 0; i < pdu->nb_qos; i++) {
    if (qfi == pdu->qos[i].qfi)
      return &pdu->qos[i];
  }
  AssertFatal(1 == 0, "The pdu session %d does not contain a qos flow with qfi = %d\n", pdu->pdusession_id, qfi);
  return NULL;
}

/* \bref return qos characteristics based on Qos flow parameters */
qos_characteristics_t get_qos_char_from_qos_flow_param(const pdusession_level_qos_parameter_t *qos_param)
{
  qos_characteristics_t qos_char = {0};
  if (qos_param->fiveQI_type == DYNAMIC_5QI) {
    qos_char.qos_type = DYNAMIC_5QI;
    qos_char.dynamic.fiveqi = qos_param->fiveQI;
    qos_char.dynamic.qos_priority_level = qos_param->qos_priority;
  } else {
    qos_char.qos_type = NON_DYNAMIC_5QI;
    qos_char.non_dynamic.fiveqi = qos_param->fiveQI;
    qos_char.non_dynamic.qos_priority_level = qos_param->qos_priority;
  }
  return qos_char;
}

/* \brief fills a list of DRBs to be setup from a number of PDU sessions in E1
 * bearer setup response */
static int fill_drb_to_be_setup_from_e1_resp(const gNB_RRC_INST *rrc,
                                             gNB_RRC_UE_t *UE,
                                             const pdu_session_setup_t *pduSession,
                                             int numPduSession,
                                             f1ap_drb_to_be_setup_t drbs[32])
{
  int nb_drb = 0;
  for (int p = 0; p < numPduSession; ++p) {
    rrc_pdu_session_param_t *RRC_pduSession = find_pduSession(UE, pduSession[p].id, false);
    DevAssert(RRC_pduSession);
    for (int i = 0; i < pduSession[p].numDRBSetup; i++) {
      const DRB_nGRAN_setup_t *drb_config = &pduSession[p].DRBnGRanList[i];
      f1ap_drb_to_be_setup_t *drb = &drbs[nb_drb];
      drb->drb_id = pduSession[p].DRBnGRanList[i].id;
      drb->rlc_mode = rrc->configuration.um_on_default_drb ? F1AP_RLC_MODE_UM_BIDIR : F1AP_RLC_MODE_AM;
      drb->up_ul_tnl[0].tl_address = drb_config->UpParamList[0].tl_info.tlAddress;
      drb->up_ul_tnl[0].port = rrc->eth_params_s.my_portd;
      drb->up_ul_tnl[0].teid = drb_config->UpParamList[0].tl_info.teId;
      drb->up_ul_tnl_length = 1;

      /* pass QoS info to MAC */
      int nb_qos_flows = drb_config->numQosFlowSetup;
      AssertFatal(nb_qos_flows > 0, "must map at least one flow to a DRB\n");
      drb->drb_info.flows_to_be_setup_length = nb_qos_flows;
      drb->drb_info.flows_mapped_to_drb = calloc(nb_qos_flows, sizeof(f1ap_flows_mapped_to_drb_t));
      AssertFatal(drb->drb_info.flows_mapped_to_drb, "could not allocate memory\n");
      for (int j = 0; j < nb_qos_flows; j++) {
        drb->drb_info.flows_mapped_to_drb[j].qfi = drb_config->qosFlows[j].qfi;
        pdusession_level_qos_parameter_t *in_qos_char = get_qos_characteristics(drb_config->qosFlows[j].qfi, RRC_pduSession);
        qos_flow_level_qos_parameters_t *qos_params = &drb->drb_info.flows_mapped_to_drb[j].qos_params;
        qos_characteristics_t *qos_char = &qos_params->qos_characteristics;

        if (in_qos_char->fiveQI_type == DYNAMIC_5QI) {
          qos_char->qos_type = DYNAMIC_5QI;
          qos_char->dynamic.fiveqi = in_qos_char->fiveQI;
          qos_char->dynamic.qos_priority_level = in_qos_char->qos_priority;
        } else {
          qos_char->qos_type = NON_DYNAMIC_5QI;
          qos_char->non_dynamic.fiveqi = in_qos_char->fiveQI;
          qos_char->non_dynamic.qos_priority_level = in_qos_char->qos_priority;
        }

        gbr_qos_flow_information_t *gbr_qos_info = &in_qos_char->gbr_qos_flow_level_qos_params;
        asn1cCalloc(qos_params->gbr_qos_flow_info, gbr_info);
        memcpy(gbr_info, gbr_qos_info, sizeof(gbr_qos_flow_information_t));
      }

      /* the DRB QoS parameters: we just reuse the ones from the first flow */
      get_drb_characteristics((qos_flow_to_setup_t *)drb->drb_info.flows_mapped_to_drb,
                              drb->drb_info.flows_to_be_setup_length,
                              &drb->drb_info.drb_qos);

      /* pass NSSAI info to MAC */
      drb->nssai = RRC_pduSession->param.nssai;

      nb_drb++;
    }
  }
  return nb_drb;
}

/**
 * @brief E1AP Bearer Context Setup Response processing on CU-CP
*/
void rrc_gNB_process_e1_bearer_context_setup_resp(e1ap_bearer_setup_resp_t *resp, instance_t instance)
{
  LOG_I(RRC, "E1: process e1 bearer context setup response - %d\n", resp->numPDUSessions);
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_cu_cp_ue_id);
  AssertFatal(ue_context_p != NULL, "did not find UE with CU UE ID %d\n", resp->gNB_cu_cp_ue_id);
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  // currently: we don't have "infrastructure" to save the CU-UP UE ID, so we
  // assume (and below check) that CU-UP UE ID == CU-CP UE ID
  AssertFatal(resp->gNB_cu_cp_ue_id == resp->gNB_cu_up_ue_id,
              "cannot handle CU-UP UE ID different from CU-CP UE ID (%d vs %d)\n",
              resp->gNB_cu_cp_ue_id,
              resp->gNB_cu_up_ue_id);

  // save the tunnel address for the PDU sessions
  for (int i = 0; i < resp->numPDUSessions; i++) {
    pdu_session_setup_t *e1_pdu = &resp->pduSession[i];
    rrc_pdu_session_param_t *rrc_pdu = find_pduSession(UE, e1_pdu->id, false);
    if (rrc_pdu == NULL) {
      LOG_W(RRC, "E1: received setup for PDU session %ld, but has not been requested\n", e1_pdu->id);
      continue;
    }
    rrc_pdu->param.gNB_teid_N3 = e1_pdu->tl_info.teId;
    memcpy(&rrc_pdu->param.gNB_addr_N3.buffer, &e1_pdu->tl_info.tlAddress, sizeof(uint8_t) * 4);
    rrc_pdu->param.gNB_addr_N3.length = sizeof(in_addr_t);

    // save the tunnel address for the DRBs
    for (int i = 0; i < e1_pdu->numDRBSetup; i++) {
      DRB_nGRAN_setup_t *drb_config = &e1_pdu->DRBnGRanList[i];
      // numUpParam only relevant in F1, but not monolithic
      // AssertFatal(drb_config->numUpParam <= 1, "can only up to one UP param\n");
      drb_t *drb = get_drb(UE, drb_config->id);
      f1u_ul_gtp_update(&drb->cuup_tunnel_config, &drb_config->UpParamList[0]);
    }
  }

  /* Instruction towards the DU for DRB configuration and tunnel creation */
  f1ap_drb_to_be_setup_t drbs[MAX_DRBS_PER_UE]; // maximum DRB can be 32
  int nb_drb = fill_drb_to_be_setup_from_e1_resp(rrc, UE, resp->pduSession, resp->numPDUSessions, drbs);

  if (!UE->f1_ue_context_active)
    rrc_gNB_generate_UeContextSetupRequest(rrc, ue_context_p, nb_drb, drbs);
  else
    rrc_gNB_generate_UeContextModificationRequest(rrc, ue_context_p, nb_drb, drbs, 0, NULL);
}

/**
 * @brief E1AP Bearer Context Modification Response processing on CU-CP
 */
void rrc_gNB_process_e1_bearer_context_modif_resp(const e1ap_bearer_modif_resp_t *resp)
{
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_cu_cp_ue_id);
  gNB_RRC_UE_t UE = ue_context_p->ue_context;
  if (ue_context_p == NULL) {
    LOG_E(RRC, "no UE with CU-CP UE ID %d found\n", resp->gNB_cu_cp_ue_id);
    return;
  }
#ifdef UNI_RAN
  e1_pdcp_status_info_t collected_pdcp_status[MAX_DRBS_PER_UE];
  int collected_drb_ids[MAX_DRBS_PER_UE];
  int nDRBcollected = 0;
#endif
  // there is not really anything to do here as of now
  for (int i = 0; i < resp->numPDUSessionsMod; ++i) {
    const pdu_session_modif_t *pdu = &resp->pduSessionMod[i];
    LOG_I(RRC, "UE %d: PDU session ID %ld modified %d bearers\n", resp->gNB_cu_cp_ue_id, pdu->id, pdu->numDRBModified);
#ifdef UNI_RAN
    for (int  j = 0; j < pdu->numDRBModified; j++) {
      // Trigger UL RAN Status Transfer
      if (pdu->DRBnGRanModList[j].pdcp_status) {
        LOG_I(NR_RRC, "UE %d: received PDU Status Info - send UL RAN Status Transfer\n", resp->gNB_cu_cp_ue_id);
        int drb_id = pdu->DRBnGRanModList[j].id;
        // check if this DRB is already collected
        bool already_collected = false;
        for (int k = 0; k < nDRBcollected; k++) {
          if (collected_drb_ids[k] == drb_id) {
            already_collected = true;
              break;
          }
        }
        if (already_collected) {
          LOG_W(NR_RRC, "UE %d: DRB ID %d already collected for UL RAN Status Transfer, skipping duplicate\n", resp->gNB_cu_cp_ue_id, drb_id);
          continue;
        }
        collected_pdcp_status[nDRBcollected] = *pdu->DRBnGRanModList[j].pdcp_status;
        collected_drb_ids[nDRBcollected] = drb_id;
        nDRBcollected++;
      }
    }
#endif
  }
#ifdef UNI_RAN
  // send once per UE if at least one DRB had PDCP status
  if (nDRBcollected > 0) {
    rrc_gNB_send_NGAP_ul_ran_status_transfer(rrc, &ue_context_p->ue_context,
                                             nDRBcollected, collected_drb_ids, collected_pdcp_status);
  }
#endif

#ifndef UNI_RAN
  if (resp->sn_status_req) {
    MessageDef *message_p = itti_alloc_new_message(TASK_RRC_GNB, 0, XNAP_SN_STATUS_TRANSFER);

    xnap_xn_sn_status_transfer_t *snstatus = &XNAP_SN_STATUS_TRANSFER(message_p);
    xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(0);
    message_p->ittiMsgHeader.originInstance = 
                         xnap_gNB_get_assoc_id(instance_p, my_neighbour_cellid); 


    snstatus->s_ng_node_ue_xnap_id = UE.ho_context_xn->source_xn->s_ue_xnap_id;
    snstatus->t_ng_node_ue_xnap_id = UE.ho_context_xn->target_xn->t_ue_xnap_id;

    for (int i = 0; i < resp->numPDUSessionsMod; ++i) {
    const pdu_session_modif_t *pdu = &resp->pduSessionMod[i];

    snstatus->drb_list.num_drb = pdu->numDRBModified;
    
    for (int j = 0; j < pdu->numDRBModified; ++j){
      const DRB_nGRAN_modified_t *drb = &pdu->DRBnGRanModList[j];

    	snstatus->drb_list.drb[i].drb_id = drb->id;
    	
       /* 12 bit */
       /* 
    	snstatus->drb_list.drb[i].pdcp_status_dl.pdcp_sn_12bit.count_value.pdcp_sn12 = drb->pdcp_sn_status_info.pdcp_status_transfer_dl.pdcp_sn;
   	snstatus->drb_list.drb[i].pdcp_status_dl.pdcp_sn_12bit.count_value.hfn_pdcp_sn12 = drb->pdcp_sn_status_info.pdcp_status_transfer_dl.hfn;
   	
   	snstatus->drb_list.drb[i].pdcp_status_ul.pdcp_sn_12bit.count_value.pdcp_sn12 = drb->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.pdcp_sn;
   	snstatus->drb_list.drb[i].pdcp_status_ul.pdcp_sn_12bit.count_value.hfn_pdcp_sn12 = drb->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.hfn;
    
       */ /* End of 12 bit */

    	snstatus->drb_list.drb[i].pdcp_status_dl.pdcp_sn_18bit.count_value.pdcp_sn18 = drb->pdcp_sn_status_info.pdcp_status_transfer_dl.pdcp_sn;
   	snstatus->drb_list.drb[i].pdcp_status_dl.pdcp_sn_18bit.count_value.hfn_pdcp_sn18 = drb->pdcp_sn_status_info.pdcp_status_transfer_dl.hfn;
   	
   	snstatus->drb_list.drb[i].pdcp_status_ul.pdcp_sn_18bit.count_value.pdcp_sn18 = drb->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.pdcp_sn;
   	snstatus->drb_list.drb[i].pdcp_status_ul.pdcp_sn_18bit.count_value.hfn_pdcp_sn18 = drb->pdcp_sn_status_info.pdcp_status_transfer_ul.countValue.hfn;
    
    }
   }
   itti_send_msg_to_task(TASK_XNAP, 0, message_p);
  }
#endif
}

/**
 * @brief E1AP Bearer Context Release processing
 */
void rrc_gNB_process_e1_bearer_context_release_cplt(const e1ap_bearer_release_cplt_t *cplt)
{
  // there is not really anything to do here as of now
  // note that we don't check for the UE: it does not exist anymore if the F1
  // UE context release complete arrived from the DU first, after which we free
  // the UE context
  LOG_I(RRC, "UE %d: received bearer release complete\n", cplt->gNB_cu_cp_ue_id);
}

void rrc_gNB_process_xn_setup_request(sctp_assoc_t assoc_id, xnap_setup_req_t *m, instance_t instance)
{
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  xnap_gNB_data_t *xnap_gnb_data_p;
  if (rrc->num_neighs > MAX_NUM_NR_NEIGH_CELLs) {
    LOG_E(NR_RRC, "Error: number of neighbouring cells is exceeded \n");
    MessageDef *msg = itti_alloc_new_message(TASK_RRC_GNB, 0, XNAP_SETUP_FAILURE);
    msg->ittiMsgHeader.originInstance = assoc_id;
    xnap_setup_failure_t *xnap_msg = &XNAP_SETUP_FAILURE(msg);
    xnap_msg->cause_type = XNAP_CAUSE_PROTOCOL;
    xnap_msg->cause_value = 6; // XNAP_CauseProtocol_unspecified;
    itti_send_msg_to_task(TASK_XNAP, 0, msg);
  }
  nr_rrc_neighcells_container_t *neigh_cell = malloc(sizeof(*neigh_cell));
  AssertFatal(neigh_cell, "out of memory\n");
  neigh_cell->assoc_id = assoc_id;
  neigh_cell->nr_cellid = m->info.nr_cellid;
  
  my_neighbour_cellid = m->info.nr_cellid;

  RB_INSERT(rrc_neigh_cell_tree, &rrc->neighs, neigh_cell);
  rrc->num_neighs++;
  xnap_gnb_data_p = xnap_get_gNB(instance, assoc_id);
  xnap_gnb_data_p->nci = m->info.nr_cellid;
  xnap_insert_gnb(instance, xnap_gnb_data_p);
  xnap_dump_trees(instance);

  MessageDef *msg = itti_alloc_new_message(TASK_RRC_GNB, 0, XNAP_SETUP_RESP);
  msg->ittiMsgHeader.originInstance = assoc_id;
  xnap_gNB_instance_t *xnap_gNB_get_instance(instance_t instanceP);
  xnap_setup_resp_t *xnap_msg = &XNAP_SETUP_RESP(msg);
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  xnap_msg->gNB_id = instance_p->setup_req.gNB_id;
  xnap_msg->tai_support = instance_p->setup_req.tai_support;
  xnap_msg->info = instance_p->setup_req.info;
  instance_p->mcc = m->info.plmn.mcc;
  instance_p->mnc = m->info.plmn.mnc;
  instance_p->mnc_len = m->info.plmn.mnc_digit_length;
  instance_p->nr_cellid = m->info.nr_cellid;
  itti_send_msg_to_task(TASK_XNAP, 0, msg);
}

void rrc_gNB_process_xn_setup_response(sctp_assoc_t assoc_id, xnap_setup_resp_t *m)
{
  instance_t instance = 0;
  xnap_gNB_data_t *xnap_gnb_data_p;
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  if (rrc->num_neighs > MAX_NUM_NR_NEIGH_CELLs) {
    LOG_E(NR_RRC, "Error: number of neighbouring cells is exceeded \n");
    return;
  }
  xnap_gnb_data_p = xnap_get_gNB(instance, assoc_id);
  xnap_gnb_data_p->nci = m->info.nr_cellid;
  xnap_insert_gnb(instance, xnap_gnb_data_p);
  xnap_dump_trees(instance);
  nr_rrc_neighcells_container_t *neigh_cell = malloc(sizeof(*neigh_cell));
  AssertFatal(neigh_cell, "out of memory\n");
  neigh_cell->assoc_id = assoc_id;
  neigh_cell->nr_cellid = m->info.nr_cellid;
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  instance_p->mcc = m->info.plmn.mcc;
  instance_p->mnc = m->info.plmn.mnc;
  instance_p->mnc_len = m->info.plmn.mnc_digit_length;
  RB_INSERT(rrc_neigh_cell_tree, &RC.nrrrc[0]->neighs, neigh_cell);
  my_neighbour_cellid = m->info.nr_cellid;
  instance_p->nr_cellid = m->info.nr_cellid;
  rrc->num_neighs++;
}

static void print_rrc_meas(FILE *f, const NR_MeasResults_t *measresults)
{
  DevAssert(measresults->measResultServingMOList.list.count >= 1);
  if (measresults->measResultServingMOList.list.count > 1)
    LOG_W(RRC, "Received %d MeasResultServMO, but handling only 1!\n", measresults->measResultServingMOList.list.count);

  NR_MeasResultServMO_t *measresultservmo = measresults->measResultServingMOList.list.array[0];
  NR_MeasResultNR_t *measresultnr = &measresultservmo->measResultServingCell;
  NR_MeasQuantityResults_t *mqr = measresultnr->measResult.cellResults.resultsSSB_Cell;

  fprintf(f, "    servingCellId %ld MeasResultNR for phyCellId %ld:\n      resultSSB:", measresultservmo->servCellId, *measresultnr->physCellId);
  if (mqr != NULL) {
    if (mqr->rsrp) {
      const long rrsrp = *mqr->rsrp - 156;
      fprintf(f, "RSRP %ld dBm ", rrsrp);
    }
    if (mqr->rsrq) {
      const float rrsrq = (float)(*mqr->rsrq - 87) / 2.0f;
      fprintf(f, "RSRQ %.1f dB ", rrsrq);
    }
    if (mqr->sinr) {
      const float rsinr = (float)(*mqr->sinr - 46) / 2.0f;
      fprintf(f, "SINR %.1f dB ", rsinr);
    }
    fprintf(f, "\n");
  } else {
    fprintf(f, "NOT PROVIDED\n");
  }

  if (measresults->measResultNeighCells
      && measresults->measResultNeighCells->present == NR_MeasResults__measResultNeighCells_PR_measResultListNR) {
    NR_MeasResultListNR_t *meas_neigh = measresults->measResultNeighCells->choice.measResultListNR;
    for (int i = 0; i < meas_neigh->list.count; ++i) {
      NR_MeasResultNR_t *measresultneigh = meas_neigh->list.array[i];
      NR_MeasQuantityResults_t *neigh_mqr = measresultneigh->measResult.cellResults.resultsSSB_Cell;
      fprintf(f, "    neighboring cell for phyCellId %ld:\n      resultSSB:", *measresultneigh->physCellId);
      if (mqr != NULL) {
        const long rrsrp = *neigh_mqr->rsrp - 156;
        const float rrsrq = (float)(*neigh_mqr->rsrq - 87) / 2.0f;
        const float rsinr = (float)(*neigh_mqr->sinr - 46) / 2.0f;
        fprintf(f, "RSRP %ld dBm RSRQ %.1f dB SINR %.1f dB\n", rrsrp, rrsrq, rsinr);
      } else {
        fprintf(f, "NOT PROVIDED\n");
      }
    }
  }
}

static const char *get_pdusession_status_text(pdu_session_status_t status)
{
  switch (status) {
    case PDU_SESSION_STATUS_NEW: return "new";
    case PDU_SESSION_STATUS_DONE: return "done";
    case PDU_SESSION_STATUS_ESTABLISHED: return "established";
    case PDU_SESSION_STATUS_REESTABLISHED: return "reestablished";
    case PDU_SESSION_STATUS_TOMODIFY: return "to-modify";
    case PDU_SESSION_STATUS_FAILED: return "failed";
    case PDU_SESSION_STATUS_TORELEASE: return "to-release";
    case PDU_SESSION_STATUS_RELEASED: return "released";
    default: AssertFatal(false, "illegal PDU status code %d\n", status); return "illegal";
  }
  return "illegal";
}

static void write_rrc_stats(const gNB_RRC_INST *rrc)
{
  const char *filename = "nrRRC_stats.log";
  FILE *f = fopen(filename, "w");
  if (f == NULL) {
    LOG_E(NR_RRC, "cannot open %s for writing\n", filename);
    return;
  }

  time_t now = time(NULL);
  int i = 0;
  rrc_gNB_ue_context_t *ue_context_p = NULL;
  /* cast is necessary to eliminate warning "discards ‘const’ qualifier" */
  RB_FOREACH(ue_context_p, rrc_nr_ue_tree_s, &((gNB_RRC_INST *)rrc)->rrc_ue_head)
  {
    const gNB_RRC_UE_t *ue_ctxt = &ue_context_p->ue_context;
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_ctxt->rrc_ue_id);

    fprintf(f,
            "UE %d CU UE ID %u DU UE ID %d RNTI %04x random identity %016lx",
            i,
            ue_ctxt->rrc_ue_id,
            ue_data.secondary_ue,
            ue_ctxt->rnti,
            ue_ctxt->random_ue_identity);
    if (ue_ctxt->Initialue_identity_5g_s_TMSI.presence)
      fprintf(f, " S-TMSI %x", ue_ctxt->Initialue_identity_5g_s_TMSI.fiveg_tmsi);
    fprintf(f, ":\n");

    time_t last_seen = now - ue_ctxt->last_seen;
    fprintf(f, "    last RRC activity: %ld seconds ago\n", last_seen);

    if (ue_ctxt->nb_of_pdusessions == 0)
      fprintf(f, "    (no PDU sessions)\n");
    for (int nb_pdu = 0; nb_pdu < ue_ctxt->nb_of_pdusessions; ++nb_pdu) {
      const rrc_pdu_session_param_t *pdu = &ue_ctxt->pduSession[nb_pdu];
      fprintf(f, "    PDU session %d ID %d status %s\n", nb_pdu, pdu->param.pdusession_id, get_pdusession_status_text(pdu->status));
    }

    fprintf(f, "    associated DU: ");
    if (ue_data.du_assoc_id == -1)
      fprintf(f, " (local/integrated CU-DU)");
    else if (ue_data.du_assoc_id == 0)
      fprintf(f, " DU offline/unavailable");
    else
      fprintf(f, " DU assoc ID %d", ue_data.du_assoc_id);
    fprintf(f, "\n");

    if (ue_ctxt->measResults)
      print_rrc_meas(f, ue_ctxt->measResults);
    ++i;
  }

  fprintf(f, "\n");
  dump_du_info(rrc, f);

  fclose(f);
}

void rrc_gNB_process_handover_request(sctp_assoc_t assoc_id, xnap_handover_req_t *m)
{
  rrc_gNB_ue_context_t *ue_context_p;
  instance_t instance;
  instance = 0;
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  // allocate new UE
  ue_context_p = rrc_gNB_allocate_new_ue_context(RC.nrrrc[0]);
  LOG_D(NR_RRC, "RRC UE ID in target %d \n", ue_context_p->ue_context.rrc_ue_id);
  if (ue_context_p == NULL) {
    LOG_E(NR_RRC, "Cannot create new UE context\n");
    return;
  }
  RB_INSERT(rrc_nr_ue_tree_s, &rrc->rrc_ue_head, ue_context_p);
  nr_rrc_du_container_t *target_du = find_target_du_xn(rrc, m->target_cgi.cgi);
  nr_handover_context_xn_t *ho_ctxt = calloc(1, sizeof(*ho_ctxt)); // allocate handover context for xn
  ho_ctxt->source_xn = calloc(1, sizeof(*ho_ctxt->source_xn));
  ho_ctxt->target_xn = calloc(1, sizeof(*ho_ctxt->target_xn));
  ho_ctxt->target_xn->du = calloc(1, sizeof(*ho_ctxt->target_xn->du));
  ho_ctxt->target_xn->du->assoc_id = target_du->assoc_id;
  ho_ctxt->source_xn->du = calloc(1, sizeof(*ho_ctxt->source_xn->du));
  ho_ctxt->source_xn->du->assoc_id = assoc_id;
  f1_ue_data_t ue_data;
  ue_data.secondary_ue = 0;
  ue_data.du_assoc_id = target_du->assoc_id;
  cu_add_f1_ue_data(ue_context_p->ue_context.rrc_ue_id, &ue_data);
  ue_context_p->ue_context.ue_guami.mcc = m->plmn_id.mcc;
  ue_context_p->ue_context.ue_guami.mnc = m->plmn_id.mnc;
  ue_context_p->ue_context.ue_guami.mnc_len = m->plmn_id.mnc_digit_length;
  ue_context_p->ue_context.ue_guami.amf_region_id = m->guami.amf_region_id;
  ue_context_p->ue_context.ue_guami.amf_set_id = m->guami.amf_set_id;
  ue_context_p->ue_context.ue_guami.amf_pointer = m->guami.amf_pointer;
  ue_context_p->ue_context.security_capabilities.nRencryption_algorithms =
      m->ue_context.security_capabilities.nRencryption_algorithms;
  ue_context_p->ue_context.security_capabilities.nRintegrity_algorithms =
      m->ue_context.security_capabilities.nRintegrity_algorithms;
  ue_context_p->ue_context.security_capabilities.eUTRAencryption_algorithms =
      m->ue_context.security_capabilities.eUTRAencryption_algorithms;
  ue_context_p->ue_context.security_capabilities.eUTRAintegrity_algorithms =
      m->ue_context.security_capabilities.eUTRAintegrity_algorithms;
  ue_context_p->ue_context.kgnb_ncc = m->ue_context.as_security_ncc;
  memcpy(ue_context_p->ue_context.kgnb, &m->ue_context.as_security_key_ranstar, 32);
  ue_context_p->ue_context.amf_ue_ngap_id = m->ue_context.ngc_ue_sig_ref;
  ue_context_p->ue_context.n_initial_pdu = m->ue_context.pdusession_tobe_setup_list.num_pdu;
  ue_context_p->ue_context.nb_of_pdusessions = m->ue_context.pdusession_tobe_setup_list.num_pdu;
  ue_context_p->ue_context.initial_pdus =
      calloc(ue_context_p->ue_context.n_initial_pdu, sizeof(*ue_context_p->ue_context.initial_pdus));
  for (int i = 0; i < ue_context_p->ue_context.n_initial_pdu; i++) {
    ue_context_p->ue_context.pduSession[i].param.pdusession_id = m->ue_context.pdusession_tobe_setup_list.pdu[i].pdusession_id;
    ue_context_p->ue_context.initial_pdus->pdusession_id = m->ue_context.pdusession_tobe_setup_list.pdu[i].pdusession_id;
    ue_context_p->ue_context.initial_pdus->nssai.sst = m->ue_context.pdusession_tobe_setup_list.pdu[i].snssai.sst;
    ue_context_p->ue_context.initial_pdus->pdu_session_type = m->ue_context.pdusession_tobe_setup_list.pdu[i].pdu_session_type;
    ue_context_p->ue_context.initial_pdus->gtp_teid = m->ue_context.pdusession_tobe_setup_list.pdu[i].up_ngu_tnl_teid_upf;
    memcpy(&ue_context_p->ue_context.initial_pdus->upf_addr.buffer, &m->ue_context.tnl_ip_source, sizeof(uint8_t) * 4);
    ue_context_p->ue_context.pduSession[i].status = PDU_SESSION_STATUS_REESTABLISHED;
    ue_context_p->ue_context.initial_pdus->nb_qos = m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.num_qos;
    ue_context_p->ue_context.pduSession[i].param.nb_qos = m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.num_qos;
    for (int j = 0; j < ue_context_p->ue_context.initial_pdus->nb_qos; j++) {
      ue_context_p->ue_context.initial_pdus->qos[j].qfi = m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qfi;
      ue_context_p->ue_context.pduSession[i].param.qos[j].qfi = m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qfi;
      ue_context_p->ue_context.initial_pdus->qos[j].fiveQI =
          m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.non_dynamic.fiveqi;
      ue_context_p->ue_context.pduSession[i].param.qos[j].fiveQI =
          m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.non_dynamic.fiveqi;
      ue_context_p->ue_context.initial_pdus->qos[j].qos_priority =
          m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.dynamic.qos_priority_level;
      ue_context_p->ue_context.pduSession[i].param.qos[j].qos_priority =
          m->ue_context.pdusession_tobe_setup_list.pdu[i].qos_list.qos[j].qos_params.dynamic.qos_priority_level;
    }
  }
  m->ue_context.rrc_buffer_size = 8192;

  ue_context_p->ue_context.as_security_active = true;
  ue_context_p->ue_context.f1_ue_context_active = false;

  
  ue_context_p->ue_context.integrity_algorithm = NR_IntegrityProtAlgorithm_nia2; /* B2A case */

  gNB_RRC_UE_t *ue = &ue_context_p->ue_context;
  ue->ho_context_xn = malloc(sizeof(*ue->ho_context_xn));
  ue->ho_context_xn->source_xn = malloc(sizeof(*ue->ho_context_xn->source_xn));
  ue->ho_context_xn->target_xn = malloc(sizeof(*ue->ho_context_xn->target_xn));
  ue->ho_context_xn = ho_ctxt;
  ue->ho_context_xn->source_xn->s_ue_xnap_id = m->s_ng_node_ue_xnap_id;
  ue->ho_context_xn->target_xn->t_ue_xnap_id = m->t_ng_node_ue_xnap_id;
  ue->masterCellGroup = malloc(sizeof(*ue->masterCellGroup));
  ue->masterCellGroup->cellGroupId = m->uehistory_info.last_visited_cgi.cgi;
  
  xnap_gNB_instance_t *instance_p;
  instance_p = xnap_gNB_get_instance(instance);

  xnap_set_ids(&instance_p->id_manager,m->ue_id,
               ue_context_p->ue_context.rrc_ue_id,
               m->s_ng_node_ue_xnap_id,
               m->t_ng_node_ue_xnap_id);

  
  // Trigger bearer context setup
  trigger_bearer_setup(rrc, ue, ue->n_initial_pdu, ue->initial_pdus, 0);
  if(ue_data.e1_assoc_id == 0)
     ue_data.e1_assoc_id = ue->e1_assoc_id_xn;
  LOG_E(NR_RRC,"ue_data.e1_assoc_id in process handover %d\n", ue_data.e1_assoc_id);
  cu_add_f1_ue_data(ue->rrc_ue_id, &ue_data);
}

void rrc_gNB_process_handover_request_ack(sctp_assoc_t assoc_id, xnap_handover_req_ack_t *m)
{
  printf("1st line in rrc_gNB_process_handover_request_ack \n");
  uint8_t rrc_reconfig_buffer[2048];
  int rrc_reconfig_buffer_len;
  rrc_reconfig_buffer_len = m->rrc_buffer_size;
  memcpy (rrc_reconfig_buffer, m->rrc_buffer, m->rrc_buffer_size); // previous 5 lines added for XnHO
	
  NR_DL_DCCH_Message_t *reconf = NULL;
  /* m->rrc_buffer_size = 8192; */ // XnHO
  asn_dec_rval_t dec_rval =
      uper_decode_complete(NULL, &asn_DEF_NR_DL_DCCH_Message, (void *)&reconf, (uint8_t *)m->rrc_buffer, (int)m->rrc_buffer_size);
  if ((dec_rval.code != RC_OK) && (dec_rval.consumed == 0)) {
    AssertFatal(1 == 0, "NR_UL_DCCH_MESSAGE decode error\n");
    // free the memory
    SEQUENCE_free(&asn_DEF_NR_RRCReconfiguration, reconf, 1);
    return;
  }
  NR_RRCReconfiguration_t *t_id = reconf->message.choice.c1->choice.rrcReconfiguration;
  if (t_id == NULL) {
    LOG_D(NR_RRC, "reconf TRA IE is null");
  } else {
    LOG_D(NR_RRC, "reconf TRA IE is NOT null %ld\n", t_id->rrc_TransactionIdentifier);
  }
  OCTET_STRING_t *reconf_ie = reconf->message.choice.c1->choice.rrcReconfiguration->criticalExtensions.choice.rrcReconfiguration
                                  ->nonCriticalExtension->masterCellGroup;
  if (reconf_ie == NULL) {
    LOG_D(NR_RRC, "reconf DCCH IE is null");
  } else {
    LOG_D(NR_RRC, "reconf DCCH IE NOT null");
  }

  NR_CellGroupConfig_t *cgc = NULL;
  asn_dec_rval_t dec_rval1 =
      uper_decode_complete(NULL, &asn_DEF_NR_CellGroupConfig, (void *)&cgc, (uint8_t *)reconf_ie->buf, (int)reconf_ie->size);
  NR_ServingCellConfigCommon_t *cgc_ie = cgc->spCellConfig->reconfigurationWithSync->spCellConfigCommon;
  if (cgc_ie == NULL) {
    LOG_D(NR_RRC, "CGC IE is null");
  } else {
    LOG_D(NR_RRC, "CGC IE NOT null %ld\n", *cgc_ie->physCellId);
  }
  if ((dec_rval1.code != RC_OK) && (dec_rval1.consumed == 0)) {
    AssertFatal(1 == 0, "NR_UL_DCCH_MESSAGE decode error\n");
    SEQUENCE_free(&asn_DEF_NR_RRCReconfiguration, reconf, 1);
    return;
  }
  instance_t instance = 0;
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  int rrc_ue_id = xnap_id_get_cu_ueid(&instance_p->id_manager, m->s_ng_node_ue_xnap_id); 
  xnap_id_set_target(&instance_p->id_manager, m->s_ng_node_ue_xnap_id, m->t_ng_node_ue_xnap_id);
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, rrc_ue_id);
  if (ue_context_p == NULL) {
    LOG_I(NR_RRC, "UE Context is NULL");
  }
  ue_context_p->ue_context.masterCellGroup = cgc;
  ue_context_p->ue_context.nb_of_pdusessions = m->ue_context.pdusession_admitted_list.num_pdu;
  for (int i = 0; i < ue_context_p->ue_context.nb_of_pdusessions; i++) {
    ue_context_p->ue_context.pduSession[i].param.pdusession_id = m->ue_context.pdusession_admitted_list.pdu[i].pdusession_id;
    ue_context_p->ue_context.pduSession[i].param.nb_qos = m->ue_context.pdusession_admitted_list.pdu[i].qos_list.num_qos;
    for (int j = 0; j < ue_context_p->ue_context.pduSession[i].param.nb_qos; j++) {
      ue_context_p->ue_context.pduSession[i].param.qos[j].qfi = m->ue_context.pdusession_admitted_list.pdu[i].qos_list.qos[j].qfi;
    }
  }
    
  ue_context_p->ue_context.ho_context_xn = malloc(sizeof(*ue_context_p->ue_context.ho_context_xn));
  ue_context_p->ue_context.ho_context_xn->source_xn = malloc(sizeof(*ue_context_p->ue_context.ho_context_xn->source_xn));
  ue_context_p->ue_context.ho_context_xn->target_xn = malloc(sizeof(*ue_context_p->ue_context.ho_context_xn->target_xn));
  ue_context_p->ue_context.ho_context_xn->source_xn->s_ue_xnap_id = m->s_ng_node_ue_xnap_id;
  ue_context_p->ue_context.ho_context_xn->target_xn->t_ue_xnap_id = m->t_ng_node_ue_xnap_id;
  
  /* Optional - Trigger UE Context Modification request - add if required */
  f1ap_drb_to_be_setup_t drbs[32];
  int nb_drb = fill_drb_to_be_setup_from_e1_resp(rrc, &ue_context_p->ue_context, m->ue_context.pdusession_admitted_list.pdu, m->ue_context.pdusession_admitted_list.num_pdu, drbs);
  // int nb_drb = fill_drb_to_be_setup_from_e1_resp(rrc, UE, m->ue_context.pdusession_admitted_list.pdu, m->ue_context.pdusession_admitted_list.num_pdu, drbs);
  rrc_gNB_generate_UeContextModificationRequest(rrc, ue_context_p, nb_drb, drbs, 0, NULL); 
  /* Trigger UE Context Modification request - add if required */

  // Triggering reconfiguration
  /* nr_rrc_reconfiguration_req(rrc, &ue_context_p->ue_context, 0, 0); Testing...*/

  printf("triggering RRC reconf and enter cuup_request_pdcp_sn_status \n");

  // To make code symmetric and SRB1 and SRB2 for dedicated RRC Reconf
  if (ue_context_p->ue_context.xn_ho_in > 0 || ue_context_p->ue_context.xn_ho_out > 1 ) {
     rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, &ue_context_p->ue_context);
  } else {
     rrc_gNB_trigger_reconfiguration_for_xn_handover(rrc, &ue_context_p->ue_context, rrc_reconfig_buffer, rrc_reconfig_buffer_len);
     ue_context_p->ue_context.xn_ho_out++;
  }
	
  // tmp_ue_p = &ue_context_p->ue_context;

  /* {
  static int first_time = 1;
  if (first_time) {
        rrc_gNB_trigger_reconfiguration_for_xn_handover(rrc, &ue_context_p->ue_context, rrc_reconfig_buffer, rrc_reconfig_buffer_len); //Apr28
        first_time = 0;
   }
   else{
       rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, &ue_context_p->ue_context);  
       }
  } */ 

  /*
  
  if (ue_context_p->ue_context.masterCellGroup->cellGroupId != ue_context_p->ue_context.nr_cellid){
    printf("calling rrc_gNB_generate_dedicatedRRCReconfiguration for HO \n");
    tmp_ue_p = &ue_context_p->ue_context;
    rrc_gNB_generate_dedicatedRRCReconfiguration(rrc, &ue_context_p->ue_context);  
  } else {
    printf("calling rrc_gNB_trigger_reconfiguration_for_xn_handover for 1st HO \n");
    rrc_gNB_trigger_reconfiguration_for_xn_handover(rrc, &ue_context_p->ue_context, rrc_reconfig_buffer, rrc_reconfig_buffer_len); 
  }
   
   */

  /* rrc_gNB_modify_dedicatedRRCReconfiguration(rrc, &ue_context_p->ue_context); *//* to be removed 22Apr2025 Testing */

  cuup_request_pdcp_sn_status(rrc, &ue_context_p->ue_context); /* Xn code for E1AP SN status transfer procedures */
}


/* Xn code for SN Status transfer procedures ** Processing SN STATUS TRANSFER MESSAGE */

void rrc_gNB_process_xn_sn_status_transfer (sctp_assoc_t assoc_id, xnap_xn_sn_status_transfer_t *sn)
{
  printf("1st line in rrc_gNB_process_xn_sn_status_transfer \n");
  instance_t instance = 0;
  xnap_gNB_instance_t *instance_p = xnap_gNB_get_instance(instance);
  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  int rrc_ue_id = xnap_id_get_cu_ueid(&instance_p->id_manager, sn->t_ng_node_ue_xnap_id);
  printf(" sn->t_ng_node_ue_xnap_id is %d and rrc_ue_id is %d \n", sn->t_ng_node_ue_xnap_id, rrc_ue_id);
  rrc_gNB_ue_context_t   *ue_context_p = rrc_gNB_get_ue_context(rrc, rrc_ue_id); /* rrc_ue_id instead of 1 */

  /* 23 Feb 2025 */
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
  drbs_subject_to_transfer_list_t *snstatus = &sn->drb_list;	
  
  UE->ho_context_xn->source_xn->s_ue_xnap_id = sn->s_ng_node_ue_xnap_id;
  UE->ho_context_xn->target_xn->t_ue_xnap_id = sn->t_ng_node_ue_xnap_id;
  								       
  if (ue_context_p == NULL)
  {
      LOG_I(NR_RRC,"UE Context is NULL");
  }

  for(int i=0; i<sn->drb_list.num_drb; i++)  // sn should be replaced with rrc structure num_drb 
  {
   ue_context_p->ue_context.established_drbs[i].drb_id = sn->drb_list.drb[i].drb_id;
  }

  printf("triggering cuup_info_pdcp_sn_status after processing sn status transfer at target\n");
  cuup_info_pdcp_sn_status(rrc, UE, snstatus); /* Xn code for E1AP SN status transfer procedures */

}

void rrc_gNB_process_ue_context_release(instance_t instance, xnap_ue_context_release_t *ue)
{
  LOG_I(NR_RRC, "Received UE CONTEXT RELEASE from TASK XNAP at source\n");
  struct rrc_gNB_ue_context_s *ue_context_p = NULL;
  ue_context_p = rrc_gNB_get_ue_context(RC.nrrrc[instance], ue->rnti);
  LOG_I(NR_RRC, "[gNB] source gNB receives the XN UE CONTEXT RELEASE \n");
  DevAssert(ue_context_p != NULL);
  gNB_RRC_UE_t *UE = &ue_context_p->ue_context;

  gNB_RRC_INST *rrc = RC.nrrrc[instance];
  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  f1ap_ue_context_release_cmd_t ue_context_release_cmd = {
      .gNB_CU_ue_id = UE->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .cause = F1AP_CAUSE_RADIO_NETWORK,
      .cause_value = 10, // F1AP_CauseRadioNetwork_normal_release
      .srb_id = DCCH,
  };
  rrc->mac_rrc.ue_context_release_command(ue_data.du_assoc_id, &ue_context_release_cmd);
  /* a UE might not be associated to a CU-UP if it never requested a PDU
   * session (intentionally, or because of erros) */
  if (ue_associated_to_cuup(rrc, UE)) {
    sctp_assoc_t assoc_id = get_existing_cuup_for_ue(rrc, UE);
    e1ap_bearer_release_cmd_t cmd = {
        .gNB_cu_cp_ue_id = UE->rrc_ue_id,
        .gNB_cu_up_ue_id = UE->rrc_ue_id,
    };
    rrc->cucp_cuup.bearer_context_release(assoc_id, &cmd);
  }
  /* special case: the DU might be offline, in which case the f1_ue_data exists
   * but is set to 0 */
  if (cu_exists_f1_ue_data(UE->rrc_ue_id) && cu_get_f1_ue_data(UE->rrc_ue_id).du_assoc_id != 0) {
    // rrc_gNB_generate_RRCRelease(rrc, UE); // has f1ap_ue_context_release_command trigger
    if (cu_remove_f1_ue_data(UE->rrc_ue_id))
      LOG_I(NR_RRC, "Data removed from cu_get_f1_ue_data!!!\n");
    /* UE will be freed after UE context release */
  } else {
    // the DU is offline already
    rrc_remove_ue(rrc, ue_context_p);
  }
  nr_pdcp_remove_UE(UE->rrc_ue_id);
  rrc_delete_ue_data(UE);
  rrc_gNB_remove_ue_context(rrc, ue_context_p);
  return;
}

void *rrc_gnb_task(void *args_p) {
  MessageDef *msg_p;
  instance_t                         instance;
  int                                result;

  long stats_timer_id = 1;
  if (!IS_SOFTMODEM_NOSTATS) {
    /* timer to write stats to file */
    timer_setup(1, 0, TASK_RRC_GNB, 0, TIMER_PERIODIC, NULL, &stats_timer_id);
  }

#ifdef UNI_RAN
  /* Timer to check UE inactivity every 1s */
  long ue_rrc_inactivity_timer_id = 2;
  gNB_RRC_INST *rrc = RC.nrrrc[0];

  if(rrc->inactivity_timer_threshold > 0){
    timer_setup(rrc->inactivity_interval_sec, rrc->inactivity_interval_us, TASK_RRC_GNB, 0, TIMER_PERIODIC, NULL, &ue_rrc_inactivity_timer_id);
  }
  else {
    LOG_I(NR_RRC, "Inactivity Timer Disabled\n");
  }
#endif
  itti_mark_task_ready(TASK_RRC_GNB);
  LOG_I(NR_RRC,"Entering main loop of NR_RRC message task\n");

  while (1) {
    // Wait for a message
    itti_receive_msg(TASK_RRC_GNB, &msg_p);
    const char *msg_name_p = ITTI_MSG_NAME(msg_p);
    instance = ITTI_MSG_DESTINATION_INSTANCE(msg_p);
    LOG_D(NR_RRC,
          "RRC GNB Task Received %s for instance %ld from task %s\n",
          ITTI_MSG_NAME(msg_p),
          ITTI_MSG_DESTINATION_INSTANCE(msg_p),
          ITTI_MSG_ORIGIN_NAME(msg_p));
    switch (ITTI_MSG_ID(msg_p)) {
      case TERMINATE_MESSAGE:
        LOG_W(NR_RRC, " *** Exiting NR_RRC thread\n");
        timer_remove(stats_timer_id);
        itti_exit_task();
        break;

      case MESSAGE_TEST:
        LOG_I(NR_RRC, "[gNB %ld] Received %s\n", instance, msg_name_p);
        break;

      case TIMER_HAS_EXPIRED:
        if (TIMER_HAS_EXPIRED(msg_p).timer_id == stats_timer_id)
          write_rrc_stats(RC.nrrrc[0]);
#ifdef UNI_RAN
        else if (TIMER_HAS_EXPIRED(msg_p).timer_id == ue_rrc_inactivity_timer_id) {
          LOG_D(RRC, "Checking Inactivity with a periodicity of 1 sec\n");
          rrc_gNB_check_inactivity(RC.nrrrc[0]);
        }
#endif
        else
          itti_send_msg_to_task(TASK_RRC_GNB, 0, TIMER_HAS_EXPIRED(msg_p).arg); /* see rrc_gNB_process_NGAP_PDUSESSION_SETUP_REQ() */
        break;

      case F1AP_INITIAL_UL_RRC_MESSAGE:
        AssertFatal(NODE_IS_CU(RC.nrrrc[instance]->node_type) || NODE_IS_MONOLITHIC(RC.nrrrc[instance]->node_type),
                    "should not receive F1AP_INITIAL_UL_RRC_MESSAGE, need call by CU!\n");
        rrc_gNB_process_initial_ul_rrc_message(msg_p->ittiMsgHeader.originInstance, &F1AP_INITIAL_UL_RRC_MESSAGE(msg_p));
        free_initial_ul_rrc_message_transfer(&F1AP_INITIAL_UL_RRC_MESSAGE(msg_p));
        break;

      /* Messages from PDCP */
      /* From DU -> CU */
      case F1AP_UL_RRC_MESSAGE:
        rrc_gNB_decode_dcch(RC.nrrrc[instance], &F1AP_UL_RRC_MESSAGE(msg_p));
        free_ul_rrc_message_transfer(&F1AP_UL_RRC_MESSAGE(msg_p));
        break;

      case NGAP_DOWNLINK_NAS:
        rrc_gNB_process_NGAP_DOWNLINK_NAS(msg_p, instance, &rrc_gNB_mui);
        break;

      case NGAP_PDUSESSION_SETUP_REQ:
        rrc_gNB_process_NGAP_PDUSESSION_SETUP_REQ(msg_p, instance);
        break;

      case NGAP_PATH_SWITCH_REQ_ACK:
        rrc_gNB_process_NGAP_PATH_SWITCH_REQ_ACK(msg_p, instance);
        break;

      case NGAP_PDUSESSION_MODIFY_REQ:
        rrc_gNB_process_NGAP_PDUSESSION_MODIFY_REQ(msg_p, instance);
        break;

      case NGAP_PDUSESSION_RELEASE_COMMAND:
        rrc_gNB_process_NGAP_PDUSESSION_RELEASE_COMMAND(msg_p, instance);
        break;
#ifdef UNI_RAN
      case NGAP_DL_RAN_STATUS_TRANSFER:
        rrc_gNB_process_NGAP_DL_RAN_STATUS_TRANSFER(msg_p, instance);
        break;
#endif
      /* Messages from F1AP task */
      case F1AP_SETUP_REQ:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type), "should not receive F1AP_SETUP_REQUEST in DU!\n");
        rrc_gNB_process_f1_setup_req(&F1AP_SETUP_REQ(msg_p), msg_p->ittiMsgHeader.originInstance);
        free_f1ap_setup_request(&F1AP_SETUP_REQ(msg_p));
        break;

      case F1AP_UE_CONTEXT_SETUP_RESP:
        rrc_CU_process_ue_context_setup_response(msg_p, instance);
        break;

      case F1AP_UE_CONTEXT_SETUP_FAILURE:
        rrc_CU_process_ue_context_setup_failure(msg_p, instance);
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_RESP:
	/* XnHO Code */ // A2B cgc issue 
        f1ap_ue_context_modif_resp_t *resp = &F1AP_UE_CONTEXT_MODIFICATION_RESP(msg_p);
        gNB_RRC_INST *rrc = RC.nrrrc[instance];
        rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context(rrc, resp->gNB_CU_ue_id);
        if (!ue_context_p) {
          LOG_E(RRC, "could not find UE context for CU UE ID %u, aborting transaction\n", resp->gNB_CU_ue_id);
        return;
        }
        gNB_RRC_UE_t *UE = &ue_context_p->ue_context;
        if (UE->ho_context_xn != NULL)
        break;
        /* XnHO code */

        rrc_CU_process_ue_context_modification_response(msg_p, instance);  /* After A2B, DU CGC is modified and reported to CU causing further HO to fail */
        break;

      case F1AP_UE_CONTEXT_MODIFICATION_REQUIRED:
        rrc_CU_process_ue_modification_required(msg_p, instance, msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_UE_CONTEXT_RELEASE_REQ:
        rrc_CU_process_ue_context_release_request(msg_p, msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_UE_CONTEXT_RELEASE_COMPLETE:
        rrc_CU_process_ue_context_release_complete(msg_p);
        break;

      case F1AP_LOST_CONNECTION:
        rrc_CU_process_f1_lost_connection(RC.nrrrc[0], &F1AP_LOST_CONNECTION(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case F1AP_GNB_DU_CONFIGURATION_UPDATE:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type), "should not receive F1AP_SETUP_REQUEST in DU!\n");
        rrc_gNB_process_f1_du_configuration_update(&F1AP_GNB_DU_CONFIGURATION_UPDATE(msg_p), msg_p->ittiMsgHeader.originInstance);
        free_f1ap_du_configuration_update(&F1AP_GNB_DU_CONFIGURATION_UPDATE(msg_p));
        break;

      case F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE:
        AssertFatal(!NODE_IS_DU(RC.nrrrc[instance]->node_type),
                    "should not receive F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE in DU!\n");
        LOG_E(NR_RRC, "Handling of F1AP_GNB_CU_CONFIGURATION_UPDATE_ACKNOWLEDGE not implemented\n");
        break;

      case F1AP_RESET_ACK:
        LOG_I(NR_RRC, "received F1AP reset acknowledgement\n");
        free_f1ap_reset_ack(&F1AP_RESET_ACK(msg_p));
        /*Messages from XNAP*/

      case XNAP_SETUP_RESP:
        rrc_gNB_process_xn_setup_response(ITTI_MSG_ORIGIN_INSTANCE(msg_p), &XNAP_SETUP_RESP(msg_p));
        break;

      case XNAP_SETUP_REQ:
        rrc_gNB_process_xn_setup_request(ITTI_MSG_ORIGIN_INSTANCE(msg_p), &XNAP_SETUP_REQ(msg_p), instance);
        break;

      case XNAP_HANDOVER_REQ:
        rrc_gNB_process_handover_request(ITTI_MSG_ORIGIN_INSTANCE(msg_p), &XNAP_HANDOVER_REQ(msg_p));
        break;

      case XNAP_HANDOVER_REQ_ACK:
        rrc_gNB_process_handover_request_ack(ITTI_MSG_ORIGIN_INSTANCE(msg_p), &XNAP_HANDOVER_REQ_ACK(msg_p));
        break;

      case XNAP_UE_CONTEXT_RELEASE:
        rrc_gNB_process_ue_context_release(instance, &XNAP_UE_CONTEXT_RELEASE(msg_p));
        break;

      /* Xn code for SN status transfer */
      case XNAP_SN_STATUS_TRANSFER: 
        LOG_I(NR_RRC,"[rrc_gNB] Received SN Status Transfer \n");
        rrc_gNB_process_xn_sn_status_transfer(ITTI_MSG_ORIGIN_INSTANCE(msg_p),&XNAP_SN_STATUS_TRANSFER(msg_p));
        break;

      /* Messages from X2AP */
      case X2AP_ENDC_SGNB_ADDITION_REQ:
        LOG_I(NR_RRC, "Received ENDC sgNB addition request from X2AP \n");
        rrc_gNB_process_AdditionRequestInformation(instance, &X2AP_ENDC_SGNB_ADDITION_REQ(msg_p));
        break;

      case X2AP_ENDC_SGNB_RECONF_COMPLETE:
        LOG_A(NR_RRC, "Handling of reconfiguration complete message at RRC gNB is pending \n");
        break;

      case NGAP_INITIAL_CONTEXT_SETUP_REQ:
        rrc_gNB_process_NGAP_INITIAL_CONTEXT_SETUP_REQ(msg_p, instance);
        break;

      case X2AP_ENDC_SGNB_RELEASE_REQUEST:
        LOG_I(NR_RRC, "Received ENDC sgNB release request from X2AP \n");
        rrc_gNB_process_release_request(instance, &X2AP_ENDC_SGNB_RELEASE_REQUEST(msg_p));
        break;

      case X2AP_ENDC_DC_OVERALL_TIMEOUT:
        rrc_gNB_process_dc_overall_timeout(instance, &X2AP_ENDC_DC_OVERALL_TIMEOUT(msg_p));
        break;

      case NGAP_UE_CONTEXT_RELEASE_REQ:
        rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_REQ(msg_p, instance);
        break;

      case NGAP_UE_CONTEXT_RELEASE_COMMAND:
        rrc_gNB_process_NGAP_UE_CONTEXT_RELEASE_COMMAND(msg_p, instance);
        break;

      case E1AP_SETUP_REQ:
        rrc_gNB_process_e1_setup_req(msg_p->ittiMsgHeader.originInstance, &E1AP_SETUP_REQ(msg_p));
        free_e1ap_cuup_setup_request(&E1AP_SETUP_REQ(msg_p));
        break;

      case E1AP_BEARER_CONTEXT_SETUP_RESP:
        rrc_gNB_process_e1_bearer_context_setup_resp(&E1AP_BEARER_CONTEXT_SETUP_RESP(msg_p), instance);
        free_e1ap_context_setup_response(&E1AP_BEARER_CONTEXT_SETUP_RESP(msg_p));
        break;

      case E1AP_BEARER_CONTEXT_MODIFICATION_RESP:
        rrc_gNB_process_e1_bearer_context_modif_resp(&E1AP_BEARER_CONTEXT_MODIFICATION_RESP(msg_p));
        free_e1ap_context_mod_response(&E1AP_BEARER_CONTEXT_MODIFICATION_RESP(msg_p));
        break;

      case E1AP_BEARER_CONTEXT_RELEASE_CPLT:
        rrc_gNB_process_e1_bearer_context_release_cplt(&E1AP_BEARER_CONTEXT_RELEASE_CPLT(msg_p));
        break;

      case E1AP_LOST_CONNECTION: /* CUCP */
        rrc_gNB_process_e1_lost_connection(RC.nrrrc[0], &E1AP_LOST_CONNECTION(msg_p), msg_p->ittiMsgHeader.originInstance);
        break;

      case NGAP_PAGING_IND:
        rrc_gNB_process_PAGING_IND(msg_p, instance);
        break;

      case NGAP_HANDOVER_COMMAND:
        rrc_gNB_process_HandoverCommand(msg_p, instance);
        break;

      case NGAP_HANDOVER_REQUEST:
        rrc_gNB_process_Handover_Request(msg_p, instance);
        break;

      default:
        LOG_E(NR_RRC, "[gNB %ld] Received unexpected message %s\n", instance, msg_name_p);
        break;
    }

    result = itti_free(ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
    AssertFatal(result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    msg_p = NULL;
  }
}

//-----------------------------------------------------------------------------
void rrc_gNB_generate_SecurityModeCommand(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue_p)
//-----------------------------------------------------------------------------
{
  uint8_t buffer[100];
  AssertFatal(!ue_p->as_security_active, "logic error: security already active\n");

  T(T_ENB_RRC_SECURITY_MODE_COMMAND, T_INT(0), T_INT(0), T_INT(0), T_INT(ue_p->rrc_ue_id));
  NR_IntegrityProtAlgorithm_t integrity_algorithm = (NR_IntegrityProtAlgorithm_t)ue_p->integrity_algorithm;
  int size = do_NR_SecurityModeCommand(buffer,
                                       rrc_gNB_get_next_transaction_identifier(rrc->module_id),
                                       ue_p->ciphering_algorithm,
                                       integrity_algorithm);
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, size, "[MSG] RRC Security Mode Command\n");
  LOG_I(NR_RRC, "UE %u Logical Channel DL-DCCH, Generate SecurityModeCommand (bytes %d)\n", ue_p->rrc_ue_id, size);

  nr_rrc_transfer_protected_rrc_message(rrc, ue_p, DL_SCH_LCID_DCCH, buffer, size);
}

//-----------------------------------------------------------------------------
/*
* Generate the RRC Connection Release to UE.
* If received, UE should switch to RRC_IDLE mode.
*/
void rrc_gNB_generate_RRCRelease(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE)
{
  uint8_t buffer[NR_RRC_BUF_SIZE] = {0};
  int size = do_NR_RRCRelease(buffer, NR_RRC_BUF_SIZE, rrc_gNB_get_next_transaction_identifier(rrc->module_id));

  LOG_UE_DL_EVENT(UE, "Send RRC Release\n");
  f1_ue_data_t ue_data = cu_get_f1_ue_data(UE->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_release_cmd_t ue_context_release_cmd = {
    .gNB_CU_ue_id = UE->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .cause = F1AP_CAUSE_RADIO_NETWORK,
    .cause_value = 10, // 10 = F1AP_CauseRadioNetwork_normal_release
    .srb_id = DL_SCH_LCID_DCCH,
  };
  deliver_ue_ctxt_release_data_t data = {.rrc = rrc, .release_cmd = &ue_context_release_cmd, .assoc_id = ue_data.du_assoc_id};
  nr_pdcp_data_req_srb(UE->rrc_ue_id, DL_SCH_LCID_DCCH, rrc_gNB_mui++, size, buffer, rrc_deliver_ue_ctxt_release_cmd, &data);
}

extern instance_t get_f1_gtp_instance(void);

void rrc_gNB_trigger_release_bearer(int rnti)
{
  /* get RRC and UE */
  gNB_RRC_INST *rrc = RC.nrrrc[0];
  rrc_gNB_ue_context_t *ue_context_p = rrc_gNB_get_ue_context_by_rnti_any_du(rrc, rnti);
  if (ue_context_p == NULL) {
    LOG_E(RRC, "unknown UE RNTI %04x\n", rnti);
    return;
  }
  gNB_RRC_UE_t *ue = &ue_context_p->ue_context;

  if (ue->established_drbs[1].status == DRB_INACTIVE) {
    LOG_E(RRC, "no second bearer, aborting\n");
    return;
  }

  // don't use E1: bearer release is not implemented, call directly
  // into PDCP/SDAP and then send corresponding message via F1

  int drb_id = 2;
  ue->established_drbs[1].status = DRB_INACTIVE;
  ue->DRB_ReleaseList = calloc(1, sizeof(*ue->DRB_ReleaseList));
  AssertFatal(ue->DRB_ReleaseList != NULL, "out of memory\n");
  NR_DRB_Identity_t *asn1_drb = malloc(sizeof(*asn1_drb));
  AssertFatal(asn1_drb != NULL, "out of memory\n");
  int idx = 0;
  NR_DRB_ToAddModList_t *drb_list = createDRBlist(ue, false);
  while (idx < drb_list->list.count) {
    const NR_DRB_ToAddMod_t *drbc = drb_list->list.array[idx];
    if (drbc->drb_Identity == drb_id)
      break;
    ++idx;
  }
  if (idx < drb_list->list.count) {
    nr_pdcp_release_drb(rnti, drb_id);
    asn_sequence_del(&drb_list->list, idx, 1);
  }
  *asn1_drb = drb_id;
  asn1cSeqAdd(&ue->DRB_ReleaseList->list, asn1_drb);

  instance_t f1inst = get_f1_gtp_instance();
  if (f1inst >= 0)
    newGtpuDeleteOneTunnel(f1inst, ue->rrc_ue_id, drb_id);
  nr_pdcp_release_drb(ue->rrc_ue_id, drb_id);

  f1ap_drb_to_be_released_t drbs_to_be_released[1] = {{.rb_id = drb_id}};
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_modif_req_t ue_context_modif_req = {
    .gNB_CU_ue_id = ue->rrc_ue_id,
    .gNB_DU_ue_id = ue_data.secondary_ue,
    .plmn.mcc = rrc->configuration.mcc[0],
    .plmn.mnc = rrc->configuration.mnc[0],
    .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
    .nr_cellid = rrc->nr_cellid,
    .servCellId = 0, /* TODO: correct value? */
    .srbs_to_be_setup_length = 0,
    .srbs_to_be_setup = NULL,
    .drbs_to_be_setup_length = 0,
    .drbs_to_be_setup = NULL,
    .drbs_to_be_released_length = 1,
    .drbs_to_be_released = drbs_to_be_released,
  };
  rrc->mac_rrc.ue_context_modification_request(ue_data.du_assoc_id, &ue_context_modif_req);
}

#ifdef UNI_RAN
int rrc_gNB_generate_pcch_msg(sctp_assoc_t assoc_id, uint64_t tmsi, uint8_t paging_drx, instance_t instance, uint8_t CC_id)
{
  const unsigned int Ttab[4] = {32,64,128,256};
  uint8_t Tc;
  uint32_t pfoffset;
  uint32_t N;  /* N: min(T,nB). total count of PF in one DRX cycle */
  uint32_t Ns = 0;  /* Ns: max(1,nB/T) */
  uint32_t T;  /* DRX cycle */
  uint8_t buffer[NR_RRC_BUF_SIZE];
  uint32_t n_and_paging_frame_offset = 1;
  uint32_t pcch_config_ns = 2;

  /* get default DRX cycle from configuration */
  Tc = NR_PagingCycle_rf128;
  T = Ttab[Tc];

  switch (n_and_paging_frame_offset) {
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneT:
      N = T;
      pfoffset = 0; /* As n_and_paging_frame_offset =  NR_PCCH_Config__nAndPagingFrameOffset_PR_oneT, Hence PF offset = 0 (Only one possible value) */
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_halfT:
      N = T/2;
      pfoffset = 1;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_quarterT:
      N = T/4;
      pfoffset = 3;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneEighthT:
      N = T/8;
      pfoffset = 7;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneSixteenthT:
      N = T/16;
      pfoffset = 15;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg:  pfoffset error (pfoffset %d)\n",
            instance, n_and_paging_frame_offset);
      return (-1);
  }

  switch (pcch_config_ns) {
    case NR_PCCH_Config__ns_four:
      Ns = 4;
      break;
    case NR_PCCH_Config__ns_two:
      Ns = 2;
      break;
    case NR_PCCH_Config__ns_one:
      Ns = 1;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg: ns error (ns %u)\n",
            instance, pcch_config_ns);
      return (-1);
  }


  /* Insert data to NR_UE_PF_PO which will be used in MAC for scheduling the Paging message */
  gNB_MAC_INST *gNB_mac = RC.nrmac[0];
  pthread_mutex_lock(&nr_ue_pf_po_mutex);

  int free_index = -1;
  bool found_existing = false;

  for (uint8_t i = 0; i < MAX_MOBILES_PER_GNB; i++) {

  /* Case 1: Existing UE → update and STOP */
  if (NR_UE_PF_PO[CC_id][i].enable_flag == true &&
      NR_UE_PF_PO[CC_id][i].ue_index_value == (tmsi % 1024)) {

      NR_UE_PF_PO[CC_id][i].T = T;
      NR_UE_PF_PO[CC_id][i].PF_min =
          (((T / N) * (NR_UE_PF_PO[CC_id][i].ue_index_value % N)) - pfoffset) % T;

      uint8_t i_s =
          ((NR_UE_PF_PO[CC_id][i].ue_index_value / N) % Ns);

      NR_UE_PF_PO[CC_id][i].PO = (i_s * (20 / Ns));

      gNB_mac->freeUeIndexPaging = i;

      LOG_D(RRC, "[gNB %ld] Update existing UE at index %d\n",
            instance, i);

      found_existing = true;
      break;   // ← VERY IMPORTANT
  }

  /* Track first free slot ONLY if no existing UE found yet */
  if (!found_existing &&
      free_index < 0 &&
      NR_UE_PF_PO[CC_id][i].enable_flag == false) {
      free_index = i;
  }
  }

  /* Case 2: Insert new UE ONLY if no existing UE was found */
  if (!found_existing) {

    if (free_index >= 0) {

      uint8_t i = free_index;

      NR_UE_PF_PO[CC_id][i].enable_flag = true;
      NR_UE_PF_PO[CC_id][i].ue_index_value = tmsi % 1024;
      NR_UE_PF_PO[CC_id][i].T = T;

      NR_UE_PF_PO[CC_id][i].PF_min =
        (((T / N) * (NR_UE_PF_PO[CC_id][i].ue_index_value % N)) - pfoffset) % T;

      uint8_t i_s =
        ((NR_UE_PF_PO[CC_id][i].ue_index_value / N) % Ns);

      NR_UE_PF_PO[CC_id][i].PO = (i_s * (20 / Ns));

      gNB_mac->freeUeIndexPaging = i;

      LOG_D(RRC, "[gNB %ld] Insert new UE at index %d\n",
          instance, i);

    } else {
    LOG_D(RRC, "[gNB %ld] No free UE paging slots available!\n",
          instance);
    }
  }

  pthread_mutex_unlock(&nr_ue_pf_po_mutex);

  /* Create message for MAC (DLInformationTransfer_t) */
  int length = do_NR_Paging(instance, buffer, tmsi);

  if (length <= 0) {
      LOG_E(NR_RRC, "do_NR_Paging error\n");
      return -1;
  }

  /* send paging message to MAC for PCCH transmission */
  gNB_RRC_INST *rrc = RC.nrrrc[instance];

  if (!rrc) {
    LOG_E(NR_RRC, "Invalid RRC instance %ld\n", instance);
    return -1;
  }
  LOG_DUMPMSG(NR_RRC, DEBUG_RRC, (char *)buffer, length, "[PCCH Paging]\n");

  /* Structure for MAC transfer (paging only) */
  f1ap_dl_rrc_message_t dl_rrc = {
    .rrc_container        = buffer,
    .rrc_container_length = length,
    .srb_id               = RRC_DL_PAGING   // <-- key: identify paging
  };
  /* Trasfer to MAC directly, bypassing PDCP */
  rrc->mac_rrc.dl_rrc_message_transfer(assoc_id, &dl_rrc);
  LOG_D(NR_RRC, "Paging message sent to MAC (size=%d bytes)\n", length);

  return 0;
}
#else
int rrc_gNB_generate_pcch_msg(sctp_assoc_t assoc_id, const NR_SIB1_t *sib1, uint32_t tmsi, uint8_t paging_drx)
{
  instance_t instance = 0;
  const unsigned int Ttab[4] = {32,64,128,256};
  uint8_t Tc;
  uint8_t Tue;
  uint32_t pfoffset;
  uint32_t N;  /* N: min(T,nB). total count of PF in one DRX cycle */
  uint32_t Ns = 0;  /* Ns: max(1,nB/T) */
  uint32_t T;  /* DRX cycle */
  uint8_t buffer[NR_RRC_BUF_SIZE];

  /* get default DRX cycle from configuration */
  Tc = sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.defaultPagingCycle;

  Tue = paging_drx;
  /* set T = min(Tc,Tue) */
  T = Tc < Tue ? Ttab[Tc] : Ttab[Tue];
  /* set N = PCCH-Config->nAndPagingFrameOffset */
  switch (sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.nAndPagingFrameOffset.present) {
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneT:
      N = T;
      pfoffset = 0;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_halfT:
      N = T/2;
      pfoffset = 1;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_quarterT:
      N = T/4;
      pfoffset = 3;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneEighthT:
      N = T/8;
      pfoffset = 7;
      break;
    case NR_PCCH_Config__nAndPagingFrameOffset_PR_oneSixteenthT:
      N = T/16;
      pfoffset = 15;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg:  pfoffset error (pfoffset %d)\n",
            instance, sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.nAndPagingFrameOffset.present);
      return (-1);

  }

  switch (sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.ns) {
    case NR_PCCH_Config__ns_four:
      if(*sib1->servingCellConfigCommon->downlinkConfigCommon.initialDownlinkBWP.pdcch_ConfigCommon->choice.setup->pagingSearchSpace == 0){
        LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg:  ns error only 1 or 2 is allowed when pagingSearchSpace is 0\n",
              instance);
        return (-1);
      } else {
        Ns = 4;
      }
      break;
    case NR_PCCH_Config__ns_two:
      Ns = 2;
      break;
    case NR_PCCH_Config__ns_one:
      Ns = 1;
      break;
    default:
      LOG_E(RRC, "[gNB %ld] In rrc_gNB_generate_pcch_msg: ns error (ns %ld)\n",
            instance, sib1->servingCellConfigCommon->downlinkConfigCommon.pcch_Config.ns);
      return (-1);
  }

  (void) N; /* not used, suppress warning */
  (void) Ns; /* not used, suppress warning */
  (void) pfoffset; /* not used, suppress warning */

  /* Create message for PDCP (DLInformationTransfer_t) */
  int length = do_NR_Paging(instance, buffer, tmsi);

  if (length == -1) {
    LOG_I(NR_RRC, "do_Paging error\n");
    return -1;
  }
  // TODO, send message to pdcp
  (void) assoc_id;

  return 0;
}
#endif

/* F1AP UE Context Management Procedures */

//-----------------------------------------------------------------------------
void rrc_gNB_generate_UeContextSetupRequest(const gNB_RRC_INST *rrc,
                                            rrc_gNB_ue_context_t *const ue_context_pP,
                                            int n_drbs,
                                            const f1ap_drb_to_be_setup_t *drbs)
//-----------------------------------------------------------------------------
{
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  AssertFatal(!ue_p->f1_ue_context_active, "logic error: ue context already active\n");

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  cu_to_du_rrc_information_t cu2du = {0};
  cu_to_du_rrc_information_t *cu2du_p = NULL;

  if (ue_p->ue_cap_buffer.len > 0 && ue_p->ue_cap_buffer.buf != NULL) {
    cu2du_p = &cu2du;
    cu2du.uE_CapabilityRAT_ContainerList = ue_p->ue_cap_buffer.buf;
    cu2du.uE_CapabilityRAT_ContainerList_length = ue_p->ue_cap_buffer.len;
    if ((ue_p->ue_handover_prep_info_buffer.len > 0) && (ue_p->ue_handover_prep_info_buffer.buf != NULL)) {
      cu2du.handoverPreparationInfo = ue_p->ue_handover_prep_info_buffer.buf;
      cu2du.handoverPreparationInfo_length = ue_p->ue_handover_prep_info_buffer.len;
    }
  }

  const nr_rrc_du_container_t *du = get_du_for_ue((gNB_RRC_INST *)rrc, ue_p->rrc_ue_id);
  DevAssert(du != NULL);
  f1ap_served_cell_info_t *cell_info = &du->setup_req->cell[0].info;
  NR_MeasConfig_t *measconfig = NULL;
  if (du->mtc != NULL) {
    int scs = get_ssb_scs(cell_info);
    int band = get_dl_band(cell_info);
    const NR_MeasTimingList_t *mtlist = du->mtc->criticalExtensions.choice.c1->choice.measTimingConf->measTiming;
    const NR_MeasTiming_t *mt = mtlist->list.array[0];
    const neighbour_cell_configuration_t *neighbour_config = get_neighbour_config(cell_info->nr_cellid);
    seq_arr_t *neighbour_cells = NULL;
    if (neighbour_config)
      neighbour_cells = neighbour_config->neighbour_cells;

    measconfig = get_MeasConfig(mt, band, scs, &rrc->measurementConfiguration, neighbour_cells);
  }
  if (measconfig) {
    uint8_t buf[NR_RRC_BUF_SIZE];
    int size = do_NR_MeasConfig(measconfig, buf, NR_RRC_BUF_SIZE);
    cu2du_p = &cu2du;
    cu2du.measConfig = buf;
    cu2du.measConfig_length = size;
  }
  int nb_srb = 2;
  f1ap_srb_to_be_setup_t srbs[2] = {{.srb_id = 1, .lcid = 1}, {.srb_id = 2, .lcid = 2}};
  activate_srb(ue_p, 1);
  activate_srb(ue_p, 2);

  /* the callback will fill the UE context setup request and forward it */
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_setup_t ue_context_setup_req = {
      .gNB_CU_ue_id = ue_p->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid,
      .servCellId = 0, /* TODO: correct value? */
      .srbs_to_be_setup_length = nb_srb,
      .srbs_to_be_setup = srbs,
      .drbs_to_be_setup_length = n_drbs,
      .drbs_to_be_setup = (f1ap_drb_to_be_setup_t *)drbs,
      .cu_to_du_rrc_information = cu2du_p,
  };

  rrc->mac_rrc.ue_context_setup_request(ue_data.du_assoc_id, &ue_context_setup_req);
  LOG_I(RRC, "UE %d trigger UE context setup request with %d DRBs\n", ue_p->rrc_ue_id, n_drbs);
}

//-----------------------------------------------------------------------------
void rrc_gNB_generate_UeContextModificationRequest(const gNB_RRC_INST *rrc,
                                                   rrc_gNB_ue_context_t *const ue_context_pP,
                                                   int n_drbs,
                                                   const f1ap_drb_to_be_setup_t *drbs,
                                                   int n_rel_drbs,
                                                   const f1ap_drb_to_be_released_t *rel_drbs)
//-----------------------------------------------------------------------------
{
  gNB_RRC_UE_t *ue_p = &ue_context_pP->ue_context;
  AssertFatal(ue_p->f1_ue_context_active, "logic error: calling ue context modification when context not established\n");
  AssertFatal(ue_p->Srb[1].Active && ue_p->Srb[2].Active, "SRBs should already be active\n");

  AssertFatal(!NODE_IS_DU(rrc->node_type), "illegal node type DU!\n");

  cu_to_du_rrc_information_t cu2du = {0};
  cu_to_du_rrc_information_t *cu2du_p = NULL;
  if (ue_p->ue_cap_buffer.len > 0 && ue_p->ue_cap_buffer.buf != NULL) {
    cu2du_p = &cu2du;
    cu2du.uE_CapabilityRAT_ContainerList = ue_p->ue_cap_buffer.buf;
    cu2du.uE_CapabilityRAT_ContainerList_length = ue_p->ue_cap_buffer.len;
  }

  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue_p->rrc_ue_id);
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_modif_req_t ue_context_modif_req = {
      .gNB_CU_ue_id = ue_p->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid,
      .servCellId = 0, /* TODO: correct value? */
      .drbs_to_be_setup_length = n_drbs,
      .drbs_to_be_setup = (f1ap_drb_to_be_setup_t *)drbs,
      .drbs_to_be_released_length = n_rel_drbs,
      .drbs_to_be_released = (f1ap_drb_to_be_released_t *)rel_drbs,
      .cu_to_du_rrc_information = cu2du_p,
  };
  rrc->mac_rrc.ue_context_modification_request(ue_data.du_assoc_id, &ue_context_modif_req);
  LOG_I(RRC, "UE %d trigger UE context modification request with %d DRBs\n", ue_p->rrc_ue_id, n_drbs);
}

void rrc_deliver_ue_ctxt_modif_req_xn(void *deliver_pdu_data, ue_id_t ue_id, int srb_id, char *buf, int size, int sdu_id)
{
  DevAssert(deliver_pdu_data != NULL);
  deliver_ue_ctxt_modification_data_t *data = deliver_pdu_data;
  data->modification_req->rrc_container = (uint8_t*)buf;
  data->modification_req->rrc_container_length = size;

  printf ("rrc_deliver_ue_ctxt_modif_req called srb_id %d, sdu_id %d size %d\n",
        srb_id, sdu_id, size);

  data->rrc->mac_rrc.ue_context_modification_request(data->assoc_id, data->modification_req);
}

void rrc_gNB_trigger_reconfiguration_for_xn_handover(gNB_RRC_INST *rrc, gNB_RRC_UE_t *ue, uint8_t *rrc_reconf, int rrc_reconf_len)
{
  f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->rrc_ue_id);

  TransmActionInd_t transmission_action_indicator = TransmActionInd_STOP;
  RETURN_IF_INVALID_ASSOC_ID(ue_data.du_assoc_id);
  f1ap_ue_context_modif_req_t ue_context_modif_req = {
      .gNB_CU_ue_id = ue->rrc_ue_id,
      .gNB_DU_ue_id = ue_data.secondary_ue,
      .plmn.mcc = rrc->configuration.mcc[0],
      .plmn.mnc = rrc->configuration.mnc[0],
      .plmn.mnc_digit_length = rrc->configuration.mnc_digit_length[0],
      .nr_cellid = rrc->nr_cellid, // TODO target cell ID
      .servCellId = 0, // TODO: correct value?
      .ReconfigComplOutcome = RRCreconf_success,
      .transm_action_ind = &transmission_action_indicator,
  };
  deliver_ue_ctxt_modification_data_t data = {.rrc = rrc,
                                              .modification_req = &ue_context_modif_req,
                                              .assoc_id = ue_data.du_assoc_id};
  int srb_id = 1; /* 1 : 22 Apr 2025 Testing */
  nr_pdcp_data_req_srb(ue->rrc_ue_id,
                       srb_id,
                       rrc_gNB_mui++,
                       rrc_reconf_len,
                       (unsigned char *const)rrc_reconf,
                       rrc_deliver_ue_ctxt_modif_req_xn,
                       &data);
}

#ifdef UNI_RAN
void rrc_gNB_check_inactivity(gNB_RRC_INST *rrc)
{
  rrc_gNB_ue_context_t *ue_context = NULL;
  bool drb_active = false;

  RB_FOREACH(ue_context, rrc_nr_ue_tree_s, &rrc->rrc_ue_head) {
    gNB_RRC_UE_t *UE = &ue_context->ue_context;

    if ((!UE) || (UE &&  (UE->ue_rrc_inactivity_timer == 0 || UE->ue_rrc_srb_inactivity_timer == 0))) continue;
    //SRB stats checking start
    bool srb_inactivity_flag = false;
    for (int srb_id = 1; srb_id <= 2; srb_id++) {
       if (UE->Srb[srb_id].Active == 1) {
         LOG_D(RRC,"SRB-ID=%d ACTIVE\n", srb_id);
         nr_pdcp_statistics_t stats = {0};
         bool rc = nr_pdcp_get_statistics(UE->rrc_ue_id, 1, srb_id, &stats);

         if (!rc) {
            LOG_E(NR_RRC, "PDCP stats unavailable for UE %u SRB %d\n", UE->rrc_ue_id, srb_id);
            continue;
         }
         else {
            // Check if new TX or RX packets since last snapshot
            if (stats.txsdu_pkts > UE->last_txsdu_pkts_srb[srb_id] || stats.rxsdu_pkts > UE->last_rxsdu_pkts_srb[srb_id]) {
               LOG_D(RRC,"SRB status: UE %04x : Penultimate: TX SDU PKTS = %u" ", RX SDU PKTS = %u\n", UE->rnti, UE->last_txsdu_pkts_srb[srb_id], UE->last_rxsdu_pkts_srb[srb_id]);
               UE->last_txsdu_pkts_srb[srb_id] = stats.txsdu_pkts;
               UE->last_rxsdu_pkts_srb[srb_id] = stats.rxsdu_pkts;
               UE->ue_rrc_srb_inactivity_timer = 1;
               LOG_D(RRC,"SRB status: UE %04x : Current: TX SDU PKTS = %u" ", RX SDU PKTS = %u\n", UE->rnti, stats.txsdu_pkts, stats.rxsdu_pkts);
               LOG_D(RRC,"UE %04x :Signalling active, resetting SRB inactivity timer to 1\n", UE->rnti);
            }
            else {
               LOG_W(RRC,"SRB: Penultimate TX/RX SDU PKTS are same as Current TX/RX SDU PKTS\n");
               srb_inactivity_flag = true;
            }
       }
     }
   }
   if (srb_inactivity_flag)
   {
      UE->ue_rrc_srb_inactivity_timer++;
      LOG_I(RRC, "UE %04x : Signalling inactive, incrementing SRB inactivity timer, current value = %u\n", UE->rnti, UE->ue_rrc_srb_inactivity_timer);
   }
      //SRB stats checking end
   bool drb_inactivity_flag = false;
    for (int drb_id = 1; drb_id <=MAX_DRBS_PER_UE; drb_id++) {
       if (UE->established_drbs[drb_id-1].status != DRB_INACTIVE) {
         LOG_D(RRC,"DRB-ID=%d ACTIVE, PDCP stats Availiable\n", drb_id);
         drb_active = true;
         nr_pdcp_statistics_t stats = {0};
         bool rc = nr_pdcp_get_statistics(UE->rrc_ue_id, 0, drb_id, &stats);

         if (!rc) {
            LOG_E(NR_RRC, "PDCP stats unavailable for UE %u DRB %d\n", UE->rrc_ue_id, drb_id);
            continue;
         }
         else {
            // Check if new TX or RX packets since last snapshot
            if (stats.txsdu_pkts > UE->last_txsdu_pkts[drb_id] || stats.rxsdu_pkts > UE->last_rxsdu_pkts[drb_id]) {
               LOG_D(RRC,"DRB status: UE %04x : Penultimate: TX SDU PKTS = %u" ", RX SDU PKTS = %u\n", UE->rnti, UE->last_txsdu_pkts[drb_id], UE->last_rxsdu_pkts[drb_id]);
               UE->last_txsdu_pkts[drb_id] = stats.txsdu_pkts;
               UE->last_rxsdu_pkts[drb_id] = stats.rxsdu_pkts;
               UE->ue_rrc_inactivity_timer = 1;
               LOG_D(RRC,"DRB status: UE %04x : Current: TX SDU PKTS = %u" ", RX SDU PKTS = %u\n", UE->rnti, stats.txsdu_pkts, stats.rxsdu_pkts);
               LOG_D(RRC,"UE %04x :Data active, resetting DRB inactivity timer to 1\n", UE->rnti);
            }
            else {
               LOG_W(RRC,"DRB: Penultimate TX/RX SDU PKTS are same as Current TX/RX SDU PKTS\n");
               drb_inactivity_flag = true;
            }
       }
     }
     else
        if (UE->established_drbs[drb_id-1].status == DRB_INACTIVE)
              break;

   }
   if (drb_inactivity_flag)
   {
        UE->ue_rrc_inactivity_timer++;
        LOG_I(RRC, "UE %04x : Data inactive, incrementing DRB inactivity timer, current value = %u\n", UE->rnti, UE->ue_rrc_inactivity_timer);
   }
   if (UE->ue_rrc_srb_inactivity_timer >= 10)
   {
      if(!(drb_active))
      {
         LOG_E(NR_RRC, "Moving UE %04x to RRC IDLE, SRB inactivity timeout Value = %u reached\n", UE->rnti, UE->ue_rrc_srb_inactivity_timer);
         rrc_gNB_generate_RRCRelease(rrc, UE);
         UE->ue_rrc_srb_inactivity_timer = 0;
      }
      else
        UE->ue_rrc_srb_inactivity_timer = 1;
   }
   else if (UE->ue_rrc_inactivity_timer >= UE->ue_rrc_inactivity_threshold) {
      LOG_E(NR_RRC, "Moving UE %04x to RRC IDLE, DRB inactivity timeout Value = %u reached\n", UE->rnti, UE->ue_rrc_inactivity_threshold);
      ngap_cause_t cause_inactivity = {.type = NGAP_CAUSE_RADIO_NETWORK, .value = NGAP_CAUSE_RADIO_NETWORK_USER_INACTIVITY};
      rrc_gNB_send_NGAP_UE_CONTEXT_RELEASE_REQ(rrc->module_id, ue_context, cause_inactivity);
      UE->ue_rrc_inactivity_timer = 0;
    }
  }
}
#endif
