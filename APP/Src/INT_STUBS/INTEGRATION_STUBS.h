#ifndef INTEGRATION_STUBS_H
#define INTEGRATION_STUBS_H

#include "STDC.h"
#include "CHKSUM.h"
#include "SYSTICK.h"
#include "DWT.h"
#include "DBG_MGR.h"
#include "TIME.h"
#include "BTN_MGR.h"
#include "ROTARY_MGR.h"
#include "HMI_SH1106.h"
#include "BUZZER.h"
#include "WS2811.h"
#include "NRF24.h"
#include "NVM_GEN2.h"
#include "CTRL_AXIS.h"
#include "WDG_HW_STM32.h"
#include "ESP01.h"
#include "WIFI.h"
#include "TB.h"
#include "TJA1051.h"
#include "MCP2515.h"
#include "RF_MGR.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "CANTP.h"
#include "CANIF.h"
#include "UDS.h"
#include "APP_PDU_IDS.h"
#include "APP_ISO_POSITIONS.h"
#include "APP_NVM_BLOCKS.h"
#include "FAULT_MGR.h"
#include "UDS_config.h"
#include "UDS_CLIENT.h"
#include "DIAG_GW.h"
#include "AUTOCFG.h"
#include "HAL_config.h"   /* HW_VARIANT: what the hardware variant has fitted, and so what is wired below */

/* The diagnostic nodes on CAN2: an ERX at one of the ISO positions of APP_ISO_POSITIONS.h (the 69 from 0x16 to 0x5A, of
   which 25 are used) has the request ID 0x700 plus its position, 0x716-0x75A, and answers on 0x600 plus it, 0x616-0x65A.
   Node number n (what UDS_CLIENT_set_node() and DIAG_GW take) is request ID - 0x716, so node 0 is the first position. */
#define APP_NODE_REQUEST_BASE_ID                ( 0x700u + APP_ISO_POSITION_FIRST )
#define APP_NODE_REQUEST_LAST_ID                ( 0x700u + APP_ISO_POSITION_LAST )
#define APP_NODE_RESPONSE_BASE_ID               ( 0x600u + APP_ISO_POSITION_FIRST )
#define APP_NODE_RESPONSE_LAST_ID               ( 0x600u + APP_ISO_POSITION_LAST )

/* The HUB's CANTP on CAN2 talks to one virtual node on these - outside both ranges, so PDUR can tell a frame the
   gateway is routing from a message CANTP has reassembled for the HUB. Never on the bus. */
#define APP_NODE_HUB_REQUEST_ID                 ( 0x7FEu )
#define APP_NODE_HUB_RESPONSE_ID                ( 0x6FEu )

#define APP_NODE_PART_NUMBER_DID                ( 0xF187u )   /* vehicleManufacturerSparePartNumber - confirm for the node */
#define APP_NODE_PART_NUMBER_MAX_LEN            ( 32u )

/* Largest reply the HUB's CANTP on CAN2 reassembles from a node, for UDS_CLIENT */
#define APP_NODE_RESPONSE_BUFFER_SIZE           ( 128u )

/* The largest request the tester can send the HUB, and the largest response it can send back. A longer First Frame
   is refused by CANTP with FC_OVERLOAD (the ISO-TP maximum on classic CAN is 4095). One buffer, shared by CANTP's
   reassembly and the UDS working buffer - two of this size would not fit in RAM. UDS_tx_confirmation releases it
   once a response has gone out, so a tester must not start a new request before the response to the last one is
   complete. */
#define APP_UDS_BUFFER_SIZE                     ( 2048u )

/* Clock, timing, debug and watchdog */
extern const SYSTICK_cfg_st             systick_cfg_s;
extern const hw_crc_config_st           hw_crc_cfg_s;
extern const DBG_MGR_cfg_st             dbg_mgr_cfg_s;
extern const TIME_cfg_st                time_cfg_s;
extern const WDG_HW_STM32_config_st     wdg_cfg_s;
extern AUTOCFG_init_st                  app_autocfg_init_s;

/* Not fitted on BLUE_PILL: the display, buzzer and LEDs, the onboard button, the radio, and WiFi and cloud */
#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
extern const HMI_SH1106_cfg_st          hmi_sh1106_cfg_s;
extern const BUZZER_func_table_st       buzzer_func_table_s;
extern       BUZZER_instance_st         buzzer_instance_s;
extern       WS2811_instance_st         ws2811_instance_s;
extern const BTN_MGR_func_table_st      btm_mgr_func_table_s[3];
extern       BTN_MGR_control_st         btm_mgr_control_s[3];
extern       BTN_MGR_instance_st        btm_mgr_instance_s;
extern       NRF24_instance_st          nrf24_instance_s;
extern const RF_MGR_cfg_st              rf_mgr_cfg_s;
extern const ESP01_cfg_st               esp01_cfg_s;
extern const WIFI_config_st             wifi_cfg_s;
extern       TB_config_st               tb_cfg_s;
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

/* Storage and messages */
extern const NVM_GEN2_hw_interface_st   nvm_gen2_hw_interface_s;
extern const MSG_SCHED_cfg_st           msg_sched_cfg_s;

/* CAN1 (APP_CAN_CH_MAIN): the tester's bus. CANTP reassembles into the buffer shared with UDS. */
extern const TJA1051_func_st            tja1051_func_s;
extern const TJA1051_config_st          tja1051_cfg_s;
extern       TJA1051_instance_st        tja1051_main_instance_s;
extern       CANTP_instance_st          app_cantp_instance_s;
extern const CANTP_cfg_st               app_cantp_cfg_s;
extern       u8_t                       app_uds_buffer_g[ APP_UDS_BUFFER_SIZE ];

/* CAN2 (APP_CAN_CH_CAN2): the nodes' bus. The HUB's own CANTP instance and reassembly buffer for UDS_CLIENT. */
extern const MCP2515_func_table_st      mcp2515_can2_func_s;
extern const MCP2515_cfg_st             mcp2515_can2_cfg_s;
extern       MCP2515_instance_st        mcp2515_can2_instance_s;
extern       CANTP_instance_st          app_cantp2_instance_s;
extern const CANTP_cfg_st               app_cantp2_cfg_s;
extern       u8_t                       app_cantp2_rx_buffer_g[ APP_NODE_RESPONSE_BUFFER_SIZE ];

/* CAN stack, diagnostics and gateway */
extern const CANIF_channel_cfg_st       canif_channels_s[APP_CAN_NUM_CHANNELS];
extern const CANIF_tx_pdu_st            canif_tx_pdus_s[];
extern const u16_t                      canif_num_tx_pdus_s;
extern const PDUR_route_st              pdur_routing_table_s[];
extern const u16_t                      pdur_num_routes_s;
extern const UDS_CLIENT_cfg_st          app_uds_client_cfg_s;
extern const DIAG_GW_cfg_st             app_diag_gw_cfg_s;

#endif /* INTEGRATION_STUBS_H */

/****************************** END OF FILE *******************************************************/
