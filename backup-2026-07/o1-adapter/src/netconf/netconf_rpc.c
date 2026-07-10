#include "netconf/netconf_rpc.h"
#include "netconf/netconf_session.h"
#include "telnet/telnet.h"
#include "common/log.h"

/*
 * Callback for /o-ran-operations:reset RPC.
 * RPC has no input and no output.
 */
static int
reset_rpc_cb(sr_session_ctx_t *session,
             uint32_t          sub_id,
             const char       *op_path,
             const struct lyd_node *input,
             sr_event_t        event,
             uint32_t          request_id,
             struct lyd_node **output,
             void             *private_data)
{
    (void)session;
    (void)sub_id;
    (void)op_path;
    (void)input;
    (void)request_id;
    (void)output;
    (void)private_data;

    printf("reset_rpc_cb called, event=%d\n", event);

    /* For RPCs, you typically act only on SR_EV_RPC.
     * SR_EV_ABORT is sent if some other subscriber fails.
     */
    if (event == SR_EV_ABORT) {
        printf("Reset RPC aborted, nothing to do.\n");
        return SR_ERR_OK;
    }
    printf("Performing O-RU reset operation...\n");
    int reset_ok = telnet_edit_reset_ru();  /* Sending RU reset to nr-softmodem over telnet */

    if (reset_ok) {
        fprintf(stderr, "Reset operation failed!\n");
        return SR_ERR_OPERATION_FAILED;
    }

    printf("Reset operation completed successfully.\n");
    return SR_ERR_OK;
}

int netconf_rpcs_init(void)
{
    int rc = 0;
    sr_subscription_ctx_t *sub  = NULL;

    /* subscribe to /o-ran-operations:reset RPC */
    rc = sr_rpc_subscribe(netconf_session_running,
                          "/o-ran-operations:reset",
                          reset_rpc_cb,
                          NULL,   /* private data */
                          0,      /* priority */
                          0,      /* options */
                          &sub);
    if (rc != SR_ERR_OK) {
        log_error("sr_rpc_subscribe for /o-ran-operations:reset failed");
        return 1;
    }

    return 0;
}