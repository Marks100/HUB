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
#include "RF_MGR.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "CANTP.h"
#include "CANIF.h"
#include "UDS.h"
#include "APP_PDU_IDS.h"

extern const SYSTICK_cfg_st          systick_cfg_s;
extern const hw_crc_config_st        hw_crc_cfg_s;
extern const DBG_MGR_cfg_st          dbg_mgr_cfg_s;
extern const TIME_cfg_st             time_cfg_s;
extern const BTN_MGR_func_table_st   btm_mgr_func_table_s[3];
extern       BTN_MGR_control_st      btm_mgr_control_s[3];
extern       BTN_MGR_instance_st     btm_mgr_instance_s;
extern const HMI_SH1106_cfg_st       hmi_sh1106_cfg_s;
extern const BUZZER_func_table_st    buzzer_func_table_s;
extern       BUZZER_instance_st      buzzer_instance_s;
extern       WS2811_instance_st      ws2811_instance_s;
extern       NRF24_instance_st       nrf24_instance_s;
extern const NVM_GEN2_hw_interface_st nvm_gen2_hw_interface_s;
extern const WDG_HW_STM32_config_st  wdg_cfg_s;
extern const ESP01_cfg_st            esp01_cfg_s;
extern const WIFI_config_st          wifi_cfg_s;
extern       TB_config_st            tb_cfg_s;
extern const TJA1051_func_st         tja1051_func_s;
extern const TJA1051_config_st       tja1051_cfg_s;
extern       TJA1051_instance_st      tja1051_main_instance_s;
extern const RF_MGR_cfg_st           rf_mgr_cfg_s;
extern const PDUR_route_st           pdur_routing_table_s[];
extern const u16_t                   pdur_num_routes_s;
extern       CANTP_instance_st        app_cantp_instance_s;
extern const CANIF_channel_cfg_st     canif_channels_s[APP_CAN_NUM_CHANNELS];

/* CANTP's reassembly buffer and UDS's working buffer, shared. Sized for APP's largest message: a
   0x19 ReadDTCInformation report of every fault (2 + 1 + 4 x FAULT_MGR_MAX_FAULTS = 259 bytes) - the
   largest request, a 0x2E key write, is 3 + APP_KEY_LEN = 131 bytes. Rounded up for headroom. */
#define APP_UDS_BUFFER_SIZE                  ( 512u )
extern       u8_t                     app_uds_buffer_g[ APP_UDS_BUFFER_SIZE ];
void app_cantp_instance_init( void );
false_true_et app_uds_is_functional_route( UDS_route_id_t route_id );
extern const MSG_SCHED_cfg_st        msg_sched_cfg_s;

#endif /* INTEGRATION_STUBS_H */
