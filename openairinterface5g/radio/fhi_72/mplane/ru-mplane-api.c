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

#include "ru-mplane-api.h"
#include "xml/get-xml.h"
#include "common/utils/assertions.h"
#include <libxml/parser.h>

#include <string.h>

static void fix_benetel_setting(xran_mplane_t *xran_mplane, const uint32_t interface_mtu, const int16_t first_iq_width, const int max_num_ant)
{
  if (interface_mtu == 1500) {
    MP_LOG_I("Interface MTU %d unreliable/not correctly reported by Benetel O-RU, hardcoding to 9600.\n", interface_mtu);
    xran_mplane->mtu = 9600;
  } else {
    xran_mplane->mtu = interface_mtu;
  }

  if (first_iq_width != 9) {
    MP_LOG_I("IQ bitwidth %d unreliable/not correctly reported by Benetel O-RU, hardcoding to 9.\n", first_iq_width);
    xran_mplane->iq_width = 9;
  } else {
    xran_mplane->iq_width = first_iq_width;
  }

  xran_mplane->prach_offset = max_num_ant;
}

bool get_config_for_xran(const char *buffer, const int max_num_ant, xran_mplane_t *xran_mplane)
{
  /* some O-RU vendors are not fully compliant as per M-plane specifications */
  const char *ru_vendor = get_ru_xml_node(buffer, "mfg-name");

  // RU MAC
  xran_mplane->ru_mac_addr = get_ru_xml_node(buffer, "mac-address"); // TODO: support for VVDN, as it defines multiple MAC addresses

  // MTU
  const char *mtu_str = get_ru_xml_node(buffer, "interface-mtu");
  const uint32_t interface_mtu = mtu_str ? (uint32_t)atoi(mtu_str) : 9600;  // default to 9600 if not found

  // IQ bitwidth
  char **match_list = NULL;
  size_t count = 0;
  get_ru_xml_list(buffer, "iq-bitwidth", &match_list, &count);
  const int16_t first_iq_width = (int16_t)atoi((char *)match_list[0]);

  // PRACH offset
  // xran_mplane->prach_offset

  // DU port ID bitmask
  xran_mplane->du_port_bitmask = 0xf000;
  // Band sector bitmask
  xran_mplane->band_sector_bitmask = 0x0f00;
  // CC ID bitmask
  xran_mplane->ccid_bitmask = 0x00f0;
  // RU port ID bitmask
  xran_mplane->ru_port_bitmask = 0x000f;

  // DU port ID
  xran_mplane->du_port = 0;
  // Band sector
  xran_mplane->band_sector = 0;
  // CC ID
  xran_mplane->ccid = 0;
  // RU port ID
  xran_mplane->ru_port = 0;

  // if (strcasecmp(ru_vendor, "BENETEL") == 0 /* || strcmp(ru_vendor, "VVDN-LPRU") == 0 || strcmp(ru_vendor, "Metanoia") == 0 */) {
  //   fix_benetel_setting(xran_mplane, interface_mtu, first_iq_width, max_num_ant);
  // } else {
  //   AssertError(false, return false, "[MPLANE] %s RU currently not supported.\n", ru_vendor);
  // }

  MP_LOG_I("Storing the following information to forward to xran:\n\
    RU MAC address %s\n\
    MTU %d\n\
    IQ bitwidth %d\n\
    PRACH offset %d\n\
    DU port bitmask %d\n\
    Band sector bitmask %d\n\
    CC ID bitmask %d\n\
    RU port ID bitmask %d\n\
    DU port ID %d\n\
    Band sector ID %d\n\
    CC ID %d\n\
    RU port ID %d\n",
      xran_mplane->ru_mac_addr,
      xran_mplane->mtu,
      xran_mplane->iq_width,
      xran_mplane->prach_offset,
      xran_mplane->du_port_bitmask,
      xran_mplane->band_sector_bitmask,
      xran_mplane->ccid_bitmask,
      xran_mplane->ru_port_bitmask,
      xran_mplane->du_port,
      xran_mplane->band_sector,
      xran_mplane->ccid,
      xran_mplane->ru_port);

  return true;
}

static void log_ru_delay_profile(delay_management_t *delay)
{
  printf("\
  T2a_min_up %d\n\
  T2a_max_up %d\n\
  T2a_min_cp_dl %d\n\
  T2a_max_cp_dl %d\n\
  Tcp_adv_dl %d\n\
  Ta3_min %d\n\
  Ta3_max %d\n\
  T2a_min_cp_ul %d\n\
  T2a_max_cp_ul %d\n",
    delay->T2a_min_up,
    delay->T2a_max_up,
    delay->T2a_min_cp_dl,
    delay->T2a_max_cp_dl,
    delay->Tcp_adv_dl,
    delay->Ta3_min,
    delay->Ta3_max,
    delay->T2a_min_cp_ul,
    delay->T2a_max_cp_ul);
}

static void store_ru_delay_profile(xmlNode *node, delay_management_t *delay)
{
  for (xmlNode *cur_child = node; cur_child; cur_child = cur_child->next) {
    if(cur_child->type == XML_ELEMENT_NODE){
      int value = atoi((const char *)xmlNodeGetContent(cur_child));

      if (strcmp((const char *)cur_child->name, "t2a-min-up") == 0) {
        delay->T2a_min_up = value;
      } else if (strcmp((const char *)cur_child->name, "t2a-max-up") == 0) {
        delay->T2a_max_up = value;
      } else if (strcmp((const char *)cur_child->name, "t2a-min-cp-dl") == 0) {
        delay->T2a_min_cp_dl = value;
      } else if (strcmp((const char *)cur_child->name, "t2a-max-cp-dl") == 0) {
        delay->T2a_max_cp_dl = value;
      } else if (strcmp((const char *)cur_child->name, "tcp-adv-dl") == 0) {
        delay->Tcp_adv_dl = value;
      } else if (strcmp((const char *)cur_child->name, "ta3-min") == 0) {
        delay->Ta3_min = value;
      } else if (strcmp((const char *)cur_child->name, "ta3-max") == 0) {
        delay->Ta3_max = value;
      } else if (strcmp((const char *)cur_child->name, "t2a-min-cp-ul") == 0) {
        delay->T2a_min_cp_ul = value;
      } else if (strcmp((const char *)cur_child->name, "t2a-max-cp-ul") == 0) {
        delay->T2a_max_cp_ul = value;
      }
    }
  }
}

static void find_ru_delay_profile(xmlNode *node, delay_management_t *delay)
{
  for(xmlNode *cur_node = node; cur_node; cur_node = cur_node->next){
    if(cur_node->type == XML_ELEMENT_NODE){
      if(strcmp((const char*)cur_node->name, "ru-delay-profile") == 0){
        store_ru_delay_profile(cur_node->children, delay);
        break;
      } else {
        find_ru_delay_profile(cur_node->children, delay);
      }
    }
  }
}

bool get_ru_delay_profile(const char *xml_string, ru_mplane_config_t *ru_mplane_config)
{

  // Initialize the xml file
  xmlDoc *doc = xmlReadMemory(xml_string, strlen(xml_string), "", NULL, 0);
  xmlNode *root_element = xmlDocGetRootElement(doc);

  delay_management_t *delay = &ru_mplane_config->delay;
  find_ru_delay_profile(root_element->children, delay);
  log_ru_delay_profile(delay);

  return true;
}

static void store_tx_array_carriers(xmlNode *node, array_carrier_t *tx_array_carrier)
{
  for (xmlNode *cur_child = node; cur_child; cur_child = cur_child->next) {
    if(cur_child->type == XML_ELEMENT_NODE){
    //   int value = atoi((const char *)xmlNodeGetContent(cur_child));
    int value;

      if (strcmp((const char *)cur_child->name, "name") == 0) {
        strcpy(tx_array_carrier->name, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "absolute-frequency-center") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->arfcn_center = value;
      } else if (strcmp((const char *)cur_child->name, "center-of-channel-bandwidth") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->center_channel_bw = (uint32_t)value;
      } else if (strcmp((const char *)cur_child->name, "channel-bandwidth") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->channel_bw = value;
      } else if (strcmp((const char *)cur_child->name, "active") == 0) {
        strcpy(tx_array_carrier->ru_carrier, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "rw-duplex-scheme") == 0) {
        strcpy(tx_array_carrier->rw_duplex_scheme, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "rw-type") == 0) {
        strcpy(tx_array_carrier->rw_type, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "gain") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->gain = value;
      } else if (strcmp((const char *)cur_child->name, "downlink-radio-frame-offset") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->dl_radio_frame_offset = value;
      } else if (strcmp((const char *)cur_child->name, "downlink-sfn-offset") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        tx_array_carrier->dl_sfn_offset = value;
      }
    }
  }
}

static void store_rx_array_carriers(xmlNode *node, array_carrier_t *rx_array_carrier)
{
  for (xmlNode *cur_child = node; cur_child; cur_child = cur_child->next) {
    if(cur_child->type == XML_ELEMENT_NODE){
    //   int value = atoi((const char *)xmlNodeGetContent(cur_child));
    int value;

      if (strcmp((const char *)cur_child->name, "name") == 0) {
        strcpy(rx_array_carrier->name, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "absolute-frequency-center") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->arfcn_center = value;
      } else if (strcmp((const char *)cur_child->name, "center-of-channel-bandwidth") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->center_channel_bw = (uint32_t)value;
      } else if (strcmp((const char *)cur_child->name, "channel-bandwidth") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->channel_bw = value;
      } else if (strcmp((const char *)cur_child->name, "active") == 0) {
        strcpy(rx_array_carrier->ru_carrier, (const char *)xmlNodeGetContent(cur_child));
      } else if (strcmp((const char *)cur_child->name, "downlink-radio-frame-offset") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->dl_radio_frame_offset = value;
      } else if (strcmp((const char *)cur_child->name, "downlink-sfn-offset") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->dl_sfn_offset = value;
      } else if (strcmp((const char *)cur_child->name, "gain-correction") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->gain_correction = value;
      } else if (strcmp((const char *)cur_child->name, "n-ta-offset") == 0) {
        value = atoi((const char *)xmlNodeGetContent(cur_child));
        rx_array_carrier->n_ta_offset = value;
      } 
    }
  }
}

static void find_uplane_conf_data(xmlNode *node, ru_mplane_config_t *ru_mplane_config){
    for(xmlNode *cur_node = node; cur_node; cur_node = cur_node->next){
            if(cur_node->type == XML_ELEMENT_NODE){
            if(strcmp((const char*)cur_node->name, "tx-array-carriers") == 0){
                store_tx_array_carriers(cur_node->children, &ru_mplane_config->tx_array_carrier);
                break;
            } else {
                find_uplane_conf_data(cur_node->children, ru_mplane_config);
            }
        }
    }
    for(xmlNode *cur_node = node; cur_node; cur_node = cur_node->next){
            if(cur_node->type == XML_ELEMENT_NODE){
            if(strcmp((const char*)cur_node->name, "rx-array-carriers") == 0){
                store_rx_array_carriers(cur_node->children, &ru_mplane_config->rx_array_carrier);
                break;
            } else {
                find_uplane_conf_data(cur_node->children, ru_mplane_config);
            }
        }
    }
}

void get_uplane_conf_data(const char *xml_string, ru_mplane_config_t *ru_mplane_config){

  // Initialize the xml file
  xmlDoc *doc = xmlReadMemory(xml_string, strlen(xml_string), "", NULL, 0);
  xmlNode *root_element = xmlDocGetRootElement(doc);

  find_uplane_conf_data(root_element->children, ru_mplane_config);
}

bool get_uplane_info(const char *buffer, ru_mplane_config_t *ru_mplane_config)
{
  // Interface name
  ru_mplane_config->interface_name = get_ru_xml_node(buffer, "interface");

  // PDSCH
  // uplane_info_t *tx_end = &ru_mplane_config->tx_endpoints;
  // get_ru_xml_list(buffer, "static-low-level-tx-endpoints", &tx_end->name, &tx_end->num);
  // AssertError(tx_end->name != NULL, return false, "[MPLANE] Cannot get TX endpoint names.\n");

  // TX carriers
  // uplane_info_t *tx_carriers = &ru_mplane_config->tx_carriers;
  // get_ru_xml_list(buffer, "tx-arrays", &tx_carriers->name, &tx_carriers->num);
  // AssertError(tx_carriers->name != NULL, return false, "[MPLANE] Cannot get TX carrier names.\n");

  // PUSCH and PRACH
  // uplane_info_t *rx_end = &ru_mplane_config->rx_endpoints;
  // get_ru_xml_list(buffer, "static-low-level-rx-endpoints", &rx_end->name, &rx_end->num);
  // AssertError(rx_end->name != NULL, return false, "[MPLANE] Cannot get RX endpoint names.\n");

  // RX carriers
  // uplane_info_t *rx_carriers = &ru_mplane_config->rx_carriers;
  // get_ru_xml_list(buffer, "rx-arrays", &rx_carriers->name, &rx_carriers->num);
  // AssertError(rx_carriers->name != NULL, return false, "[MPLANE] Cannot get RX carrier names.\n");

  MP_LOG_I("Successfully retreived all the U-plane info - interface name, TX/RX carrier names, and TX/RX endpoint names.\n");

  return true;
}
