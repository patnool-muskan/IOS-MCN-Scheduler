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

#ifndef RU_MPLANE_API_H
#define RU_MPLANE_API_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "common/utils/LOG/log.h"

#define MP_LOG_I(x, args...) LOG_I(HW, "[MPLANE] " x, ##args)
#define MP_LOG_W(x, args...) LOG_W(HW, "[MPLANE] " x, ##args)

typedef struct {
  bool ptp_state;
  bool rx_carrier_state;
  bool tx_carrier_state;
  bool config_change;
  // to be extended with any notification callback

} ru_notif_t;

typedef struct {
  char *ru_mac_addr;
  uint32_t mtu;
  int16_t iq_width;
  uint8_t prach_offset;

  // DU sends to RU and xran
  uint16_t du_port_bitmask;
  uint16_t band_sector_bitmask;
  uint16_t ccid_bitmask;
  uint16_t ru_port_bitmask;

  // DU retreives from RU, and sends to xran
  uint8_t du_port;
  uint8_t band_sector;
  uint8_t ccid;
  uint8_t ru_port;

} xran_mplane_t;

typedef struct {
  size_t num;
  char **name;
} uplane_info_t;

typedef struct array_carrier {
  char name[50];
  uint32_t arfcn_center;
  uint64_t center_channel_bw;
  uint32_t channel_bw;
  char ru_carrier[10];
  char rw_duplex_scheme[10];
  char rw_type[10];
  uint8_t dl_radio_frame_offset;
  uint8_t dl_sfn_offset;
  float gain_correction;
  float gain;
  uint32_t n_ta_offset;
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

typedef struct {
  char *du_mac_addr[2]; // [0] -> U-plane; [1] -> C-plane
  int32_t vlan_tag[2];  // [0] -> U-plane; [1] -> C-plane
  char *interface_name;
  uplane_info_t tx_endpoints;
  uplane_info_t rx_endpoints;
  uplane_info_t tx_carriers;
  uplane_info_t rx_carriers;
  array_carrier_t tx_array_carrier;
  array_carrier_t rx_array_carrier;
  delay_management_t delay;

} ru_mplane_config_t;

typedef struct{
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

} ru_mplane_metrics_t;

typedef struct {
  char *ru_ip_add;
  char *ru_username;
  ru_mplane_config_t ru_mplane_config;
  ru_mplane_metrics_t ru_mplane_metrics;
  void *session;
  xran_mplane_t xran_mplane;
  ru_notif_t ru_notif;

} ru_session_t;

typedef struct {
  size_t num_rus;
  ru_session_t *ru_session;
  char **du_key_pair;

} ru_session_list_t;

bool get_config_for_xran(const char *buffer, const int max_num_ant, xran_mplane_t *xran_mplane);

bool get_uplane_info(const char *buffer, ru_mplane_config_t *ru_mplane_config);

#endif /* RU_MPLANE_API_H */
