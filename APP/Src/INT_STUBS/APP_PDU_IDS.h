#ifndef APP_PDU_IDS_H
#define APP_PDU_IDS_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "MRA_CLUSTER.h"   /* MRA_CLUSTER_NUM_TX_MSGS - sizes the MRA_CLUSTER TX route block below */
#include "MQB_CLUSTER.h"   /* MQB_CLUSTER_NUM_TX_MSGS - sizes the MQB_CLUSTER TX route block below */

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Must match the number of ENTRY(n) calls in APP_SENSOR_SLOTS (INTEGRATION_STUBS.c) - the enum below
   needs a plain integer, which can't be derived from that list without more preprocessor machinery
   than 12 fixed slots justify. */
#define APP_NUM_SENSOR_SLOTS ( 12u )

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
/* APP's CAN buses - the value a route's PDUR .channel and its CANTP instance's .channel both carry.
   The main bus is 0 so every route that doesn't name a channel lands on it by default. Adding a bus:
   a new enumerator here, a CANTP instance + driver for it (INTEGRATION_STUBS.c, app_cantp_instance_for()),
   and .channel = <it> on each of its routes. */
typedef enum
{
   APP_CAN_CH_MAIN = 0u,        /* bxCAN1 via TJA1051, 500 kbit/s */
   APP_CAN_NUM_CHANNELS
} app_can_channel_et;

/* APP's own logical PDU IDs - these, not raw CAN IDs, are what pdur_routing_table_s's array position
   means and what PDUR_tx() dispatches on (see PDUR.h). Values double as array indices (designated-
   index initializers in INTEGRATION_STUBS.c pin each route to its enum value explicitly, so
   reordering this enum without reordering the table - or vice versa - is a compile error from a
   duplicate/out-of-range index, not a silent mismatch).

   Lives in its own header because both sides of a route need it: INTEGRATION_STUBS.c builds the table
   from these values, and each module that transmits (via PDUR_tx) is handed its route id from its
   own config in VEHICLE_SIM_config.c. */
typedef enum
{
   APP_PDU_SENSOR_BASE          = 0u,                    /* sensor slots occupy +0 .. +(APP_NUM_SENSOR_SLOTS-1); tx-only, sent by MSG_SCHED */
   APP_PDU_GKT_LEVER_RX         = APP_PDU_SENSOR_BASE + APP_NUM_SENSOR_SLOTS,   /* rx-only: GKT_SHIFTER_STATE_CAN_ID (0x197) -> GKT_SHIFTER */
   APP_PDU_GKT_HEARTBEAT_RX,    /* rx-only: GKT_SHIFTER_HEARTBEAT_CAN_ID (0x55E) -> GKT_SHIFTER */
   APP_PDU_UDS_FUNCTIONAL,      /* rx: APP_UDS_REQUEST_ID (0x700) / tx: APP_UDS_RESPONSE_ID (0x600) */
   APP_PDU_UDS_PHYSICAL,        /* rx: APP_CAN_RX_ID (0x7E0) / tx: APP_CAN_TX_ID (0x7E8) */
   APP_PDU_GKT_LED_TX,          /* tx-only: GKT_SHIFTER_LED_CAN_ID (0x3FD), sent by GKT_SHIFTER_tick() */
   APP_PDU_GKT_BACKLIGHT_TX,    /* tx-only: GKT_SHIFTER_BACKLIGHT_CAN_ID (0x202), sent by GKT_SHIFTER_tick() */
   APP_PDU_TCU_GEAR_STATE_TX,   /* tx-only: TCU_GEAR_STATE_CAN_ID, sent by TCU_tick() */
   APP_PDU_TCU_DYNAMICS_TX,     /* tx-only: TCU_DYNAMICS_CAN_ID (RPM + speed + requested throttle), sent by TCU_tick() */
   APP_PDU_MRA_TX_BASE,         /* tx-only: MRA_CLUSTER_NUM_TX_MSGS routes, one per MRA_CLUSTER_tx_msg_et, in that order */
   APP_PDU_MQB_TX_BASE          = APP_PDU_MRA_TX_BASE + MRA_CLUSTER_NUM_TX_MSGS,   /* tx-only: MQB_CLUSTER_NUM_TX_MSGS routes, one per MQB_CLUSTER_tx_msg_et, in that order */
   APP_PDU_NUM_ROUTES           = APP_PDU_MQB_TX_BASE + MQB_CLUSTER_NUM_TX_MSGS
} app_pdu_id_et;

#endif /* APP_PDU_IDS_H */

/****************************** END OF FILE *******************************************************/
