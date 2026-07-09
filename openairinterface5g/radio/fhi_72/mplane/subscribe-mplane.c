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

#include "subscribe-mplane.h"
#include "rpc-send-recv.h"
#include "common/utils/assertions.h"

#include <libyang/libyang.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <nc_client.h>

extern ru_global_metrics_t ru_global_metrics;

#ifdef MPLANE_V1
static void recv_notif_v1(const struct nc_notif *notif, ru_notif_t *answer)
{
  const char *node_name = notif->tree->child->attr->name;
  const char *value = notif->tree->child->attr->value_str;
  if (strcmp(node_name, "sync-state")) {
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else {
      answer->ptp_state = false;
    }
  }

  // carriers state - to be filled
}

static void notif_clb_v1(struct nc_session *session, const struct nc_notif *notif)
{
  ru_notif_t *answer = nc_session_get_data(session);
  LYD_FORMAT output_format = LYD_JSON;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, notif->tree, output_format, LYP_WITHSIBLINGS);
  MP_LOG_I("\nReceived notification at (%s)\n%s\n", notif->datetime, subs_reply);

  recv_notif_v1(notif, answer);
}
#elif MPLANE_V2
static void recv_notif_v2(struct lyd_node_inner *op, ru_notif_t *answer)
{
  const char *notif = op->schema->name;

  if (strcmp(notif, "synchronization-state-change") == 0) {
    const char *value = lyd_get_value(op->child);
    if (strcmp(value, "LOCKED") == 0) {
      answer->ptp_state = true;
    } else { // "FREERUN" or "HOLDOVER"
      answer->ptp_state = false;
    }
  } else if (strcmp(notif, "rx-array-carriers-state-change") == 0) {
    const char *value = lyd_get_value(lyd_child(op->child)->next);
    if (strcmp(value, "READY") == 0) {
      answer->rx_carrier_state = true;
    } else { // "DISABLED" or "BUSY"
      answer->rx_carrier_state = false;
    }
  } else if (strcmp(notif, "tx-array-carriers-state-change") == 0) {
    const char *value = lyd_get_value(lyd_child(op->child)->next);
    if (strcmp(value, "READY") == 0) {
      answer->tx_carrier_state = true;
    } else { // "DISABLED" or "BUSY"
      answer->tx_carrier_state = false;
    }
  } else if (strcmp(notif, "netconf-config-change") == 0) {
    answer->config_change = true;
  }
}

#include <stdio.h>
#include <string.h>

// Helper function: extract value between <tag> and </tag>
int extract_value(const char *xml, const char *tag, char *out, size_t out_size) {
    char open_tag[128], close_tag[128];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    char *start = strstr(xml, open_tag);
    if (!start) return 0;
    start += strlen(open_tag);

    char *end = strstr(start, close_tag);
    if (!end) return 0;

    size_t len = end - start;
    if (len >= out_size) len = out_size - 1;

    strncpy(out, start, len);
    out[len] = '\0';
    return 1;
}

// Print only the last measurement-object
void print_performance_metrics(const char *xml) {

    printf("=== Performance Metrics ===\n");

    char value[128];
    if (extract_value(xml, "time", value, sizeof(value)))
        printf("Time: %s\n", value);


    if (extract_value(xml, "total-rx-good-pkt-cnt", value, sizeof(value)))
        printf("  Total RX Good Packets: %s\n", value);
    ru_global_metrics.total_rx_good_pkt_cnt = atol(value);
    if (extract_value(xml, "total-rx-bit-rate", value, sizeof(value)))
        printf("  Total RX Bitrate: %s\n", value);
    ru_global_metrics.total_rx_bit_rate = atol(value);
    if (extract_value(xml, "oran-rx-bit-rate", value, sizeof(value)))
        printf("  ORAN RX Bitrate: %s\n", value);
    ru_global_metrics.oran_rx_bit_rate = atoi(value);
    if (extract_value(xml, "oran-rx-on-time", value, sizeof(value)))
        printf("  ORAN RX On Time: %s\n", value);
    ru_global_metrics.oran_rx_on_time = atoi(value);
    if (extract_value(xml, "oran-rx-early", value, sizeof(value)))
        printf("  ORAN RX Early: %s\n", value);
    ru_global_metrics.oran_rx_early = atoi(value);
    if (extract_value(xml, "oran-rx-late", value, sizeof(value)))
        printf("  ORAN RX Late: %s\n", value);
    ru_global_metrics.oran_rx_late = atoi(value);
    if (extract_value(xml, "oran-rx-corrupt", value, sizeof(value)))
        printf("  ORAN RX Corrupt: %s\n", value);
    ru_global_metrics.oran_rx_corrupt = atoi(value);
    if (extract_value(xml, "oran-rx-total", value, sizeof(value)))
        printf("  ORAN RX Total: %s\n", value);
    ru_global_metrics.oran_rx_total = atoi(value);
    if (extract_value(xml, "oran-rx-total-c", value, sizeof(value)))
        printf("  ORAN RX Total C: %s\n", value);
    ru_global_metrics.oran_rx_total_c = atoi(value);
    if (extract_value(xml, "oran-rx-on-time-c", value, sizeof(value)))
        printf("  ORAN RX On Time C: %s\n", value);
    ru_global_metrics.oran_rx_on_time_c = atoi(value);
    if (extract_value(xml, "oran-rx-early-c", value, sizeof(value)))
        printf("  ORAN RX Early C: %s\n", value);
    ru_global_metrics.oran_rx_early_c = atoi(value);
    if (extract_value(xml, "oran-rx-late-c", value, sizeof(value)))
        printf("  ORAN RX Late C: %s\n", value);
    ru_global_metrics.oran_rx_late_c = atoi(value);
    if (extract_value(xml, "oran-rx-error-drop", value, sizeof(value)))
        printf("  ORAN RX Error Drop: %s\n", value);
    ru_global_metrics.oran_rx_error_drop = atoi(value);
    if (extract_value(xml, "oran-tx-total", value, sizeof(value)))
      printf("  ORAN TX Total: %s\n", value);
    ru_global_metrics.oran_tx_total = atoi(value);
    if (extract_value(xml, "oran-tx-total-c", value, sizeof(value)))
      printf("  ORAN TX Total C: %s\n", value);
    ru_global_metrics.oran_tx_total_c = atoi(value);
}
static void notif_clb_v2(struct nc_session *session, const struct lyd_node *envp, const struct lyd_node *op, void *user_data)
{
  ru_notif_t *answer = (ru_notif_t *)user_data;
  LYD_FORMAT output_format = LYD_JSON;

  char *subs_reply = NULL;
  lyd_print_mem(&subs_reply, op, output_format, LYD_PRINT_WITHSIBLINGS);
  const char *ru_ip_add = nc_session_get_host(session);
  MP_LOG_I("Received notification from RU \"%s\" at (%s)\n%s\n", ru_ip_add, ((struct lyd_node_opaq *)lyd_child(envp))->value, subs_reply);

  
  printf("ru_global_metrics.ru_reset = %d\n", ru_global_metrics.ru_reset);
  if (ru_global_metrics.ru_reset == 1) {
    const char *reset_xml = "<reset xmlns=\"urn:o-ran:operations:1.0\"/>";
    struct nc_rpc *reset_rpc = nc_rpc_act_generic_xml(reset_xml, NC_PARAMTYPE_CONST);
    if (!reset_rpc) {
      MP_LOG_E("Failed to create reset RPC for RU \"%s\".\n", ru_ip_add);
    } else {
      int msgtype;
      int msgid;
      msgtype = nc_send_rpc(session, reset_rpc, 10000, &msgid); /* 10s timeout for sending */
      if (msgtype == NC_MSG_ERROR) {
        MP_LOG_E("Error while sending reset RPC to RU \"%s\".\n", ru_ip_add);
      } else if (msgtype == NC_MSG_WOULDBLOCK) {
        MP_LOG_E("Timeout while sending reset RPC to RU \"%s\".\n", ru_ip_add);
      } else if (msgtype != NC_MSG_RPC) {
        MP_LOG_E("Unexpected message type (%d) when sending reset RPC to RU \"%s\".\n", msgtype, ru_ip_add);
      } else {
        MP_LOG_I("Reset RPC sent to RU \"%s\".\n", ru_ip_add);
        ru_global_metrics.ru_reset = 0;
      }
      nc_rpc_free(reset_rpc);
      return;
    }
  }

  recv_notif_v2((struct lyd_node_inner *)op, answer);
  /* Check if notification contains "o-ran-performance" */
  if (subs_reply == NULL || strstr(subs_reply, "o-ran-performance") == NULL) {
    MP_LOG_I("Notification does not contain o-ran-performance change, skipping.\n");
    return;
  }

  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  char *operational_ds = NULL;
  char *filter = "/*[local-name()='performance-counters']/*[local-name()='measurement-object'][last()]";
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;

  MP_LOG_I("RPC request to RU \"%s\" = <get> operational datastore.\n", ru_ip_add);
  rpc = nc_rpc_get(filter, wd, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <get> RPC creation failed.\n");

  bool success = rpc_send_recv(session, rpc, wd, timeout, &operational_ds);
  print_performance_metrics(operational_ds);
  AssertError(success, return false, "[MPLANE] Unable to retreive operational datastore.\n");
}
#endif

bool subscribe_mplane(ru_session_t *ru_session, const char *stream, const char *filter, void *answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  char *start_time = NULL, *stop_time = NULL;

  MP_LOG_I("RPC request to RU \"%s\" = <subscribe> with stream \"%s\" and filter \"%s\".\n", ru_session->ru_ip_add, stream, filter);
  rpc = nc_rpc_subscribe(stream, NULL, start_time, stop_time, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <subscribe> RPC creation failed.\n");

  /* create notification thread so that notifications can immediately be received */
#ifdef MPLANE_V1
  if (!nc_session_ntf_thread_running(ru_session->session)) {
    nc_session_set_data(ru_session->session, answer);
    int ret = nc_recv_notif_dispatch(ru_session->session, notif_clb_v1);
    AssertError(ret == 0, return false, "[MPLANE] Failed to create notification thread.\n");
  } else {
    MP_LOG_I("Notification thread is already running for RU %s.\n", ru_session->ru_ip_add);
  }
#elif MPLANE_V2
  int ret = nc_recv_notif_dispatch_data(ru_session->session, notif_clb_v2, answer, NULL);
  AssertError(ret == 0, return false, "[MPLANE] Failed to create notification thread.\n");
#endif

  bool success = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, NULL);
  AssertError(success, return false, "[MPLANE] Failed to subscribe to: stream \"%s\", filter \"%s\".\n", stream, filter);

  MP_LOG_I("Successfully subscribed to all notifications from RU \"%s\".\n", ru_session->ru_ip_add);

  nc_rpc_free(rpc);

  return true;
}

bool update_timer_mplane(ru_session_t *ru_session, char **answer)
{
  int timeout = CLI_RPC_REPLY_TIMEOUT;
  struct nc_rpc *rpc;
  NC_WD_MODE wd = NC_WD_ALL;
  NC_PARAMTYPE param = NC_PARAMTYPE_CONST;
  const char *content = "<supervision-watchdog-reset xmlns=\"urn:o-ran:supervision:1.0\">\n\
<supervision-notification-interval>65535</supervision-notification-interval>\n\
<guard-timer-overhead>65535</guard-timer-overhead>\n\
</supervision-watchdog-reset>";

  MP_LOG_I("RPC request to RU \"%s\" = \"%s\".\n", ru_session->ru_ip_add, content);
  rpc = nc_rpc_act_generic_xml(content, param);
  AssertError(rpc != NULL, return false, "[MPLANE] <supervision-watchdog-reset> RPC creation failed.\n");

  bool success = rpc_send_recv((struct nc_session *)ru_session->session, rpc, wd, timeout, answer);
  AssertError(success, return false, "[MPLANE] Failed to update the watchdog timer.\n");

  MP_LOG_I("Successfully updated supervision timer to (65535+65535)[s] for RU \"%s\".\n", ru_session->ru_ip_add);

  nc_rpc_free(rpc);

  return true;
}
