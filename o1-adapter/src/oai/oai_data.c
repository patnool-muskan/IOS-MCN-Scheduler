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

#include "oai_data.h"
#include "common/log.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern char *nodeType;

oai_data_t *oai_data_parse_json(const char *json)
{
  cJSON *cjson = 0;

  oai_data_t *data = (oai_data_t *)malloc(sizeof(oai_data_t));
  if (data == 0) {
    log_error("oai_data_t malloc failed");
    goto failed;
  }

  memset(data, 0, sizeof(oai_data_t));

  cJSON *object;

  cjson = cJSON_Parse(json);
  if (cjson == 0) {
    log_error("cJSON parse failed");
    goto failed;
  }

  cJSON *o1 = cJSON_GetObjectItem(cjson, "o1-config");
  if (o1 == 0) {
    log_error("not found: o1-config");
    goto failed;
  }

  if (strcmp(nodeType, "cu-up") == 0) {
    cJSON *gnbcuup = PJSON_GET_OBJ(o1, "GNBCUUP");

    PJSON_PARSE_INT(gnbcuup, "nrgnbcuup3gpp:gNBId", data->gnbcuup.gNBId);
    PJSON_PARSE_INT(gnbcuup, "nrgnbcuup3gpp:cu_up_id", data->gnbcuup.gNBCUUPId);
    PJSON_PARSE_STRDUP(gnbcuup, "nrgnbcuup3gpp:mcc", data->gnbcuup.mcc);
    PJSON_PARSE_STRDUP(gnbcuup, "nrgnbcuup3gpp:mnc", data->gnbcuup.mnc);
    PJSON_PARSE_INT(gnbcuup, "nrgnbcuup3gpp:mnc_length", data->gnbcuup.mnc_length);
    PJSON_PARSE_STRDUP(gnbcuup, "vendor", data->device_data.vendor);
  }

  else {
    if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
      cJSON *nrfreqrel = PJSON_GET_OBJ(o1, "NRFREQREL");
      PJSON_PARSE_INT(nrfreqrel, "nrfreqrel3gpp:pMax", data->nrfreqrel.pMax);

      cJSON *nrcellcu = PJSON_GET_OBJ(o1, "NRCELLCU");
      PJSON_PARSE_INT(nrcellcu, "nrcellcu3gpp:cellLocalId", data->nrcellcu.cellLocalId);
      PJSON_PARSE_STRDUP(nrcellcu, "nrcellcu3gpp:mcc", data->nrcellcu.mcc);
      PJSON_PARSE_STRDUP(nrcellcu, "nrcellcu3gpp:mnc", data->nrcellcu.mnc);
      PJSON_PARSE_INT(nrcellcu, "nrcellcu3gpp:sst", data->nrcellcu.sst);
      PJSON_PARSE_INT(nrcellcu, "nrcellcu3gpp:sd", data->nrcellcu.sd);
    }

    if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
      cJSON *bwp = PJSON_GET_OBJ(o1, "BWP");
      if (bwp == 0) {
        log_error("not found: BWP");
        goto failed;
      }

      {
        cJSON *bwp_dl = PJSON_GET_OBJ(bwp, "dl");
        PJSON_PARSE_ARRAY(bwp, "dl", bwp_dl);

        cJSON *bwpElement = cJSON_GetArrayItem(bwp_dl, 0);

        object = PJSON_GET_OBJ(bwpElement, "bwp3gpp:isInitialBwp");
        if (object == 0) {
          log_error("not found: bwp3gpp:isInitialBwp");
          goto failed;
        }
        if (!cJSON_IsBool(object)) {
          log_error("failed type: bwp3gpp:isInitialBwp");
          goto failed;
        }
        data->bwp[0].isInitialBwp = cJSON_IsTrue(object);

        PJSON_PARSE_INT(bwpElement, "bwp3gpp:numberOfRBs", data->bwp[0].numberOfRBs);
        PJSON_PARSE_INT(bwpElement, "bwp3gpp:startRB", data->bwp[0].startRB);

        object = PJSON_GET_OBJ(bwpElement, "bwp3gpp:subCarrierSpacing");
        if (object == 0) {
          log_error("not found: bwp3gpp:subCarrierSpacing");
          goto failed;
        }
        if (!cJSON_IsNumber(object)) {
          log_error("failed type: bwp3gpp:subCarrierSpacing");
          goto failed;
        }
        switch (object->valueint) {
          case 0:
            data->bwp[0].subCarrierSpacing = 15;
            break;
          case 1:
            data->bwp[0].subCarrierSpacing = 30;
            break;
          case 2:
            data->bwp[0].subCarrierSpacing = 60;
            break;
          case 3:
            data->bwp[0].subCarrierSpacing = 120;
            break;
          default:
            data->bwp[0].subCarrierSpacing = object->valueint;
        }
      }

      {
        cJSON *bwp_ul = PJSON_GET_OBJ(bwp, "ul");
        PJSON_PARSE_ARRAY(bwp, "ul", bwp_ul);

        cJSON *bwpElement = cJSON_GetArrayItem(bwp_ul, 0);

        object = PJSON_GET_OBJ(bwpElement, "bwp3gpp:isInitialBwp");
        if (object == 0) {
          log_error("not found: bwp3gpp:isInitialBwp");
          goto failed;
        }
        if (!cJSON_IsBool(object)) {
          log_error("failed type: bwp3gpp:isInitialBwp");
          goto failed;
        }
        data->bwp[1].isInitialBwp = cJSON_IsTrue(object);

        PJSON_PARSE_INT(bwpElement, "bwp3gpp:numberOfRBs", data->bwp[1].numberOfRBs);
        PJSON_PARSE_INT(bwpElement, "bwp3gpp:startRB", data->bwp[1].startRB);

        object = PJSON_GET_OBJ(bwpElement, "bwp3gpp:subCarrierSpacing");
        if (object == 0) {
          log_error("not found: bwp3gpp:subCarrierSpacing");
          goto failed;
        }
        if (!cJSON_IsNumber(object)) {
          log_error("failed type: bwp3gpp:subCarrierSpacing");
          goto failed;
        }
        switch (object->valueint) {
          case 0:
            data->bwp[1].subCarrierSpacing = 15;
            break;
          case 1:
            data->bwp[1].subCarrierSpacing = 30;
            break;
          case 2:
            data->bwp[1].subCarrierSpacing = 60;
            break;
          case 3:
            data->bwp[1].subCarrierSpacing = 120;
            break;
          default:
            data->bwp[1].subCarrierSpacing = object->valueint;
        }
      }

      cJSON *nrcelldu = PJSON_GET_OBJ(o1, "NRCELLDU");
      if (nrcelldu == 0) {
        log_error("not found: NRCELLDU");
        goto failed;
      }

      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:ssbFrequency", data->nrcelldu.ssbFrequency);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:arfcnDL", data->nrcelldu.arfcnDL);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:bSChannelBwDL", data->nrcelldu.bSChannelBwDL);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:arfcnUL", data->nrcelldu.arfcnUL);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:bSChannelBwUL", data->nrcelldu.bSChannelBwUL);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:nRPCI", data->nrcelldu.nRPCI);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:nRTAC", data->nrcelldu.nRTAC);
      PJSON_PARSE_STRDUP(nrcelldu, "nrcelldu3gpp:mcc", data->nrcelldu.mcc);
      PJSON_PARSE_STRDUP(nrcelldu, "nrcelldu3gpp:mnc", data->nrcelldu.mnc);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:sd", data->nrcelldu.sd);
      PJSON_PARSE_INT(nrcelldu, "nrcelldu3gpp:sst", data->nrcelldu.sst);
      PJSON_PARSE_INT(nrcelldu, "DL_UL_TransmissionPeriodicity", data->nrcelldu.TP);
      PJSON_PARSE_INT(nrcelldu, "nrofDownlinkSlots", data->nrcelldu.nrofDownlinkSlots);
      PJSON_PARSE_INT(nrcelldu, "nrofDownlinkSymbols", data->nrcelldu.nr_of_downlink_symbols);
      PJSON_PARSE_INT(nrcelldu, "nrofUplinkSlots", data->nrcelldu.nrofUplinkSlots);
      PJSON_PARSE_INT(nrcelldu, "nrofUplinkSymbols", data->nrcelldu.nr_of_uplink_symbols);
    }

    cJSON *device = PJSON_GET_OBJ(o1, "device");
    if (device == 0) {
      log_error("not found: device");
      goto failed;
    }

    if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
      PJSON_PARSE_INT(device, "gnbId", data->device_data.gnbId);
      PJSON_PARSE_STRDUP(device, "gnbName", data->device_data.gnbName);
    }
    if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
      PJSON_PARSE_INT(device, "gNBId", data->device_data.gnbId);
      PJSON_PARSE_STRDUP(device, "gnbCUName", data->device_data.gnbCUName);
    }
    if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
      PJSON_PARSE_STRDUP(device, "vendor", data->device_data.vendor);
    }

    cJSON *additional = PJSON_GET_OBJ(cjson, "O1-Operational");
    if (additional == 0) {
      log_error("not found: O1-Operational");
      goto failed;
    }
    if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
      PJSON_PARSE_INT(additional, "num_dus", data->additional_data.num_dus);
      PJSON_PARSE_INT(additional, "num_cuups", data->additional_data.num_cuups);
      PJSON_PARSE_STRDUP(additional, "vendor", data->device_data.vendor);
    }

    if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
      PJSON_PARSE_STRDUP(additional, "frame-type", data->additional_data.frameType);
      PJSON_PARSE_INT(additional, "band-number", data->additional_data.bandNumber);
      PJSON_PARSE_ARRAY(additional, "ues", object);
    }

    if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
      data->additional_data.numUes = cJSON_GetArraySize(object);
      data->additional_data.ues = (int *)malloc(data->additional_data.numUes * sizeof(int));
      if (!data->additional_data.ues) {
        log_error("malloc failed: ues");
        goto failed;
      }
      for (int i = 0; i < data->additional_data.numUes; i++) {
        cJSON *ue = cJSON_GetArrayItem(object, i);
        if (ue == 0) {
          log_error("not found: ues[i]");
          goto failed;
        }
        if (!cJSON_IsNumber(ue)) {
          log_error("failed type: ues[i]");
          goto failed;
        }
        data->additional_data.ues[i] = ue->valueint;
      }

      PJSON_PARSE_INT(additional, "load", data->additional_data.load);

      object = PJSON_GET_OBJ(additional, "ues-thp");
      data->additional_data.ues_thp = (oai_ues_thp_t *)malloc(data->additional_data.numUes * sizeof(oai_ues_thp_t));
      if (!data->additional_data.ues_thp) {
        log_error("malloc failed: ues_thp");
        goto failed;
      }
      for (int i = 0; i < data->additional_data.numUes; i++) {
        cJSON *thpUe = cJSON_GetArrayItem(object, i);
        if (thpUe == 0) {
          log_error("not found: thpUe");
          goto failed;
        }

        PJSON_PARSE_INT(thpUe, "rnti", data->additional_data.ues_thp[i].rnti);
        PJSON_PARSE_INT(thpUe, "dl", data->additional_data.ues_thp[i].dl);
        PJSON_PARSE_INT(thpUe, "ul", data->additional_data.ues_thp[i].ul);
        PJSON_PARSE_INT(thpUe, "dl_ul_tbs", data->additional_data.ues_thp[i].dl_ul_tbs);
        PJSON_PARSE_INT(thpUe, "err_dl_tbs", data->additional_data.ues_thp[i].dl_tbs_err);
        PJSON_PARSE_INT(thpUe, "err_ul_tbs", data->additional_data.ues_thp[i].ul_tbs_err);
        PJSON_PARSE_INT(thpUe, "dl_prbs", data->additional_data.ues_thp[i].dl_prbs);
        PJSON_PARSE_INT(thpUe, "ul_prbs", data->additional_data.ues_thp[i].tot_ul_prbs);
        PJSON_PARSE_INT(thpUe, "tot_ul_prbs", data->additional_data.ues_thp[i].ul_prbs);
        PJSON_PARSE_INT(thpUe, "ul_pdcp_sdu", data->additional_data.ues_thp[i].ul_pdcp_sdu);
        PJSON_PARSE_INT(thpUe, "dl_pdcp_sdu", data->additional_data.ues_thp[i].dl_pdcp_sdu);
      }
      if (mplane_enabled) {
        cJSON *oru = PJSON_GET_OBJ(o1, "o-ru-stats");
        if (oru == 0) {
          log_error("not found: o-ru-stats");
          goto failed;
        }
        cJSON *o_ran_uplane_conf = PJSON_GET_OBJ(oru, "o-ran-uplane-conf");

        if (o_ran_uplane_conf == 0) {
          log_error("not found: o-ran-uplane-conf");
          goto failed;
        }

        if (!cJSON_IsArray(o_ran_uplane_conf)) {
          log_error("o-ran-uplane-conf not array");
          goto failed;
        }

        cJSON *o_ran_uplane_conf_element = cJSON_GetArrayItem(o_ran_uplane_conf, 0);

        GET_JSON_INT(o_ran_uplane_conf_element, "tx-array-carrier:absolute-frequency-center", data->oru.tx_array_carrier.arfcn_center);
        GET_JSON_UINT32(o_ran_uplane_conf_element, "tx-array-carrier:center-of-channel-bandwidth", data->oru.tx_array_carrier.center_channel_bw);
        GET_JSON_INT(o_ran_uplane_conf_element, "tx-array-carrier:channel-bandwidth", data->oru.tx_array_carrier.channel_bw);

        {
          const char *tmp;
          GET_JSON_STR(o_ran_uplane_conf_element, "tx-array-carrier:active", tmp);
          if (strcmp(tmp, "INACTIVE") == 0 || strcmp(tmp, "SLEEP") == 0 || strcmp(tmp, "ACTIVE") == 0)
          strcpy(data->oru.tx_array_carrier.ru_carrier, tmp);
          else
          printf("INAVALID RU CARRIER STATE\n");
        }

        {
          const char *tmp;
          GET_JSON_STR(o_ran_uplane_conf_element, "tx-array-carrier:rw-duplex-scheme", tmp);
          if (strcmp(tmp, "TDD") == 0 || strcmp(tmp, "FDD") == 0)
          strcpy(data->oru.tx_array_carrier.rw_duplex_scheme, tmp);
          else
          printf("INAVALID RU CARRIER DUPLEX SCHEME TYPE\n");
        }

        {
          const char *tmp;
          GET_JSON_STR(o_ran_uplane_conf_element, "tx-array-carrier:rw-type", tmp);
          if (strcmp(tmp, "LTE") == 0 || strcmp(tmp, "NR") == 0)
          strcpy(data->oru.tx_array_carrier.rw_type, tmp);
          else
          printf("INVALID RU CARRIER DUPLEX TYPE\n");
        }

        GET_JSON_INT(o_ran_uplane_conf_element, "tx-array-carrier:gain", data->oru.tx_array_carrier.gain);
        GET_JSON_INT(o_ran_uplane_conf_element, "tx-array-carrier:downlink-radio-frame-offset", data->oru.tx_array_carrier.dl_radio_frame_offset);
        GET_JSON_INT(o_ran_uplane_conf_element, "tx-array-carrier:downlink-sfn-offset", data->oru.tx_array_carrier.dl_sfn_offset);

        o_ran_uplane_conf_element = cJSON_GetArrayItem(o_ran_uplane_conf, 1);

        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:absolute-frequency-center", data->oru.rx_array_carrier.arfcn_center);
        GET_JSON_UINT32(o_ran_uplane_conf_element, "rx-array-carrier:center-of-channel-bandwidth", data->oru.rx_array_carrier.center_channel_bw);
        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:channel-bandwidth", data->oru.rx_array_carrier.channel_bw);

        {
          const char *tmp;
          GET_JSON_STR(o_ran_uplane_conf_element, "rx-array-carrier:active", tmp);
          if (strcmp(tmp, "INACTIVE") == 0 || strcmp(tmp, "SLEEP") == 0 || strcmp(tmp, "ACTIVE") == 0)
          strcpy(data->oru.rx_array_carrier.ru_carrier, tmp);
          else
          printf("INAVALID RU CARRIER STATE\n");
        }

        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:downlink-radio-frame-offset", data->oru.rx_array_carrier.dl_radio_frame_offset);
        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:downlink-sfn-offset", data->oru.rx_array_carrier.dl_sfn_offset);
        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:gain-correction", data->oru.rx_array_carrier.gain_correction);
        GET_JSON_INT(o_ran_uplane_conf_element, "rx-array-carrier:n-ta-offset", data->oru.rx_array_carrier.nta_offset);

        // Getting delay-management data
        cJSON *delay_management = cJSON_GetObjectItem(oru, "delay-management");

        if (delay_management == 0) {
          log_error("not found: delay-management");
          goto failed;
        }

        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-min-up",        data->oru.delay_management.T2a_min_up);
        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-max-up",        data->oru.delay_management.T2a_max_up);
        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-min-cp-dl",     data->oru.delay_management.T2a_min_cp_dl);
        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-max-cp-dl",     data->oru.delay_management.T2a_max_cp_dl);
        GET_JSON_INT(delay_management, "ru-delay-profile:tcp-adv-dl",        data->oru.delay_management.Tcp_adv_dl);
        GET_JSON_INT(delay_management, "ru-delay-profile:ta3-min",           data->oru.delay_management.Ta3_min);
        GET_JSON_INT(delay_management, "ru-delay-profile:ta3-max",           data->oru.delay_management.Ta3_max);
        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-min-cp-ul",     data->oru.delay_management.T2a_min_cp_ul);
        GET_JSON_INT(delay_management, "ru-delay-profile:t2a-max-cp-ul",     data->oru.delay_management.T2a_max_cp_ul);

        // Usage
        cJSON *o_ran_performance_counters = cJSON_GetObjectItem(oru, "o-ran-performance-counters");
        if (o_ran_performance_counters == NULL) {
            log_error("not found: o-ran-performance-counters");
            goto failed;
        }

        GET_JSON_INT(o_ran_performance_counters, "performance-counters:total-rx-good-pkt-cnt",    data->oru.o_ran_performance_metrics.total_rx_good_pkt_cnt);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:total-rx-bit-rate",        data->oru.o_ran_performance_metrics.total_rx_bit_rate);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-on-time",          data->oru.o_ran_performance_metrics.oran_rx_on_time);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-early",            data->oru.o_ran_performance_metrics.oran_rx_early);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-late",             data->oru.o_ran_performance_metrics.oran_rx_late);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-corrupt",          data->oru.o_ran_performance_metrics.oran_rx_corrupt);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-total",            data->oru.o_ran_performance_metrics.oran_rx_total);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-total-c",          data->oru.o_ran_performance_metrics.oran_rx_total_c);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-on-time-c",        data->oru.o_ran_performance_metrics.oran_rx_on_time_c);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-early-c",          data->oru.o_ran_performance_metrics.oran_rx_early_c);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-late-c",           data->oru.o_ran_performance_metrics.oran_rx_late_c);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-rx-error-drop",       data->oru.o_ran_performance_metrics.oran_rx_error_drop);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-tx-total",            data->oru.o_ran_performance_metrics.oran_tx_total);
        GET_JSON_INT(o_ran_performance_counters, "performance-counters:oran-tx-total-c",          data->oru.o_ran_performance_metrics.oran_tx_total_c);
#undef GET_JSON_INT
      }
    }
  }
  cJSON_Delete(cjson);
  return data;

failed:
  oai_data_free(data);
  cJSON_Delete(cjson);

  return 0;
}

void oai_data_print(const oai_data_t *data)
{
  log("==========================\n");
  log("BWP[Downlink].isInitialBwp: %d\n", data->bwp[0].isInitialBwp);
  log("BWP[Downlink].numberOfRBs: %d\n", data->bwp[0].numberOfRBs);
  log("BWP[Downlink].startRB: %d\n", data->bwp[0].startRB);
  log("BWP[Downlink].subCarrierSpacing: %d\n", data->bwp[0].subCarrierSpacing);
  log("\n");
  log("BWP[Uplink].isInitialBwp: %d\n", data->bwp[1].isInitialBwp);
  log("BWP[Uplink].numberOfRBs: %d\n", data->bwp[1].numberOfRBs);
  log("BWP[Uplink].startRB: %d\n", data->bwp[1].startRB);
  log("BWP[Uplink].subCarrierSpacing: %d\n", data->bwp[1].subCarrierSpacing);
  log("\n");
  log("NRCELLDU.ssbFrequency: %d\n", data->nrcelldu.ssbFrequency);
  log("NRCELLDU.arfcnDL: %d\n", data->nrcelldu.arfcnDL);
  log("NRCELLDU.bSChannelBwDL: %d\n", data->nrcelldu.bSChannelBwDL);
  log("NRCELLDU.arfcnUL: %d\n", data->nrcelldu.arfcnUL);
  log("NRCELLDU.bSChannelBwUL: %d\n", data->nrcelldu.bSChannelBwUL);
  log("NRCELLDU.nRPCI: %d\n", data->nrcelldu.nRPCI);
  log("NRCELLDU.nRTAC: %d\n", data->nrcelldu.nRTAC);
  log("\n");
  log("NRCELLDU.mcc: %s\n", data->nrcelldu.mcc);
  log("NRCELLDU.mnc: %s\n", data->nrcelldu.mnc);
  log("NRCELLDU.sd: %d\n", data->nrcelldu.sd);
  log("NRCELLDU.sst: %d\n", data->nrcelldu.sst);
  log("\n");
  log("DEVICE.gnbId: %d\n", data->device_data.gnbId);
  log("DEVICE.gnbName: %s\n", data->device_data.gnbName);
  log("DEVICE.vendor: %s\n", data->device_data.vendor);

  log("\n");
  log("ADDITIONAL.frameType: %s\n", data->additional_data.frameType);
  log("ADDITIONAL.bandNumber: %d\n", data->additional_data.bandNumber);
  log("ADDITIONAL.UEs[%d]: \n", data->additional_data.numUes);
  for (int i = 0; i < data->additional_data.numUes; i++) {
    log(" - [%d]: %d\n", i, data->additional_data.ues[i]);
  }
  log("ADDITIONAL.load: %d\n", data->additional_data.load);
  log("ADDITIONAL.UEs-THP[%d]: \n", data->additional_data.numUes);
  for (int i = 0; i < data->additional_data.numUes; i++) {
    log(" - rnti[%d]: %d\n", i, data->additional_data.ues_thp[i].rnti);
    log(" - dl[%d]: %d\n", i, data->additional_data.ues_thp[i].dl);
    log(" - ul[%d]: %d\n", i, data->additional_data.ues_thp[i].ul);
  }

  log("\n");
}

void oai_data_free(oai_data_t *data)
{
  if (strcmp(nodeType, "du") == 0 || strcmp(nodeType, "gNB") == 0) {
    free(data->nrcelldu.mcc);
    data->nrcelldu.mcc = 0;
    free(data->nrcelldu.mnc);
    data->nrcelldu.mnc = 0;

    free(data->device_data.gnbName);
    data->device_data.gnbName = 0;
    free(data->device_data.vendor);
    data->device_data.vendor = 0;

    free(data->additional_data.frameType);
    data->additional_data.frameType = 0;

    free(data->additional_data.ues);
    data->additional_data.ues = 0;

    free(data->additional_data.ues_thp);
    data->additional_data.ues_thp = 0;

  }
  if (strcmp(nodeType, "cu-cp") == 0 || strcmp(nodeType, "gNB") == 0) {
    free(data->nrcellcu.mcc);
    data->nrcellcu.mcc = 0;
    free(data->nrcellcu.mnc);
    data->nrcellcu.mnc = 0;

  }
  if (strcmp(nodeType, "cu-up") == 0) {
    free(data->gnbcuup.mcc);
    data->gnbcuup.mcc = 0;
    free(data->gnbcuup.mnc);
    data->gnbcuup.mnc = 0;
  }

  free(data);
}
