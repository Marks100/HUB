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
/* Must match the rows of can_msg_table_s (INTEGRATION_STUBS.c) - the enum below needs a plain integer */
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
   APP_CAN_CH_CAN2,             /* MCP2515 on SPI1 - the node's diagnostic bus, see INTEGRATION_STUBS.c */
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
   APP_PDU_NODE_DIAG,           /* CAN2, UDS_CLIENT: the HUB's own virtual node, see APP_NODE_HUB_*_ID */
   APP_PDU_GW_TESTER,           /* CAN1, DIAG_GW: every node's request ID (0x718-0x799) from the tester */
   APP_PDU_GW_NODE,             /* CAN2, DIAG_GW: every node's response ID (0x618-0x699) */
   APP_PDU_GKT_LED_TX,          /* tx-only: GKT_SHIFTER_LED_CAN_ID (0x3FD), sent by GKT_SHIFTER_tick() */
   APP_PDU_GKT_BACKLIGHT_TX,    /* tx-only: GKT_SHIFTER_BACKLIGHT_CAN_ID (0x202), sent by GKT_SHIFTER_tick() */
   APP_PDU_TCU_GEAR_STATE_TX,   /* tx-only: TCU_GEAR_STATE_CAN_ID, sent by TCU_tick() */
   APP_PDU_TCU_DYNAMICS_TX,     /* tx-only: TCU_DYNAMICS_CAN_ID (RPM + speed + requested throttle), sent by TCU_tick() */
   APP_PDU_MRA_TX_BASE,         /* tx-only: MRA_CLUSTER_NUM_TX_MSGS routes, one per MRA_CLUSTER_tx_msg_et, in that order */
   APP_PDU_MQB_TX_BASE          = APP_PDU_MRA_TX_BASE + MRA_CLUSTER_NUM_TX_MSGS,   /* tx-only: MQB_CLUSTER_NUM_TX_MSGS routes, one per MQB_CLUSTER_tx_msg_et, in that order */
   APP_PDU_NUM_ROUTES           = APP_PDU_MQB_TX_BASE + MQB_CLUSTER_NUM_TX_MSGS
} app_pdu_id_et;

/* CANIF's own PDU IDs - the ones that are not PDUR routes: the N-PDUs CANTP knows its frames by, and the
   transmit PDUs the gateway sends on. Plain-frame routes use their PDUR route ID as their CANIF transmit
   PDU ID. Kept above every route ID, so the two never collide in CANIF's tables. */
typedef enum
{
   APP_CANIF_PDU_BASE           = 0x100u,
   APP_CANIF_PDU_UDS_FUNC_RX    = APP_CANIF_PDU_BASE,   /* CAN1, CANTP: functional request (APP_UDS_REQUEST_ID) */
   APP_CANIF_PDU_UDS_FUNC_TX,                           /* CAN1, CANTP: its response (APP_UDS_RESPONSE_ID) */
   APP_CANIF_PDU_UDS_PHYS_RX,                           /* CAN1, CANTP: physical request (APP_CAN_RX_ID) */
   APP_CANIF_PDU_UDS_PHYS_TX,                           /* CAN1, CANTP: its response (APP_CAN_TX_ID) */
   APP_CANIF_PDU_HUB_RX,                                /* CAN2, CANTP: a node's reply, presented as the virtual node's */
   APP_CANIF_PDU_HUB_TX,                                /* CAN2, CANTP: HUB requests and Flow Controls - any node request ID */
   APP_CANIF_PDU_GW_TESTER_TX,                          /* CAN1, DIAG_GW: every node response ID, to the tester */
   APP_CANIF_PDU_GW_NODE_TX                             /* CAN2, DIAG_GW: every node request ID, to the nodes */
} app_canif_pdu_id_et;

#endif /* APP_PDU_IDS_H */

/****************************** END OF FILE *******************************************************/
