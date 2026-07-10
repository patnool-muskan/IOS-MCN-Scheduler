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

#pragma once

#include <stdint.h>

typedef struct oai_nrsectcarr_data {
  char *txDirection;
  int configuredMaxTxPower;
  int configuredMaxTxEIRP;
  int arfcnDL;
  int arfcnUL;
  int bSChannelBwDL;
  int bSChannelBwUL;
  char *sectorEquipmentFunctionRef;
} oai_nrsectcarr_data_t;

typedef struct oai_ues_thp {
  int rnti;
  int dl;
  int ul;
  uint32_t dl_ul_tbs;
  uint64_t dl_tbs_err;
  uint64_t ul_tbs_err;
  uint32_t tot_ul_prbs;
  uint32_t dl_prbs;
  uint32_t ul_prbs;
  uint32_t ul_pdcp_sdu;
  uint32_t dl_pdcp_sdu;
} oai_ues_thp_t;

typedef struct oai_additional_data {
  char *frameType;
  int bandNumber;
  int numUes;
  int *ues;
  int load;
  oai_ues_thp_t *ues_thp;
  int num_dus;
  int num_cuups;
  char vendor;
} oai_additional_data_t;

typedef struct oai_config_data {
  int gnb_DU_Id;
  char *gnbCUName;
  int gnbId;
  int gnbIdLength;
  char *mcc;
  char *mnc;
  char *gnbName;
  char *vendor;
} oai_device_data_t;

typedef struct oai_bwp_data {
  int isInitialBwp;
  int numberOfRBs;
  int startRB;
  int subCarrierSpacing;
} oai_bwp_data_t;

typedef struct oai_nrcelldu_data {
  int ssbFrequency;
  int arfcnDL;
  int bSChannelBwDL;
  int arfcnUL;
  int bSChannelBwUL;
  int nRPCI;
  int nRTAC;
  char *mcc;
  char *mnc;
  uint32_t sd;
  uint32_t sst;
  uint32_t ssb_period;
  uint32_t ssb_offset;
  enum DL_UL_TransmissionPeriodicity {ms0p5 = 0,
	  ms0p625 = 1,
	  ms1 = 2,
	  ms1p25 = 3,
	  ms2 = 4,
	  ms2p5 = 5,
	  ms5 = 6,
	  ms10 = 7} TP;
  uint16_t nrofDownlinkSlots;
  uint8_t nr_of_downlink_symbols;
  uint16_t nrofUplinkSlots;
  uint8_t nr_of_uplink_symbols;
} oai_nrcelldu_data_t;

typedef struct array_carrier {
  uint32_t arfcn_center;
  uint32_t center_channel_bw;
  uint32_t channel_bw;
  char ru_carrier[10];
  char rw_duplex_scheme[10];
  char rw_type[10];
  uint8_t dl_radio_frame_offset;
  uint8_t dl_sfn_offset;
  uint8_t gain_correction;
  uint8_t gain;
  uint8_t nta_offset;
} array_carrier_t;

typedef struct delay_management {
  uint32_t T2a_min_up;
  uint32_t T2a_max_up;
  uint32_t T2a_min_cp_dl;
  uint32_t T2a_max_cp_dl;
  uint32_t Tcp_adv_dl;
  uint32_t Ta3_min;
  uint32_t Ta3_max;
  uint32_t T2a_min_cp_ul;
  uint32_t T2a_max_cp_ul;
} delay_management_t;

typedef struct o_ran_performance_metrics {
  uint32_t total_rx_good_pkt_cnt;
  uint32_t total_rx_bit_rate;
  uint32_t oran_rx_on_time;
  uint32_t oran_rx_early;
  uint32_t oran_rx_late;
  uint32_t oran_rx_corrupt;
  uint32_t oran_rx_total;
  uint32_t oran_rx_total_c;
  uint32_t oran_rx_on_time_c;
  uint32_t oran_rx_early_c;
  uint32_t oran_rx_late_c;
  uint32_t oran_rx_error_drop;
  uint32_t oran_tx_total;
  uint32_t oran_tx_total_c;
} o_ran_performance_metrics_t;

typedef struct oai_oru_data {
  array_carrier_t tx_array_carrier;
  array_carrier_t rx_array_carrier;
  delay_management_t delay_management;
  o_ran_performance_metrics_t o_ran_performance_metrics;
} oai_oru_data_t;

typedef struct oai_nrsectorcarrier {
  int arfcnDL;
  int bSChannelBwDL;
  int arfcnUL;
  int bSChannelBwUL;
} oai_nrsectorcarrier_t;

typedef struct oai_nrcellcu_data {
  uint32_t pMax;
  int cellLocalId;
  char *mcc;
  char *mnc;
  uint32_t sd;
  uint32_t sst;
  uint32_t tac;
  uint32_t mnc_digit_length;
} oai_nrcellcu_data_t;

typedef struct oai_nrfreqrel_data {
  uint8_t tReselectionNRSfHigh;
  uint32_t tReselectionNR;
  uint8_t tReselectionNRSfMedium;
  uint32_t pMax;
  char *nRFrequencyRef;
  uint8_t cellReselectionSubPriority;
  uint32_t cellReselectionPriority;
  uint8_t qOffsetFreq;
  uint32_t threshXLowP;
  uint32_t threshXLowQ;
  uint32_t threshXHighQ;
  uint32_t threshXHighP;
  int32_t qQualMin;
  int32_t qRxLevMin;
} oai_nrfreqrel_data_t;

typedef struct oai_gnbcuup_data {
  uint64_t gNBCUUPId;
  uint32_t gNBId;
  char *mcc;
  char *mnc;
  int mnc_length;
} oai_gnbcuup_data_t;

typedef struct oai_data {
  oai_bwp_data_t bwp[2];
  oai_nrcelldu_data_t nrcelldu;
  oai_device_data_t device_data;
  oai_additional_data_t additional_data;
  oai_nrsectorcarrier_t nrSectorCarrier;
  oai_nrcellcu_data_t nrcellcu;
  oai_nrfreqrel_data_t nrfreqrel;
  oai_nrsectcarr_data_t nrsectcarr;
  oai_gnbcuup_data_t gnbcuup;
  oai_oru_data_t oru;
} oai_data_t;

oai_data_t *oai_data_parse_json(const char *json);
void oai_data_print(const oai_data_t *data);
oai_data_t *oai_data_parse_json(const char *json);
void oai_data_free(oai_data_t *data);

/* helper macros to reduce boilerplate checks */
#ifndef PJSON_GET_OBJ
#define PJSON_GET_OBJ(parent, key) cJSON_GetObjectItem((parent), (key))
#endif

#ifndef PJSON_PARSE_INT
#define PJSON_PARSE_INT(parent, key, target)                                 \
do {                                                                         \
  cJSON *o = PJSON_GET_OBJ((parent), (key));                                 \
  if ((o) == NULL) {                                                         \
    log_error("not found: %s", (key));                                       \
    goto failed;                                                             \
  }                                                                          \
  if (!cJSON_IsNumber(o)) {                                                   \
    log_error("failed type: %s", (key));                                     \
    goto failed;                                                             \
  }                                                                          \
  (target) = (o)->valueint;                                                  \
} while (0)
#endif

#ifndef PJSON_PARSE_UINT32
#define PJSON_PARSE_UINT32(parent, key, target)                              \
do {                                                                         \
  cJSON *o = PJSON_GET_OBJ((parent), (key));                                 \
  if ((o) == NULL) {                                                         \
    log_error("not found: %s", (key));                                       \
    goto failed;                                                             \
  }                                                                          \
  if (!cJSON_IsNumber(o)) {                                                   \
    log_error("failed type: %s", (key));                                     \
    goto failed;                                                             \
  }                                                                          \
  (target) = (uint32_t)(o)->valuedouble;                                     \
} while (0)
#endif

#ifndef PJSON_PARSE_STRDUP
#define PJSON_PARSE_STRDUP(parent, key, target)                              \
do {                                                                         \
  cJSON *o = PJSON_GET_OBJ((parent), (key));                                 \
  if ((o) == NULL) {                                                         \
    log_error("not found: %s", (key));                                       \
    goto failed;                                                             \
  }                                                                          \
  if (!cJSON_IsString(o)) {                                                   \
    log_error("failed type: %s", (key));                                     \
    goto failed;                                                             \
  }                                                                          \
  (target) = strdup((o)->valuestring);                                       \
  if ((target) == NULL) {                                                    \
    log_error("strdup failed: %s", (key));                                   \
    goto failed;                                                             \
  }                                                                          \
} while (0)
#endif

#ifndef PJSON_PARSE_BOOL_TO_INT
#define PJSON_PARSE_BOOL_TO_INT(parent, key, target)                         \
do {                                                                         \
  cJSON *o = PJSON_GET_OBJ((parent), (key));                                 \
  if ((o) == NULL) {                                                         \
    log_error("not found: %s", (key));                                       \
    goto failed;                                                             \
  }                                                                          \
  if (!cJSON_IsBool(o)) {                                                     \
    log_error("failed type: %s", (key));                                     \
    goto failed;                                                             \
  }                                                                          \
  (target) = cJSON_IsTrue(o);                                                \
} while (0)
#endif

#ifndef PJSON_PARSE_ARRAY
#define PJSON_PARSE_ARRAY(parent, key, target)                               \
do {                                                                         \
  (target) = PJSON_GET_OBJ((parent), (key));                                 \
  if ((target) == NULL) {                                                    \
    log_error("not found: %s", (key));                                       \
    goto failed;                                                             \
  }                                                                          \
  if (!cJSON_IsArray((target))) {                                            \
    log_error("%s not array", (key));                                        \
    goto failed;                                                             \
  }                                                                          \
} while (0)
#endif

#ifndef GET_JSON_INT
#define GET_JSON_INT(parent, key, target)                                  \
do {                                                                       \
  cJSON *obj = cJSON_GetObjectItem((parent), (key));                     \
  if (obj == NULL) {                                                     \
    log_error("not found: %s", (key));                                 \
    goto failed;                                                       \
  }                                                                      \
  if (!cJSON_IsNumber(obj)) {                                            \
    log_error("failed type: %s", (key));                               \
    goto failed;                                                       \
  }                                                                      \
  (target) = obj->valueint;                                              \
} while (0)
#endif

#ifndef GET_JSON_UINT32
#define GET_JSON_UINT32(parent, key, target)                               \
do {                                                                       \
  cJSON *obj = cJSON_GetObjectItem((parent), (key));                     \
  if (obj == NULL) {                                                     \
    log_error("not found: %s", (key));                                 \
    goto failed;                                                       \
  }                                                                      \
  if (!cJSON_IsNumber(obj)) {                                            \
    log_error("failed type: %s", (key));                               \
    goto failed;                                                       \
  }                                                                      \
  (target) = (uint32_t)obj->valuedouble;                                 \
} while (0)
#endif

#ifndef GET_JSON_STR
#define GET_JSON_STR(parent, key, target_ptr)                              \
do {                                                                       \
  cJSON *obj = cJSON_GetObjectItem((parent), (key));                     \
  if (obj == NULL) {                                                     \
    log_error("not found: %s", (key));                                 \
    goto failed;                                                       \
  }                                                                      \
  if (!cJSON_IsString(obj)) {                                            \
    log_error("failed type: %s", (key));                               \
    goto failed;                                                       \
  }                                                                      \
  (target_ptr) = obj->valuestring;                                       \
} while (0)
#endif