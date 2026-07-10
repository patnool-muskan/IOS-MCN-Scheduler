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
 * Copyright: Fraunhofer Heinrich Hertz Institute
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
#define _GNU_SOURCE
#include "netconf_data.h"
#include "netconf.h"
#include "common/log.h"
#include "netconf_session.h"
#include "telnet/telnet.h"

#include <sysrepo.h>
#include <libyang/libyang.h>

#define MAX_XPATH_ENTRIES 200

static char *xpath_running[MAX_XPATH_ENTRIES] = {0};
static char *values_running[MAX_XPATH_ENTRIES] = {0};

static char *xpath_operational[MAX_XPATH_ENTRIES] = {0};
static char *values_operational[MAX_XPATH_ENTRIES] = {0};

static const char *MANAGED_ELEMENT_XPATH = 0;
static const char *MANAGED_ELEMENT_XPATH_OPER = 0;
static const char *GNBDU_FUNCTION_XPATH = 0;
static const char *BWP_DOWNLINK_XPATH = 0;
static const char *BWP_UPLINK_XPATH = 0;
static const char *NRCELLDU_XPATH = 0;
static const char *O_RAN_NRCELLDU_XPATH = 0;
static const char *NPNIDENTITYLIST_XPATH = 0;
static const char *ALARMLIST_XPATH = 0;
static const char **ALARM_XPATH = 0;
static const char **ALARM_XPATH_OPER = 0;
static const char *GNBCUCP_FUNCTION_XPATH = 0;
static const char *NRCELLCU_XPATH = 0;
static const char *NRFREQRELATION_XPATH = 0;
static const char *GNBCUUP_FUNCTION_XPATH = 0;

static const config_t *netconf_config = 0;
static const alarm_t **netconf_alarms = 0;
static sr_subscription_ctx_t *netconf_data_subscription = 0;
static sr_subscription_ctx_t *netconf_ru_data_subscription = 0;

static int netconf_data_register_callbacks();
static int netconf_ru_data_register_callbacks();
static int netconf_data_unregister_callbacks();
static int netconf_data_edit_callback(sr_session_ctx_t *session,
                                      uint32_t sub_id,
                                      const char *module_name,
                                      const char *xpath_running,
                                      sr_event_t event,
                                      uint32_t request_id,
                                      void *private_data);
static int netconf_ru_data_edit_callback(sr_session_ctx_t *session,
                                         uint32_t sub_id,
                                         const char *module_name,
                                         const char *xpath_running,
                                         sr_event_t event,
                                         uint32_t request_id,
                                         void *private_data);

/* compact helpers */
#define ASPRINTF_OR_FAIL(dest, fmt, arg) do { if (asprintf(&(dest), (fmt), (arg)) == -1 || (dest) == NULL) { log_error("asprintf failed"); goto failure; } } while (0)
#define ASPF_OR_FAIL(var, ...) do { if (asprintf(&(var), __VA_ARGS__) == -1 || (var) == NULL) { log_error("asprintf failed"); goto failure; } } while(0)
#define ASPF_VAL_OR_FAIL(var, fmt, val) do { if (asprintf(&(var), fmt, (val)) == -1 || (var) == NULL) { log_error("asprintf failed"); goto failure; } } while(0)
#define STRDUP_OR_FAIL(dst, src) do { if (((dst) = strdup(src)) == NULL) { log_error("strdup failed"); goto failure; } } while(0)
#define FREE_RANGE(s,e) do { for (int _i = (s); _i < (e); _i++) { free(values_running[_i]); free(xpath_running[_i]); values_running[_i] = NULL; xpath_running[_i] = NULL; } } while(0)

extern char *nodeType;

int netconf_data_init(const config_t *config)
{
  if (config == 0) {
    log_error("config is null");
    goto failure;
  }

  netconf_config = config;
  netconf_alarms = 0;
  netconf_data_subscription = 0;
  netconf_ru_data_subscription = 0;

  MANAGED_ELEMENT_XPATH = 0;
  MANAGED_ELEMENT_XPATH_OPER = 0;
  ALARM_XPATH = 0;
  ALARM_XPATH_OPER = 0;

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBDU_FUNCTION_XPATH = 0;
    BWP_DOWNLINK_XPATH = 0;
    BWP_UPLINK_XPATH = 0;
    NRCELLDU_XPATH = 0;
    O_RAN_NRCELLDU_XPATH = 0;
    NPNIDENTITYLIST_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBCUCP_FUNCTION_XPATH = 0;
    NRCELLCU_XPATH = 0;
    NRFREQRELATION_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-up") == 0) {
    GNBCUUP_FUNCTION_XPATH = 0;
  }

  return 0;

failure:
  return 1;
}

int netconf_data_alarms_init(const alarm_t **alarms)
{
  int alarms_no = 0;
  netconf_alarms = alarms;
  while (*netconf_alarms) {
    netconf_alarms++;
    alarms_no++;
  }

  netconf_alarms = alarms;

  ALARM_XPATH = (const char **)malloc(sizeof(char *) * alarms_no);
  if (ALARM_XPATH == 0) {
    log_error("malloc failed");
    goto failed;
  }

  for (int i = 0; i < alarms_no; i++) {
    ALARM_XPATH[i] = 0;
  }

  ALARM_XPATH_OPER = (const char **)malloc(sizeof(char *) * alarms_no);
  if (ALARM_XPATH_OPER == 0) {
    log_error("malloc failed");
    goto failed;
  }

  for (int i = 0; i < alarms_no; i++) {
    ALARM_XPATH_OPER[i] = 0;
  }

  return 0;
failed:
  free(ALARM_XPATH);
  ALARM_XPATH = 0;
  free(ALARM_XPATH_OPER);
  ALARM_XPATH_OPER = 0;
  return 1;
}

int netconf_data_free()
{
  for (int i = 0; i < MAX_XPATH_ENTRIES; i++) {
    free(xpath_running[i]);
    xpath_running[i] = 0;
    free(values_running[i]);
    values_running[i] = 0;

    free(xpath_operational[i]);
    xpath_operational[i] = 0;
    free(values_operational[i]);
    values_operational[i] = 0;
  }

  free(ALARM_XPATH);
  ALARM_XPATH = 0;

  free(ALARM_XPATH_OPER);
  ALARM_XPATH_OPER = 0;

  MANAGED_ELEMENT_XPATH = 0;
  MANAGED_ELEMENT_XPATH_OPER = 0;
  ALARMLIST_XPATH = 0;

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBDU_FUNCTION_XPATH = 0;
    BWP_DOWNLINK_XPATH = 0;
    BWP_UPLINK_XPATH = 0;
    NRCELLDU_XPATH = 0;
    O_RAN_NRCELLDU_XPATH = 0;
    NPNIDENTITYLIST_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBCUCP_FUNCTION_XPATH = 0;
    NRCELLCU_XPATH = 0;
    NRFREQRELATION_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-up") == 0) {
    GNBCUCP_FUNCTION_XPATH = 0;
  }

  netconf_data_unregister_callbacks();

  return 0;
}

int netconf_data_update_full(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (MANAGED_ELEMENT_XPATH) {
    rc = sr_delete_item(netconf_session_running, MANAGED_ELEMENT_XPATH, SR_EDIT_STRICT);
    if (rc != SR_ERR_OK) {
      log_error("sr_delete_item failure");
      goto failure;
    }

    rc = sr_apply_changes(netconf_session_running, 0);
    if (rc != SR_ERR_OK) {
      log_error("sr_apply_changes failed");
      goto failure;
    }
  }

  for (int i = 0; i < MAX_XPATH_ENTRIES; i++) {
    free(xpath_running[i]);
    xpath_running[i] = 0;
    free(values_running[i]);
    values_running[i] = 0;

    free(xpath_operational[i]);
    xpath_operational[i] = 0;
    free(values_operational[i]);
    values_operational[i] = 0;
  }

  MANAGED_ELEMENT_XPATH = 0;
  MANAGED_ELEMENT_XPATH_OPER = 0;
  ALARMLIST_XPATH = 0;

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBDU_FUNCTION_XPATH = 0;
    BWP_DOWNLINK_XPATH = 0;
    BWP_UPLINK_XPATH = 0;
    NRCELLDU_XPATH = 0;
    O_RAN_NRCELLDU_XPATH = 0;
    NPNIDENTITYLIST_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBCUCP_FUNCTION_XPATH = 0;
    NRCELLCU_XPATH = 0;
    NRFREQRELATION_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-up") == 0) {
    GNBCUUP_FUNCTION_XPATH = 0;
  }

  int k_running = 0, k_operational = 0;

  ASPRINTF_OR_FAIL(xpath_running[k_running], "/_3gpp-common-managed-element:ManagedElement[id='ManagedElement=%s']", netconf_config->info.node_id);
  MANAGED_ELEMENT_XPATH = xpath_running[k_running];
  k_running++;

  ASPRINTF_OR_FAIL(xpath_operational[k_operational], "/_3gpp-common-managed-element:ManagedElement[id='ManagedElement=%s']", netconf_config->info.node_id);
  MANAGED_ELEMENT_XPATH_OPER = xpath_operational[k_operational];
  k_operational++;

  values_running[k_running] = strdup("1");
  if (values_running[k_running] == 0) {
    log_error("strdup failed");
    goto failure;
  }
  ASPRINTF_OR_FAIL(xpath_running[k_running], "%s/attributes/priorityLabel", MANAGED_ELEMENT_XPATH);
  k_running++;

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    ASPRINTF_OR_FAIL(values_operational[k_running], "ip-v4-address=%s", netconf_config->network.host);
    ASPRINTF_OR_FAIL(xpath_operational[k_running], "%s/attributes/dnPrefix", MANAGED_ELEMENT_XPATH);
    k_running++;

    values_operational[k_operational] = strdup(netconf_config->info.location_name);
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/locationName", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup(netconf_config->info.managed_by);
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/managedBy", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup(netconf_config->info.managed_element_type);
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/managedElementTypeList", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup(oai->device_data.vendor);
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/vendorName", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup(netconf_config->software_version);
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/swVersion", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.UEThpDl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.UEThpUl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.MeanActiveUeDl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.MaxActiveUeDl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.MeanActiveUeUl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("DRB.MaxActiveUeUl");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/performanceMetrics", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("900");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/granularityPeriods", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("FILE_BASED_LOC_SET_BY_PRODUCER");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/reportingMethods", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;

    values_operational[k_operational] = strdup("FILE_BASED_LOC_SET_BY_CONSUMER");
    if (values_operational[k_operational] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_operational[k_operational], "%s/attributes/SupportedPerfMetricGroups/reportingMethods", MANAGED_ELEMENT_XPATH_OPER);
    k_operational++;
  }

  if (strcmp(nodeType, "cu-up") == 0) {
    ASPRINTF_OR_FAIL(xpath_running[k_running],
                     "%s/_3gpp-nr-nrm-gnbcuupfunction:GNBCUUPFunction[id='ManagedElement=%s,GNBCUCPFunction=%d']",
                     MANAGED_ELEMENT_XPATH);
    /* Note: above multi-arg format from original requires keeping the original asprintf form.
       Restore explicit asprintf for proper arguments: */
    if (asprintf(&xpath_running[k_running],
                 "%s/_3gpp-nr-nrm-gnbcuupfunction:GNBCUUPFunction[id='ManagedElement=%s,GNBCUCPFunction=%d']",
                 MANAGED_ELEMENT_XPATH,
                 netconf_config->info.node_id,
                 netconf_config->info.gnb_cu_id) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    GNBCUUP_FUNCTION_XPATH = xpath_running[k_running];
    k_running++;

    values_running[k_running] = strdup("1");
    if (values_running[k_running] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_running[k_running], "%s/attributes/priorityLabel", GNBCUUP_FUNCTION_XPATH);
    k_running++;

    ASPRINTF_OR_FAIL(values_running[k_running], "%d", oai->gnbcuup.gNBId);
    ASPRINTF_OR_FAIL(xpath_running[k_running], "%s/attributes/gNBId", GNBCUUP_FUNCTION_XPATH);
    k_running++;

    values_running[k_running] = strdup("32"); // The value is taken as 32 because the length of GNBID is 32 bits
    if (values_running[k_running] == 0) {
      log_error("strdup failed");
      goto failure;
    }
    ASPRINTF_OR_FAIL(xpath_running[k_running], "%s/attributes/gNBIdLength", GNBCUUP_FUNCTION_XPATH);
    k_running++;

    /* multi-arg asprintf for pLMNInfoList kept as original to supply all args */
    if (asprintf(&xpath_running[k_running],
                 "%s/attributes/pLMNInfoList[mcc='%s'][mnc='%s'][sd='%s'][sst='%d']",
                 GNBCUUP_FUNCTION_XPATH,
                 oai->gnbcuup.mcc,
                 oai->gnbcuup.mnc,
                 "00:00:00",
                 1) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    k_running++;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    /* GNBCUCPFunction entry */
    ASPF_OR_FAIL(xpath_running[k_running],
           "%s/_3gpp-nr-nrm-gnbcucpfunction:GNBCUCPFunction[id='ManagedElement=%s,GNBCUCPFunction=%d']",
           MANAGED_ELEMENT_XPATH,
           netconf_config->info.node_id,
           netconf_config->info.gnb_cu_id);
    GNBCUCP_FUNCTION_XPATH = xpath_running[k_running++];
    /* compact attribute population for GNBCUCPFunction */
    const char *gnbcu_attrs[] = {
      "priorityLabel", "gNBId", "gNBIdLength", "gNBCUName"
    };
    for (size_t ai = 0; ai < sizeof(gnbcu_attrs)/sizeof(gnbcu_attrs[0]); ++ai) {
      switch (ai) {
      case 0: STRDUP_OR_FAIL(values_running[k_running], "1"); break;
      case 1: ASPF_OR_FAIL(values_running[k_running], "%d", oai->device_data.gnbId); break;
      case 2: STRDUP_OR_FAIL(values_running[k_running], "32"); break;
      case 3: ASPF_OR_FAIL(values_running[k_running], "%s-CU-%d", oai->device_data.gnbCUName, netconf_config->info.gnb_cu_id); break;
      }
      ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", GNBCUCP_FUNCTION_XPATH, gnbcu_attrs[ai]);
      k_running++;
    }

    /* pLMNId element (no direct value, just create node xpath) */
    ASPF_OR_FAIL(xpath_running[k_running],
           "%s/attributes/pLMNId[mcc='%s'][mnc='%s']",
           GNBCUCP_FUNCTION_XPATH,
           oai->nrcellcu.mcc,
           oai->nrcellcu.mnc);
    k_running++;

    /* qceIdMappingInfoList node and its pLMNTarget child with value 0 */
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/qceIdMappingInfoList[idx='']", GNBCUCP_FUNCTION_XPATH);
    k_running++;

    STRDUP_OR_FAIL(values_running[k_running], "0");
    ASPF_OR_FAIL(xpath_running[k_running],
           "%s/attributes/qceIdMappingInfoList[idx='']/pLMNTarget[mcc='%s'][mnc='%s']",
           GNBCUCP_FUNCTION_XPATH,
           oai->nrcellcu.mcc,
           oai->nrcellcu.mnc);
    k_running++;
  }

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    /* GNBDUFunction */
    ASPF_OR_FAIL(xpath_running[k_running],
         "%s/_3gpp-nr-nrm-gnbdufunction:GNBDUFunction[id='ManagedElement=%s,GNBDUFunction=%d']",
         MANAGED_ELEMENT_XPATH,
         netconf_config->info.node_id,
         netconf_config->info.gnb_du_id);
    GNBDU_FUNCTION_XPATH = xpath_running[k_running++];
    /* GNBDU attributes (compact loop) */
    {
      const char *attrs[] = { "priorityLabel", "gNBId", "gNBIdLength", "gNBDUId", "gNBDUName" };
      for (size_t ai = 0; ai < sizeof(attrs)/sizeof(attrs[0]); ++ai) {
      switch (ai) {
        case 0: ASPF_OR_FAIL(values_running[k_running], "%s", "1"); break;
        case 1: ASPF_OR_FAIL(values_running[k_running], "%d", oai->device_data.gnbId); break;
        case 2: ASPF_OR_FAIL(values_running[k_running], "%s", "32"); break;
        case 3: ASPF_OR_FAIL(values_running[k_running], "%d", netconf_config->info.gnb_du_id); break;
        case 4: ASPF_OR_FAIL(values_running[k_running], "%s-DU-%d", oai->device_data.gnbName, netconf_config->info.gnb_du_id); break;
      }
      ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", GNBDU_FUNCTION_XPATH, attrs[ai]);
      k_running++;
      }
    }

    /* BWP entries (Downlink / Uplink) compacted */
    for (int dir = 0; dir < 2; ++dir) {
      const char *id = (dir == 0) ? "Downlink" : "Uplink";
      int bwp_idx = dir; /* 0 -> DL, 1 -> UL */
      ASPF_OR_FAIL(xpath_running[k_running],
         "%s/_3gpp-nr-nrm-bwp:BWP[id='%s']",
         GNBDU_FUNCTION_XPATH,
         id);
      if (dir == 0) BWP_DOWNLINK_XPATH = xpath_running[k_running];
      else BWP_UPLINK_XPATH = xpath_running[k_running];
      k_running++;

      const char *attrs[] = { "priorityLabel", "subCarrierSpacing", "bwpContext", "isInitialBwp", "cyclicPrefix", "startRB", "numberOfRBs" };
      for (size_t ai = 0; ai < sizeof(attrs)/sizeof(attrs[0]); ++ai) {
      switch (ai) {
        case 0:  ASPF_OR_FAIL(values_running[k_running], "%s", "1"); break;
        case 1:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[bwp_idx].subCarrierSpacing); break;
        case 2:  ASPF_OR_FAIL(values_running[k_running], "%s", (dir == 0) ? "DL" : "UL"); break;
        case 3:  ASPF_OR_FAIL(values_running[k_running], "%s", oai->bwp[bwp_idx].isInitialBwp ? "INITIAL" : "OTHER"); break;
        case 4:  ASPF_OR_FAIL(values_running[k_running], "%s", "NORMAL"); break;
        case 5:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[bwp_idx].startRB); break;
        case 6:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[bwp_idx].numberOfRBs); break;
        default: ASPF_OR_FAIL(values_running[k_running], "%s", ""); break;
      }
      ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", (dir == 0) ? BWP_DOWNLINK_XPATH : BWP_UPLINK_XPATH, attrs[ai]);
      k_running++;
      }
    }

    /* NRCellDU */
    ASPF_OR_FAIL(xpath_running[k_running],
         "%s/_3gpp-nr-nrm-nrcelldu:NRCellDU[id='ManagedElement=%s,GNBDUFunction=%d,NRCellDu=0']",
         GNBDU_FUNCTION_XPATH,
         netconf_config->info.node_id,
         netconf_config->info.gnb_du_id);
    NRCELLDU_XPATH = xpath_running[k_running++];
    /* priorityLabel and cellLocalId */
    ASPF_OR_FAIL(values_running[k_running], "%s", "1"); ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/priorityLabel", NRCELLDU_XPATH); k_running++;
    ASPF_OR_FAIL(values_running[k_running], "%d", netconf_config->info.cell_local_id); ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/cellLocalId", NRCELLDU_XPATH); k_running++;

    /* pLMNInfoList with formatted sd */
    char sdHex[9];
    sprintf(sdHex, "%06x", oai->nrcelldu.sd);
    sdHex[8] = 0; sdHex[7] = sdHex[5]; sdHex[6] = sdHex[4]; sdHex[5] = ':'; sdHex[4] = sdHex[3]; sdHex[3] = sdHex[2]; sdHex[2] = ':';
    ASPF_OR_FAIL(xpath_running[k_running],
         "%s/attributes/pLMNInfoList[mcc='%s'][mnc='%s'][sd='%s'][sst='%d']",
         NRCELLDU_XPATH,
         oai->nrcelldu.mcc,
         oai->nrcelldu.mnc,
         sdHex,
         oai->nrcelldu.sst);
    k_running++;

    /* nPNIdentityList and its children */
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/nPNIdentityList[idx='%d']", NRCELLDU_XPATH, 0);
    NPNIDENTITYLIST_XPATH = xpath_running[k_running++];
    ASPF_OR_FAIL(xpath_running[k_running], "%s/plmnid[mcc='%s'][mnc='%s']", NPNIDENTITYLIST_XPATH, oai->nrcelldu.mcc, oai->nrcelldu.mnc); k_running++;
    ASPF_OR_FAIL(values_running[k_running], "%s", "1"); ASPF_OR_FAIL(xpath_running[k_running], "%s/cAGIdList", NPNIDENTITYLIST_XPATH); k_running++;
    ASPF_OR_FAIL(values_running[k_running], "%s", "1"); ASPF_OR_FAIL(xpath_running[k_running], "%s/nIDList", NPNIDENTITYLIST_XPATH); k_running++;

    /* Compact list of NRCELLDU attributes */
    {
      const char *attrs[] = {
      "nRPCI", "arfcnDL", "arfcnUL", "bSChannelBwDL", "bSChannelBwUL",
      "rimRSMonitoringStartTime", "rimRSMonitoringStopTime",
      "rimRSMonitoringWindowDuration", "rimRSMonitoringWindowStartingOffset",
      "rimRSMonitoringWindowPeriodicity", "rimRSMonitoringOccasionInterval",
      "rimRSMonitoringOccasionStartingOffset", "ssbFrequency", "ssbPeriodicity",
      "ssbSubCarrierSpacing", "ssbOffset", "ssbDuration",
      "nRSectorCarrierRef", "victimSetRef", "aggressorSetRef"
      };
      for (size_t ai = 0; ai < sizeof(attrs)/sizeof(attrs[0]); ++ai) {
      switch (ai) {
        case 0:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.nRPCI); break;
        case 1:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.arfcnDL); break;
        case 2:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.arfcnUL); break;
        case 3:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.bSChannelBwDL); break;
        case 4:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.bSChannelBwUL); break;
        case 5:  ASPF_OR_FAIL(values_running[k_running], "%s", "2023-06-06T00:00:00Z"); break;
        case 6:  ASPF_OR_FAIL(values_running[k_running], "%s", "2023-06-06T00:00:00Z"); break;
        case 7:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 8:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 9:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 10: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 11: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 12: ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.ssbFrequency); break;
        case 13: ASPF_OR_FAIL(values_running[k_running], "%d", 5); break;
        case 14: ASPF_OR_FAIL(values_running[k_running], "%d", 15); break;
        case 15: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 16: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
        case 17: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
        case 18: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
        case 19: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
        default: ASPF_OR_FAIL(values_running[k_running], "%s", ""); break;
      }
      ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", NRCELLDU_XPATH, attrs[ai]);
      k_running++;
      }
    }

    if(mplane_enabled) {
      /* Compact bulk set helper */
      typedef enum { T_INT, T_UINT, T_STR } val_t;
      struct set_item {
        const char *xpath;
        val_t type;
        int apply_after; /* call sr_apply_changes(netconf_session_running) after this item */
        int operational; /* 1 -> operational session, 0 -> running session */
        union {
          int i;
          unsigned u;
          const char *s;
        } v;
      };

      char str[64];

      struct set_item items[] = {
        /* delay-management -> operational */
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-min-up",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_min_up },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-max-up",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_max_up },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-min-cp-dl",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_min_cp_dl },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-max-cp-dl",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_max_cp_dl },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/tcp-adv-dl",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.Tcp_adv_dl },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/ta3-min",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.Ta3_min },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/ta3-max",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.Ta3_max },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-min-cp-ul",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_min_cp_ul },
        { "/o-ran-delay-management:delay-management/bandwidth-scs-delay-state[bandwidth='100000'][subcarrier-spacing='30000']/ru-delay-profile/t2a-max-cp-ul",
          T_INT, 0, 1, .v.i = oai->oru.delay_management.T2a_max_cp_ul },

        /* tx-array-carriers -> running */
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/absolute-frequency-center",
          T_INT, 0, 0, .v.i = oai->oru.tx_array_carrier.arfcn_center },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/center-of-channel-bandwidth",
          T_UINT, 0, 0, .v.u = (unsigned)oai->oru.tx_array_carrier.center_channel_bw },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/channel-bandwidth",
          T_INT, 0, 0, .v.i = oai->oru.tx_array_carrier.channel_bw },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/active",
          T_STR, 0, 0, .v.s = oai->oru.tx_array_carrier.ru_carrier },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/rw-duplex-scheme",
          T_STR, 0, 0, .v.s = oai->oru.tx_array_carrier.rw_duplex_scheme },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/rw-type",
          T_STR, 0, 0, .v.s = oai->oru.tx_array_carrier.rw_type },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/gain",
          T_INT, 0, 0, .v.i = oai->oru.tx_array_carrier.gain },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/downlink-radio-frame-offset",
          T_INT, 0, 0, .v.i = oai->oru.tx_array_carrier.dl_radio_frame_offset },
        { "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/downlink-sfn-offset",
          T_INT, 1, 0, .v.i = oai->oru.tx_array_carrier.dl_sfn_offset }, /* apply after this */

        /* rx-array-carriers -> running */
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/absolute-frequency-center",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.arfcn_center },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/center-of-channel-bandwidth",
          T_UINT, 0, 0, .v.u = (unsigned)oai->oru.rx_array_carrier.center_channel_bw },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/channel-bandwidth",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.channel_bw },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/active",
          T_STR, 0, 0, .v.s = oai->oru.tx_array_carrier.ru_carrier },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/downlink-radio-frame-offset",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.dl_radio_frame_offset },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/downlink-sfn-offset",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.dl_sfn_offset },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/gain-correction",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.gain_correction },
        { "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/n-ta-offset",
          T_INT, 0, 0, .v.i = oai->oru.rx_array_carrier.nta_offset }
      };

      int n_items = sizeof(items) / sizeof(items[0]);
      for (int idx = 0; idx < n_items; idx++) {
        struct set_item *it = &items[idx];
        sr_session_ctx_t *sess = it->operational ? netconf_session_operational : netconf_session_running;
        if (it->type == T_INT) {
          snprintf(str, sizeof(str), "%d", it->v.i);
          rc = sr_set_item_str(sess, it->xpath, str, NULL, 0);
        } else if (it->type == T_UINT) {
          snprintf(str, sizeof(str), "%u", it->v.u);
          rc = sr_set_item_str(sess, it->xpath, str, NULL, 0);
        } else { /* T_STR */
          rc = sr_set_item_str(sess, it->xpath, it->v.s ? it->v.s : "", NULL, 0);
        }

        if (it->apply_after) {
          rc = sr_apply_changes(netconf_session_running, 0);
        }
      }

      rc = sr_apply_changes(netconf_session_running, 0);

      rc = netconf_ru_data_register_callbacks();
      if (rc != 0) {
        log_error("netconf_ru_data_register_callbacks");
        goto failure;
      }
    }
  }

  /* helper macros already available above (ASPF_OR_FAIL, STRDUP_OR_FAIL) are used here
     to keep this block compact while preserving original semantics */
  ASPF_OR_FAIL(xpath_running[k_running],
               "%s/_3gpp-common-managed-element:AlarmList[id='ManagedElement=%s,AlarmList=1']",
               MANAGED_ELEMENT_XPATH,
               netconf_config->info.node_id);
  ALARMLIST_XPATH = xpath_running[k_running++];
  ASPF_OR_FAIL(xpath_operational[k_operational],
               "%s/_3gpp-common-managed-element:AlarmList[id='ManagedElement=%s,AlarmList=1']",
               MANAGED_ELEMENT_XPATH,
               netconf_config->info.node_id);
  k_operational++;

  const alarm_t **alarm = netconf_alarms;
  int i = 0;
  while (*alarm) {
    /* running alarm record */
    ASPF_OR_FAIL(xpath_running[k_running],
                 "%s/attributes/alarmRecords[alarmId='%s-%s']",
                 ALARMLIST_XPATH,
                 (*alarm)->object_instance,
                 (*alarm)->alarm);
    ALARM_XPATH[i] = xpath_running[k_running++];

    alarm_severity_t severity = ((*alarm)->state != ALARM_STATE_CLEARED) ? (*alarm)->severity : ALARM_SEVERITY_CLEARED;
    STRDUP_OR_FAIL(values_running[k_running], alarm_severity_to_str(severity));
    ASPF_OR_FAIL(xpath_running[k_running], "%s/perceivedSeverity", ALARM_XPATH[i]);
    k_running++;

    /* operational alarm record */
    ASPF_OR_FAIL(xpath_operational[k_operational],
                 "%s/attributes/alarmRecords[alarmId='%s-%s']",
                 ALARMLIST_XPATH,
                 (*alarm)->object_instance,
                 (*alarm)->alarm);
    ALARM_XPATH_OPER[i] = xpath_operational[k_operational++];

    STRDUP_OR_FAIL(values_operational[k_operational], (*alarm)->object_instance);
    ASPF_OR_FAIL(xpath_operational[k_operational], "%s/objectInstance", ALARM_XPATH_OPER[i]);
    k_operational++;

    ASPF_OR_FAIL(values_operational[k_operational], "%d", 0);
    ASPF_OR_FAIL(xpath_operational[k_operational], "%s/notificationId", ALARM_XPATH_OPER[i]);
    k_operational++;

    STRDUP_OR_FAIL(values_operational[k_operational], alarm_type_to_str((*alarm)->type));
    ASPF_OR_FAIL(xpath_operational[k_operational], "%s/alarmType", ALARM_XPATH_OPER[i]);
    k_operational++;

    STRDUP_OR_FAIL(values_operational[k_operational], "unset");
    ASPF_OR_FAIL(xpath_operational[k_operational], "%s/probableCause", ALARM_XPATH_OPER[i]);
    k_operational++;

    /* leave time fields empty (placeholders) */
    k_operational += 3;

    i++;
    alarm++;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    ASPF_OR_FAIL(xpath_running[k_running],
                 "%s/_3gpp-nr-nrm-nrcellcu:NRCellCU[id='ManagedElement=%s,NRCellCu=0']",
                 GNBCUCP_FUNCTION_XPATH,
                 netconf_config->info.node_id);
    NRCELLCU_XPATH = xpath_running[k_running++];
    STRDUP_OR_FAIL(values_running[k_running], "1");
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/priorityLabel", NRCELLCU_XPATH);
    k_running++;

    ASPF_OR_FAIL(values_running[k_running], "%d", netconf_config->info.cell_local_id);
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/cellLocalId", NRCELLCU_XPATH);
    k_running++;

    char sdHex[9];
    sprintf(sdHex, "%06x", oai->nrcellcu.sd);
    sdHex[8] = 0;
    sdHex[7] = sdHex[5];
    sdHex[6] = sdHex[4];
    sdHex[5] = ':';
    sdHex[4] = sdHex[3];
    sdHex[3] = sdHex[2];
    sdHex[2] = ':';

    /* pLMN info */
    if (asprintf(&xpath_running[k_running],
           "%s/attributes/pLMNInfoList[mcc='%s'][mnc='%s'][sd='%s'][sst='%d']",
           NRCELLCU_XPATH,
           oai->nrcellcu.mcc,
           oai->nrcellcu.mnc,
           sdHex,
           oai->nrcellcu.sst) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    k_running++;

    /* NRFreqRelation entry */
    if (asprintf(&xpath_running[k_running],
           "%s/_3gpp-nr-nrm-nrfreqrelation:NRFreqRelation[id='ManagedElement=%s,NRFreqRelation=0']",
           NRCELLCU_XPATH,
           netconf_config->info.node_id) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    NRFREQRELATION_XPATH = xpath_running[k_running++];
    
    /* compact array-driven population of attributes (use stable defaults) */
    #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
    struct { const char *attr; const char *val; } nrf_attrs[] = {
      { "qRxLevMin",                    "-100" },
      { "threshXHighP",                 "40"   },
      { "threshXHighQ",                 "30"   },
      { "threshXLowP",                  "5"    },
      { "threshXLowQ",                  "5"    },
      { "tReselectionNR",               "2"    },
      { "tReselectionNRSfHigh",         "75"   },
      { "tReselectionNRSfMedium",       "50"   },
      { "nRFrequencyRef",               "ManagedElement=oai" },
      { "cellReselectionPriority",      "32"   },
      { "cellReselectionSubPriority",   "2"    }
    };

    for (size_t ai = 0; ai < ARRAY_SIZE(nrf_attrs); ai++) {
      if (asprintf(&xpath_running[k_running], "%s/attributes/%s", NRFREQRELATION_XPATH, nrf_attrs[ai].attr) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
      }
      values_running[k_running] = strdup(nrf_attrs[ai].val);
      if (values_running[k_running] == NULL) {
      log_error("strdup failed");
      goto failure;
      }
      k_running++;
    }
    #undef ARRAY_SIZE

    /* pMax comes from oai data */
    if (asprintf(&values_running[k_running], "%d", oai->nrfreqrel.pMax) == -1 || values_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    if (asprintf(&xpath_running[k_running], "%s/attributes/pMax", NRFREQRELATION_XPATH) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    k_running++;
  }

  if (k_running) {
    for (int i = 0; i < k_running; i++) {
      if (xpath_running[i]) {
        log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
        rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
        if (rc != SR_ERR_OK) {
          log_error("sr_set_item_str failed");
          goto failure;
        }
      }
    }

    rc = sr_apply_changes(netconf_session_running, 0);
    if (rc != SR_ERR_OK) {
      log_error("sr_apply_changes failed");
      goto failure;
    }
  }

  if (k_operational) {
    for (int i = 0; i < k_operational; i++) {
      if (xpath_operational[i]) {
        log("[oper] populating %s with %s.. ", xpath_operational[i], values_operational[i]);
        rc = sr_set_item_str(netconf_session_operational, xpath_operational[i], values_operational[i], 0, 0);
        if (rc != SR_ERR_OK) {
          log_error("sr_set_item_str failed");
          goto failure;
        }
      }
    }

    rc = sr_apply_changes(netconf_session_operational, 0);
    if (rc != SR_ERR_OK) {
      log_error("sr_apply_changes failed");
      goto failure;
    }
  }

  rc = netconf_data_register_callbacks();
  if (rc != 0) {
    log_error("netconf_data_register_callbacks");
    goto failure;
  }

  return 0;

failure:
  for (int i = 0; i < k_running; i++) {
    free(xpath_running[i]);
    xpath_running[i] = 0;
    free(values_running[i]);
    values_running[i] = 0;
  }

  for (int i = 0; i < k_operational; i++) {
    free(xpath_operational[i]);
    xpath_operational[i] = 0;
    free(values_operational[i]);
    values_operational[i] = 0;
  }

  MANAGED_ELEMENT_XPATH = 0;
  ALARMLIST_XPATH = 0;

  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBDU_FUNCTION_XPATH = 0;
    BWP_DOWNLINK_XPATH = 0;
    BWP_UPLINK_XPATH = 0;
    NRCELLDU_XPATH = 0;
    O_RAN_NRCELLDU_XPATH = 0;
    NPNIDENTITYLIST_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBCUCP_FUNCTION_XPATH = 0;
    NRCELLCU_XPATH = 0;
    NRFREQRELATION_XPATH = 0;
  }

  if (strcmp(nodeType, "cu-up") == 0 || strcmp(nodeType, "gNB") == 0) {
    GNBCUUP_FUNCTION_XPATH = 0;
  }

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  return 1;
}

int netconf_data_update_gnbcuup(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) { log_error("netconf_data_unregister_callbacks"); goto failure; }

  if (oai == 0) { log_error("oai is null"); goto failure; }
  if (GNBCUUP_FUNCTION_XPATH == 0) { log_error("GNBCUUP_FUNCTION_XPATH is null"); goto failure; }

  /* locate existing GNBCUUP_FUNCTION_XPATH entry */
  int k_running = find_index(xpath_running, (char *)GNBCUUP_FUNCTION_XPATH);
  if (k_running < 0) { log_error("GNBCUUP_FUNCTION_XPATH not found among running xpaths"); goto failure; }

  int start_k_running = k_running;
  int stop_k_running = k_running;
  while (xpath_running[stop_k_running] &&
         strstr(xpath_running[stop_k_running], GNBCUUP_FUNCTION_XPATH) == xpath_running[stop_k_running]) stop_k_running++;

  rc = sr_delete_item(netconf_session_running, GNBCUUP_FUNCTION_XPATH, SR_EDIT_STRICT);
  if (rc != SR_ERR_OK) { log_error("sr_delete_item failure"); goto failure; }
  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  FREE_RANGE(start_k_running, stop_k_running);

  /* rebuild GNBCUUPFunction and compactly populate its attributes */
  ASPF_OR_FAIL(xpath_running[k_running],
               "%s/_3gpp-nr-nrm-gnbcuupfunction:GNBCUUPFunction[id='ManagedElement=%s,GNBCUCPFunction=%d']",
               MANAGED_ELEMENT_XPATH,
               netconf_config->info.node_id,
               netconf_config->info.gnb_cu_id);
  GNBCUUP_FUNCTION_XPATH = xpath_running[k_running++];

  STRDUP_OR_FAIL(values_running[k_running], "1");
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/attributes/priorityLabel", GNBCUUP_FUNCTION_XPATH);

  ASPF_VAL_OR_FAIL(values_running[k_running], "%d", oai->gnbcuup.gNBId);
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/attributes/gNBId", GNBCUUP_FUNCTION_XPATH);

  STRDUP_OR_FAIL(values_running[k_running], "32");
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/attributes/gNBIdLength", GNBCUUP_FUNCTION_XPATH);

  ASPF_OR_FAIL(xpath_running[k_running++],
               "%s/attributes/pLMNInfoList[mcc='%s'][mnc='%s'][sd='%s'][sst='%d']",
               GNBCUUP_FUNCTION_XPATH,
               oai->gnbcuup.mcc,
               oai->gnbcuup.mnc,
               "00:00:00",
               1);

  /* write new entries into running datastore */
  for (int i = start_k_running; i < k_running; ++i) {
    if (xpath_running[i]) {
      log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
      if (rc != SR_ERR_OK) { log_error("sr_set_item_str failed"); goto failure; }
    }
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  rc = netconf_data_register_callbacks();
  if (rc != 0) { log_error("netconf_data_register_callbacks"); goto failure; }

  return 0;

failure:
  rc = netconf_data_unregister_callbacks();
  if (rc != 0) { log_error("netconf_data_unregister_callbacks"); goto failure; }
  return 1;
}

int netconf_data_update_bwp_dl(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (oai == 0) {
    log_error("oai is null");
    goto failure;
  }

  if (BWP_DOWNLINK_XPATH == 0) {
    log_error("BWP_DOWNLINK_XPATH is null");
    goto failure;
  }

  /* find k_running */
  int k_running = 0;
  while (k_running < MAX_XPATH_ENTRIES) {
    if (xpath_running[k_running] == BWP_DOWNLINK_XPATH) break;
    k_running++;
  }
  if (k_running >= MAX_XPATH_ENTRIES) {
    log_error("BWP_DOWNLINK_XPATH not found among running xpaths");
    goto failure;
  }

  int start_k_running = k_running;
  int stop_k_running = k_running;
  while (xpath_running[stop_k_running] &&
         strstr(xpath_running[stop_k_running], BWP_DOWNLINK_XPATH) == xpath_running[stop_k_running]) {
    stop_k_running++;
  }

  rc = sr_delete_item(netconf_session_running, BWP_DOWNLINK_XPATH, SR_EDIT_STRICT);
  if (rc != SR_ERR_OK) {
    log_error("sr_delete_item failure");
    goto failure;
  }
  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) {
    log_error("sr_apply_changes failed");
    goto failure;
  }

  for (int i = start_k_running; i < stop_k_running; i++) {
    free(values_running[i]);
    free(xpath_running[i]);
    values_running[i] = 0;
    xpath_running[i] = 0;
  }

  if (asprintf(&xpath_running[k_running], "%s/_3gpp-nr-nrm-bwp:BWP[id='Downlink']", GNBDU_FUNCTION_XPATH) == -1 || xpath_running[k_running] == NULL) {
    log_error("asprintf failed");
    goto failure;
  }
  BWP_DOWNLINK_XPATH = xpath_running[k_running++];
  /* attribute list for Downlink BWP (compact population) */
  const char *attrs[] = {
    "priorityLabel", "subCarrierSpacing", "bwpContext",
    "isInitialBwp", "cyclicPrefix", "startRB", "numberOfRBs"
  };
  int n_attrs = sizeof(attrs) / sizeof(attrs[0]);

  for (int ai = 0; ai < n_attrs; ++ai) {
    /* prepare value depending on attribute */
    if (ai == 0) { /* priorityLabel */
      if (asprintf(&values_running[k_running], "1") == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else if (ai == 1) { /* subCarrierSpacing */
      if (asprintf(&values_running[k_running], "%d", oai->bwp[0].subCarrierSpacing) == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else if (ai == 2) { /* bwpContext */
      if (asprintf(&values_running[k_running], "%s", "DL") == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else if (ai == 3) { /* isInitialBwp */
      if (asprintf(&values_running[k_running], "%s", (oai->bwp[0].isInitialBwp) ? "INITIAL" : "OTHER") == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else if (ai == 4) { /* cyclicPrefix */
      if (asprintf(&values_running[k_running], "%s", "NORMAL") == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else if (ai == 5) { /* startRB */
      if (asprintf(&values_running[k_running], "%d", oai->bwp[0].startRB) == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    } else { /* numberOfRBs */
      if (asprintf(&values_running[k_running], "%d", oai->bwp[0].numberOfRBs) == -1 || values_running[k_running] == NULL) { log_error("asprintf failed"); goto failure; }
    }

    if (asprintf(&xpath_running[k_running], "%s/attributes/%s", BWP_DOWNLINK_XPATH, attrs[ai]) == -1 || xpath_running[k_running] == NULL) {
      log_error("asprintf failed");
      goto failure;
    }
    k_running++;
  }

  for (int i = start_k_running; i < stop_k_running; i++) {
    if (xpath_running[i]) {
      log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
      if (rc != SR_ERR_OK) {
        log_error("sr_set_item_str failed");
        goto failure;
      }
    }
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) {
    log_error("sr_apply_changes failed");
    goto failure;
  }

  rc = netconf_data_register_callbacks();
  if (rc != 0) {
    log_error("netconf_data_register_callbacks");
    goto failure;
  }

  return 0;

failure:
  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  return 1;
}

int netconf_data_update_bwp_ul(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (oai == 0) {
    log_error("oai is null");
    goto failure;
  }

  if (BWP_UPLINK_XPATH == 0) {
    log_error("BWP_UPLINK_XPATH is null");
    goto failure;
  }

  /* find k_running */
  int k_running = 0;
  while (k_running < MAX_XPATH_ENTRIES && xpath_running[k_running] != BWP_UPLINK_XPATH) k_running++;
  if (k_running >= MAX_XPATH_ENTRIES) {
    log_error("BWP_UPLINK_XPATH not found among running xpaths");
    goto failure;
  }

  int start_k_running = k_running;
  int stop_k_running = k_running;
  while (xpath_running[stop_k_running] &&
         strstr(xpath_running[stop_k_running], BWP_UPLINK_XPATH) == xpath_running[stop_k_running]) {
    stop_k_running++;
  }

  rc = sr_delete_item(netconf_session_running, BWP_UPLINK_XPATH, SR_EDIT_STRICT);
  if (rc != SR_ERR_OK) { log_error("sr_delete_item failure"); goto failure; }
  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  for (int i = start_k_running; i < stop_k_running; i++) {
    free(values_running[i]); free(xpath_running[i]); values_running[i] = NULL; xpath_running[i] = NULL;
  }

  ASPF_OR_FAIL(xpath_running[k_running], "%s/_3gpp-nr-nrm-bwp:BWP[id='Uplink']", GNBDU_FUNCTION_XPATH);
  BWP_UPLINK_XPATH = xpath_running[k_running++];
  /* compact attribute population */
  const char *attrs[] = {
    "priorityLabel", "subCarrierSpacing", "bwpContext",
    "isInitialBwp", "cyclicPrefix", "startRB", "numberOfRBs"
  };
  for (size_t ai = 0; ai < sizeof(attrs)/sizeof(attrs[0]); ++ai) {
    switch (ai) {
      case 0: ASPF_OR_FAIL(values_running[k_running], "%s", "1"); break;
      case 1: ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[1].subCarrierSpacing); break;
      case 2: ASPF_OR_FAIL(values_running[k_running], "%s", "UL"); break;
      case 3: ASPF_OR_FAIL(values_running[k_running], "%s", (oai->bwp[1].isInitialBwp) ? "INITIAL" : "OTHER"); break;
      case 4: ASPF_OR_FAIL(values_running[k_running], "%s", "NORMAL"); break;
      case 5: ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[1].startRB); break;
      case 6: ASPF_OR_FAIL(values_running[k_running], "%d", oai->bwp[1].numberOfRBs); break;
      default: ASPF_OR_FAIL(values_running[k_running], "%s", ""); break;
    }
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", BWP_UPLINK_XPATH, attrs[ai]);
    k_running++;
  }

  for (int i = start_k_running; i < stop_k_running; i++) {
    if (xpath_running[i]) {
      log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
      if (rc != SR_ERR_OK) { log_error("sr_set_item_str failed"); goto failure; }
    }
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  rc = netconf_data_register_callbacks();
  if (rc != 0) { log_error("netconf_data_register_callbacks"); goto failure; }

  return 0;

failure:
  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  return 1;
}

int netconf_data_update_nrcelldu(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (oai == 0) {
    log_error("oai is null");
    goto failure;
  }

  if (NRCELLDU_XPATH == 0) {
    log_error("NRCELLDU_XPATH is null");
    goto failure;
  }

  /* find k */
  int k_running = 0;
  while (k_running < MAX_XPATH_ENTRIES && xpath_running[k_running] != NRCELLDU_XPATH) k_running++;
  if (k_running >= MAX_XPATH_ENTRIES) {
    log_error("NRCELLDU_XPATH not found among running xpaths");
    goto failure;
  }

  int start_k_running = k_running;
  int stop_k_running = k_running;
  while (xpath_running[stop_k_running] &&
         strstr(xpath_running[stop_k_running], NRCELLDU_XPATH) == xpath_running[stop_k_running]) {
    stop_k_running++;
  }

  rc = sr_delete_item(netconf_session_running, NRCELLDU_XPATH, SR_EDIT_STRICT);
  if (rc != SR_ERR_OK) { log_error("sr_delete_item failure"); goto failure; }
  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  /* free old entries in range */
  FREE_RANGE(start_k_running, stop_k_running);

  /* rebuild NRCELLDU entry and attributes (compact) */
  ASPF_OR_FAIL(xpath_running[k_running],
               "%s/_3gpp-nr-nrm-nrcelldu:NRCellDU[id='ManagedElement=%s,GNBDUFunction=%d,NRCellDu=0']",
               GNBDU_FUNCTION_XPATH,
               netconf_config->info.node_id,
               netconf_config->info.gnb_du_id);
  NRCELLDU_XPATH = xpath_running[k_running++];

  /* priorityLabel */
  STRDUP_OR_FAIL(values_running[k_running], "1");
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/attributes/priorityLabel", NRCELLDU_XPATH);

  /* cellLocalId */
  ASPF_OR_FAIL(values_running[k_running], "%d", netconf_config->info.cell_local_id);
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/attributes/cellLocalId", NRCELLDU_XPATH);

  /* pLMNInfoList (sd formatted as hex with colons) */
  char sdHex[9];
  sprintf(sdHex, "%06x", oai->nrcelldu.sd);
  sdHex[8] = 0;
  sdHex[7] = sdHex[5];
  sdHex[6] = sdHex[4];
  sdHex[5] = ':';
  sdHex[4] = sdHex[3];
  sdHex[3] = sdHex[2];
  sdHex[2] = ':';

  ASPF_OR_FAIL(xpath_running[k_running],
               "%s/attributes/pLMNInfoList[mcc='%s'][mnc='%s'][sd='%s'][sst='%d']",
               NRCELLDU_XPATH,
               oai->nrcelldu.mcc,
               oai->nrcelldu.mnc,
               sdHex,
               oai->nrcelldu.sst);
  k_running++;

  /* nPNIdentityList + plmnid */
  ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/nPNIdentityList[idx='%d']", NRCELLDU_XPATH, 0);
  NPNIDENTITYLIST_XPATH = xpath_running[k_running++];
  ASPF_OR_FAIL(xpath_running[k_running], "%s/plmnid[mcc='%s'][mnc='%s']", NPNIDENTITYLIST_XPATH, oai->nrcelldu.mcc, oai->nrcelldu.mnc);
  k_running++;

  /* cAGIdList and nIDList */
  STRDUP_OR_FAIL(values_running[k_running], "1");
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/cAGIdList", NPNIDENTITYLIST_XPATH);

  STRDUP_OR_FAIL(values_running[k_running], "1");
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/nIDList", NPNIDENTITYLIST_XPATH);

  /* compact list of subsequent attributes and their values */
  const char *attrs[] = {
    "nRPCI", "arfcnDL", "arfcnUL", "bSChannelBwDL", "bSChannelBwUL",
    "rimRSMonitoringStartTime", "rimRSMonitoringStopTime",
    "rimRSMonitoringWindowDuration", "rimRSMonitoringWindowStartingOffset",
    "rimRSMonitoringWindowPeriodicity", "rimRSMonitoringOccasionInterval",
    "rimRSMonitoringOccasionStartingOffset", "ssbFrequency", "ssbPeriodicity",
    "ssbSubCarrierSpacing", "ssbOffset", "ssbDuration",
    "nRSectorCarrierRef", "victimSetRef", "aggressorSetRef"
  };
  int n_attrs = sizeof(attrs) / sizeof(attrs[0]);

  for (int ai = 0; ai < n_attrs; ++ai) {
    switch (ai) {
      case 0:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.nRPCI); break;
      case 1:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.arfcnDL); break;
      case 2:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.arfcnUL); break;
      case 3:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.bSChannelBwDL); break;
      case 4:  ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.bSChannelBwUL); break;
      case 5:  ASPF_OR_FAIL(values_running[k_running], "%s", "2023-06-06T00:00:00Z"); break;
      case 6:  ASPF_OR_FAIL(values_running[k_running], "%s", "2023-06-06T00:00:00Z"); break;
      case 7:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 8:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 9:  ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 10: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 11: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 12: ASPF_OR_FAIL(values_running[k_running], "%d", oai->nrcelldu.ssbFrequency); break;
      case 13: ASPF_OR_FAIL(values_running[k_running], "%d", 5); break;
      case 14: ASPF_OR_FAIL(values_running[k_running], "%d", 15); break;
      case 15: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 16: ASPF_OR_FAIL(values_running[k_running], "%d", 1); break;
      case 17: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
      case 18: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
      case 19: ASPF_OR_FAIL(values_running[k_running], "%s", "Tuf=Jy,H:u=|"); break;
      default: ASPF_OR_FAIL(values_running[k_running], "%s", ""); break;
    }
    ASPF_OR_FAIL(xpath_running[k_running], "%s/attributes/%s", NRCELLDU_XPATH, attrs[ai]);
    k_running++;
  }

  /* populate running datastore (keep same loop bounds as original) */
  for (int i = start_k_running; i < stop_k_running; i++) {
    if (xpath_running[i]) {
      log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
      if (rc != SR_ERR_OK) {
        log_error("sr_set_item_str failed");
        goto failure;
      }
    }
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) {
    log_error("sr_apply_changes failed");
    goto failure;
  }

  rc = netconf_data_register_callbacks();
  if (rc != 0) {
    log_error("netconf_data_register_callbacks");
    goto failure;
  }

  return 0;

failure:
  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  return 1;
}

int netconf_data_update_nrcellcu(const oai_data_t *oai)
{
  int rc = 0;

  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (oai == 0) {
    log_error("oai is null");
    goto failure;
  }

  if (NRCELLCU_XPATH == 0) {
    log_error("NRCELLCU_XPATH is null");
    goto failure;
  }

  // find k
  int k_running = 0;
  while (k_running < MAX_XPATH_ENTRIES) {
    if (xpath_running[k_running] == NRCELLCU_XPATH) {
      break;
    }
    k_running++;
  }

  if (k_running >= MAX_XPATH_ENTRIES) {
    log_error("NRCELLDU_XPATH not found among running xpaths");
    goto failure;
  }

  int start_k_running = k_running;
  int stop_k_running = k_running;
  while ((xpath_running[stop_k_running])
         && (strstr(xpath_running[stop_k_running], NRCELLCU_XPATH) == xpath_running[stop_k_running])) {
    stop_k_running++;
  }

  rc = sr_delete_item(netconf_session_running, NRCELLCU_XPATH, SR_EDIT_STRICT);
  if (rc != SR_ERR_OK) {
    log_error("sr_delete_item failure");
    goto failure;
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) {
    log_error("sr_apply_changes failed");
    goto failure;
  }

  for (int i = start_k_running; i < stop_k_running; i++) {
    free(values_running[i]);
    free(xpath_running[i]);
    values_running[i] = 0;
    xpath_running[i] = 0;
  }

  asprintf(&xpath_running[k_running],
           "%s/_3gpp-nr-nrm-nrcellcu:NRCellCU[id='ManagedElement=%s,GNBCUCPFunction=%d,NRCellCu=0']",
           GNBCUCP_FUNCTION_XPATH,
           netconf_config->info.node_id,
           netconf_config->info.gnb_cu_id);
  if (xpath_running[k_running] == 0) {
    log_error("asprintf failed");
    goto failure;
  }
  NRCELLCU_XPATH = xpath_running[k_running];
  k_running++;

  values_running[k_running] = strdup("1");
  if (values_running[k_running] == 0) {
    log_error("strdup failed");
    goto failure;
  }
  asprintf(&xpath_running[k_running], "%s/attributes/priorityLabel", NRCELLCU_XPATH);
  if (xpath_running[k_running] == 0) {
    log_error("asprintf failed");
    goto failure;
  }
  k_running++;

  asprintf(&values_running[k_running], "%d", netconf_config->info.cell_local_id);
  if (values_running[k_running] == 0) {
    log_error("asprintf failed");
    goto failure;
  }
  asprintf(&xpath_running[k_running], "%s/attributes/cellLocalId", NRCELLCU_XPATH);
  if (xpath_running[k_running] == 0) {
    log_error("asprintf failed");
    goto failure;
  }
  k_running++;

  for (int i = start_k_running; i < stop_k_running; i++) {
    if (xpath_running[i]) {
      log("[runn] populating %s with %s.. ", xpath_running[i], values_running[i]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[i], values_running[i], 0, 0);
      if (rc != SR_ERR_OK) {
        log_error("sr_set_item_str failed");
        goto failure;
      }
    }
  }

  rc = sr_apply_changes(netconf_session_running, 0);
  if (rc != SR_ERR_OK) {
    log_error("sr_apply_changes failed");
    goto failure;
  }

  rc = netconf_data_register_callbacks();
  if (rc != 0) {
    log_error("netconf_data_register_callbacks");
    goto failure;
  }

  return 0;

failure:
  rc = netconf_data_unregister_callbacks();
  if (rc != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  return 1;
}

int find_index(char **arr, const char *val)
{
    for (int idx = 0; idx < MAX_XPATH_ENTRIES; ++idx) {
        if (arr[idx] && strcmp(arr[idx], val) == 0)
            return idx;
    }
    return -1;  // or return -1 if you prefer “not found”
}

int netconf_data_update_alarm(const alarm_t *alarm, int notification_id)
{
  int rc = 0;
  char *now = get_netconf_timestamp();
  if (now == 0) { log_error("get_netconf_timestamp() error"); goto failure; }

  if ((rc = netconf_data_unregister_callbacks()) != 0) {
    log_error("netconf_data_unregister_callbacks");
    goto failure;
  }

  if (alarm == 0) { log_error("alarm is null"); goto failure; }

  /* find alarm index */
  alarm_t **alarms = (alarm_t **)netconf_alarms;
  int alarm_idx = 0;
  while (*alarms && strcmp(alarm->alarm, (*alarms)->alarm) != 0) {
    alarms++; alarm_idx++;
  }
  if (*alarms == 0) { log_error("alarm not found in list"); goto failure; }
  if (ALARM_XPATH[alarm_idx] == 0) { log_error("ALARM_XPATH not found"); goto failure; }

  /* running: remove existing alarm records range */
  int k_running = find_index(xpath_running, (char *)ALARM_XPATH[alarm_idx]);
  if (k_running >= MAX_XPATH_ENTRIES) { log_error("ALARM_XPATH[i] not found among running xpaths"); goto failure; }
  int start_k_running = k_running;
  int stop_k_running = k_running;
  while (xpath_running[stop_k_running] &&
         strstr(xpath_running[stop_k_running], ALARM_XPATH[alarm_idx]) == xpath_running[stop_k_running]) stop_k_running++;

  if ((rc = sr_delete_item(netconf_session_running, ALARM_XPATH[alarm_idx], SR_EDIT_STRICT)) != SR_ERR_OK) { log_error("sr_delete_item failure"); goto failure; }
  if ((rc = sr_apply_changes(netconf_session_running, 0)) != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  FREE_RANGE(start_k_running, stop_k_running);

  /* rebuild running alarm record entries */
  ASPF_OR_FAIL(xpath_running[k_running],
               "%s/attributes/alarmRecords[alarmId='%s-%s']",
               ALARMLIST_XPATH,
               alarm->object_instance,
               alarm->alarm);
  ALARM_XPATH[alarm_idx] = xpath_running[k_running++];
  alarm_severity_t severity = (alarm->state != ALARM_STATE_CLEARED) ? alarm->severity : ALARM_SEVERITY_CLEARED;
  STRDUP_OR_FAIL(values_running[k_running], alarm_severity_to_str(severity));
  ASPF_OR_FAIL(xpath_running[k_running++], "%s/perceivedSeverity", ALARM_XPATH[alarm_idx]);
  stop_k_running = k_running;

  /* operational: remove existing alarm records range */
  int k_operational = find_index(xpath_operational, (char *)ALARM_XPATH_OPER[alarm_idx]);
  if (k_operational >= MAX_XPATH_ENTRIES) { log_error("ALARM_XPATH_OPER[i] not found among operational xpaths"); goto failure; }
  int start_k_operational = k_operational;
  int stop_k_operational = k_operational;
  while (xpath_operational[stop_k_operational] &&
         strstr(xpath_operational[stop_k_operational], ALARM_XPATH_OPER[alarm_idx]) == xpath_operational[stop_k_operational]) stop_k_operational++;

  for (int ii = start_k_operational; ii < stop_k_operational; ++ii) {
    free(values_operational[ii]); free(xpath_operational[ii]); values_operational[ii] = NULL; xpath_operational[ii] = NULL;
  }

  /* rebuild operational alarm record entries */
  ASPF_OR_FAIL(xpath_operational[k_operational],
               "%s/attributes/alarmRecords[alarmId='%s-%s']",
               ALARMLIST_XPATH,
               alarm->object_instance,
               alarm->alarm);
  ALARM_XPATH_OPER[alarm_idx] = xpath_operational[k_operational++];
  STRDUP_OR_FAIL(values_operational[k_operational], alarm->object_instance);
  ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/objectInstance", ALARM_XPATH_OPER[alarm_idx]);

  ASPF_OR_FAIL(values_operational[k_operational], "%d", notification_id);
  ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/notificationId", ALARM_XPATH_OPER[alarm_idx]);

  STRDUP_OR_FAIL(values_operational[k_operational], alarm_type_to_str(alarm->type));
  ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/alarmType", ALARM_XPATH_OPER[alarm_idx]);

  STRDUP_OR_FAIL(values_operational[k_operational], "unset");
  ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/probableCause", ALARM_XPATH_OPER[alarm_idx]);

  /* alarmChangedTime always set to now */
  STRDUP_OR_FAIL(values_operational[k_operational], now);
  ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/alarmChangedTime", ALARM_XPATH_OPER[alarm_idx]);

  if (alarm->state == ALARM_STATE_CLEARED) {
    /* alarmRaisedTime left unset, alarmClearedTime set */
    k_operational++; /* alarmRaisedTime placeholder */
    STRDUP_OR_FAIL(values_operational[k_operational], now);
    ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/alarmClearedTime", ALARM_XPATH_OPER[alarm_idx]);
  } else {
    /* alarmRaisedTime set, alarmClearedTime left unset */
    STRDUP_OR_FAIL(values_operational[k_operational], now);
    ASPF_OR_FAIL(xpath_operational[k_operational++], "%s/alarmRaisedTime", ALARM_XPATH_OPER[alarm_idx]);
    k_operational++; /* alarmClearedTime placeholder */
  }
  stop_k_operational = k_operational;

  /* populate running datastore */
  for (int ii = start_k_running; ii < stop_k_running; ++ii) {
    if (xpath_running[ii]) {
      log("[runn] populating %s with %s.. ", xpath_running[ii], values_running[ii]);
      rc = sr_set_item_str(netconf_session_running, xpath_running[ii], values_running[ii], 0, 0);
      if (rc != SR_ERR_OK) { log_error("sr_set_item_str failed"); goto failure; }
    }
  }
  if ((rc = sr_apply_changes(netconf_session_running, 0)) != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  /* populate operational datastore */
  for (int ii = start_k_operational; ii < stop_k_operational; ++ii) {
    if (xpath_operational[ii]) {
      log("[oper] populating %s with %s.. ", xpath_operational[ii], values_operational[ii]);
      rc = sr_set_item_str(netconf_session_operational, xpath_operational[ii], values_operational[ii], 0, 0);
      if (rc != SR_ERR_OK) { log_error("sr_set_item_str failed"); goto failure; }
    }
  }
  if ((rc = sr_apply_changes(netconf_session_operational, 0)) != SR_ERR_OK) { log_error("sr_apply_changes failed"); goto failure; }

  if ((rc = netconf_data_register_callbacks()) != 0) { log_error("netconf_data_register_callbacks"); goto failure; }

  free(now);
  return 0;

failure:
  netconf_data_unregister_callbacks();
  free(now);
  return 1;
}

static int netconf_ru_data_register_callbacks()
{
  if (netconf_ru_data_subscription) {
    log_error("already subscribed") goto failed;
  }

  int rc = sr_module_change_subscribe(netconf_session_running,
                                      "o-ran-uplane-conf",
                                      NULL,
                                      netconf_ru_data_edit_callback,
                                      NULL,
                                      0,
                                      0,
                                      &netconf_ru_data_subscription);
  if (rc != SR_ERR_OK) {
    log_error("sr_module_change_subscribe() failed");
    goto failed;
  }

  return 0;
failed:
  sr_unsubscribe(netconf_data_subscription);

  return 1;
}

static int netconf_data_register_callbacks()
{
  if (MANAGED_ELEMENT_XPATH == 0) {
    log_error("MANAGED_ELEMENT_XPATH is null") goto failed;
  }

  if (netconf_data_subscription) {
    log_error("already subscribed") goto failed;
  }

  int rc = sr_module_change_subscribe(netconf_session_running,
                                      "_3gpp-common-managed-element",
                                      MANAGED_ELEMENT_XPATH,
                                      netconf_data_edit_callback,
                                      NULL,
                                      0,
                                      0,
                                      &netconf_data_subscription);
  if (rc != SR_ERR_OK) {
    log_error("sr_module_change_subscribe() failed");
    goto failed;
  }

  return 0;
failed:
  sr_unsubscribe(netconf_data_subscription);

  return 1;
}

static int netconf_data_unregister_callbacks()
{
  if (netconf_data_subscription) {
    sr_unsubscribe(netconf_data_subscription);
    netconf_data_subscription = 0;
  }

  return 0;
}

static int netconf_data_edit_callback(sr_session_ctx_t *session,
                                      uint32_t sub_id,
                                      const char *module_name,
                                      const char *xpath_running,
                                      sr_event_t event,
                                      uint32_t request_id,
                                      void *private_data)
{
  (void)sub_id;
  (void)request_id;
  (void)private_data;

  int rc = SR_ERR_OK;
  sr_change_iter_t *it = NULL;
  sr_change_oper_t oper;
  sr_val_t *old_value = NULL, *new_value = NULL;
  char *change_path = NULL;

  if (xpath_running) asprintf(&change_path, "%s//.", xpath_running);
  else asprintf(&change_path, "/%s:*//.", module_name);

  if (event == SR_EV_CHANGE) {
    rc = sr_get_changes_iter(session, change_path, &it);
    if (rc != SR_ERR_OK) { log_error("sr_get_changes_iter() failed"); goto failed; }

    int invalidEdit = 0;
    char *invalidEditReason = NULL;

    /* variables to collect edits */
    int bSChannelBwDL = -1, bSChannelBwUL = -1;
    uint32_t pMax = UINT32_MAX;
    int gNBId = -1;
    const char *cu_name = NULL;
    const char *dl_ul = NULL;
    long dl_ul_trans_period = 6;
    uint16_t nr_of_downlink_slots = UINT16_MAX;
    uint8_t nr_of_downlink_symbols = UINT8_MAX;
    uint16_t nr_of_uplink_slots = UINT16_MAX;
    uint16_t nr_of_uplink_symbols = UINT16_MAX;

    /* helper macro to shorten assignments */
    #define ASSIGN_IF(sub, dest, member) \
      else if (strstr(new_value->xpath, (sub))) (dest) = new_value->data.member

    while ((rc = sr_get_change_next(session, it, &oper, &old_value, &new_value)) == SR_ERR_OK) {
      if (oper != SR_OP_MODIFIED) {
        invalidEdit = 1; invalidEditReason = strdup("invalid operation (only MODIFY enabled)"); goto checkInvalidEdit;
      }

      if (0) {}
      ASSIGN_IF("nr-of-downlink-slots", nr_of_downlink_slots, uint16_val);
      ASSIGN_IF("nr-of-uplink-slots", nr_of_uplink_slots, uint16_val);
      ASSIGN_IF("nr-of-downlink-symbols", nr_of_downlink_symbols, uint8_val);
      ASSIGN_IF("nr-of-uplink-symbols", nr_of_uplink_symbols, uint16_val);
      ASSIGN_IF("dl-ul-transmission-periodicity", dl_ul, string_val);
      ASSIGN_IF("bSChannelBwDL", bSChannelBwDL, uint16_val);
      ASSIGN_IF("bSChannelBwUL", bSChannelBwUL, uint16_val);
      ASSIGN_IF("pMax", pMax, uint32_val);
      ASSIGN_IF("gNBId", gNBId, uint32_val);
      ASSIGN_IF("gNBCUName", cu_name, string_val);
      else { invalidEdit = 1; invalidEditReason = strdup(new_value->xpath); }

      sr_free_val(old_value); old_value = NULL;
      sr_free_val(new_value); new_value = NULL;

    checkInvalidEdit:
      if (invalidEdit) break;
    }
    #undef ASSIGN_IF

    if (invalidEdit) {
      log_error("invalid edit data detected: %s", invalidEditReason);
      free(invalidEditReason);
      goto failed_validation;
    }

    /* map dl_ul string to numeric code if provided */
    if (dl_ul) {
      struct { const char *s; long v; } map[] = {
        { "ms0p5", 0 }, { "ms0p625", 1 }, { "ms1", 2 }, { "ms1p25", 3 },
        { "ms2", 4 }, { "ms2p5", 5 }, { "ms5", 6 }, { "ms10", 7 }
      };
      dl_ul_trans_period = 6; /* default */
      for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); ++i) {
        if (strcmp(dl_ul, map[i].s) == 0) { dl_ul_trans_period = map[i].v; break; }
      }
    }

    /* apply edits (preserve original validation semantics) */
    if (bSChannelBwDL != -1 || bSChannelBwUL != -1) {
      if (bSChannelBwDL != bSChannelBwUL) {
        log_error("bSChannelBwDL (%d) != bSChannelBwUL (%d)", bSChannelBwDL, bSChannelBwUL);
        goto failed_validation;
      }
      if (telnet_change_bandwidth(bSChannelBwDL) != 0) { log_error("telnet_change_bandwidth failed"); goto failed_validation; }
    } else if (gNBId != -1) {
      if (telnet_change_gNBId(gNBId) != 0) { log_error("telnet_change_gNBId failed"); goto failed_validation; }
    } else if (nr_of_downlink_slots != UINT16_MAX && nr_of_uplink_slots != UINT16_MAX) {
      if (telnet_change_tdd_slot_config(dl_ul_trans_period, nr_of_downlink_slots, nr_of_uplink_slots) != 0) {
        log_error("telnet_change_tdd_slot_config failed"); goto failed_validation;
      }
    } else if (nr_of_downlink_symbols != UINT8_MAX || nr_of_uplink_symbols != UINT16_MAX) {
      if (telnet_change_tdd_symbol_config(nr_of_downlink_symbols, nr_of_uplink_symbols) != 0) {
        log_error("telnet_change_tdd_symbols_config failed"); goto failed_validation;
      }
    } else {
      log_error("unknown edit request");
      goto failed_validation;
    }

    sr_free_change_iter(it); it = NULL;
  }

  free(change_path);
  if (it) sr_free_change_iter(it);
  if (old_value) sr_free_val(old_value);
  if (new_value) sr_free_val(new_value);
  return SR_ERR_OK;

failed:
  free(change_path);
  if (it) sr_free_change_iter(it);
  if (old_value) sr_free_val(old_value);
  if (new_value) sr_free_val(new_value);
  return SR_ERR_INTERNAL;

failed_validation:
  free(change_path);
  if (it) sr_free_change_iter(it);
  if (old_value) sr_free_val(old_value);
  if (new_value) sr_free_val(new_value);
  return SR_ERR_VALIDATION_FAILED;
}

static int netconf_ru_data_edit_callback(sr_session_ctx_t *session,
                                         uint32_t sub_id,
                                         const char *module_name,
                                         const char *xpath_running,
                                         sr_event_t event,
                                         uint32_t request_id,
                                         void *private_data)
{
  (void)sub_id; (void)request_id; (void)private_data;

  int ret = SR_ERR_OK, rc = SR_ERR_OK;
  sr_change_iter_t *it = NULL;
  sr_change_oper_t oper;
  sr_val_t *old_value = NULL, *new_value = NULL;
  char *change_path = NULL;
  int tx_active = -1, rx_active = -1; /* -1 = unset */

  if (xpath_running) {
    if (asprintf(&change_path, "%s//.", xpath_running) == -1) change_path = NULL;
  } else {
    if (asprintf(&change_path, "/%s:*//.", module_name) == -1) change_path = NULL;
  }

  if (event == SR_EV_CHANGE) {
    rc = sr_get_changes_iter(session, change_path, &it);
    if (rc != SR_ERR_OK) { log_error("sr_get_changes_iter() failed"); ret = SR_ERR_INTERNAL; goto cleanup; }

    int invalid = 0;
    char *invalidReason = NULL;
    while ((rc = sr_get_change_next(session, it, &oper, &old_value, &new_value)) == SR_ERR_OK) {
      if (oper != SR_OP_MODIFIED) { invalid = 1; invalidReason = strdup("invalid operation (only MODIFY enabled)"); }
      else if (strstr(new_value->xpath, "/o-ran-uplane-conf:user-plane-configuration/tx-array-carriers[name='txarraycarrier0']/active"))
        tx_active = new_value->data.binary_val;
      else if (strstr(new_value->xpath, "/o-ran-uplane-conf:user-plane-configuration/rx-array-carriers[name='rxarraycarrier0']/active"))
        rx_active = new_value->data.binary_val;
      else { invalid = 1; invalidReason = strdup(new_value->xpath); }

      sr_free_val(old_value); old_value = NULL;
      sr_free_val(new_value); new_value = NULL;
      if (invalid) break;
    }

    if (invalid) {
      log_error("invalid edit data detected: %s", invalidReason ? invalidReason : "(unknown)");
      free(invalidReason);
      ret = SR_ERR_VALIDATION_FAILED;
      goto cleanup;
    }

    if (tx_active != -1 || rx_active != -1) {
      if (tx_active != rx_active) {
        log_error("tx_array_carrier_active (%d) != rx_array_carrier_active (%d)", tx_active, rx_active);
        ret = SR_ERR_VALIDATION_FAILED;
        goto cleanup;
      }
      if (telnet_edit_carrier_ru(tx_active) != 0) {
        log_error("telnet_edit_carrier_ru failed");
        ret = SR_ERR_VALIDATION_FAILED;
        goto cleanup;
      }
    }

    sr_free_change_iter(it); it = NULL;
  }

cleanup:
  free(change_path);
  if (it) sr_free_change_iter(it);
  if (old_value) sr_free_val(old_value);
  if (new_value) sr_free_val(new_value);
  return ret;
}
