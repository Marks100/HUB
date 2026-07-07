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
#include "BUZZER.h"
#include "WS2811.h"
#include "ST7567.h"
#include "NRF24.h"
#include "NVM.h"
#include "CTRL_AXIS.h"
#include "WDG_HW_STM32.h"
#include "ESP01.h"
#include "WIFI.h"
#include "TB.h"
#include "TJA1051.h"
#include "RF_MGR.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "CPS.h"

extern const SYSTICK_cfg_st          systick_cfg_s;
extern const hw_crc_config_st        hw_crc_cfg_s;
extern const DBG_MGR_cfg_st          dbg_mgr_cfg_s;
extern const TIME_cfg_st             time_cfg_s;
extern const BTN_MGR_func_table_st   btm_mgr_func_table_s[3];
extern const ROTARY_MGR_func_p_st    rotary_mgr_func_table_s;
extern const BUZZER_func_table_st    buzzer_func_table_s;
extern       BUZZER_instance_st      buzzer_instance_s;
extern       WS2811_instance_st      ws2811_instance_s;
extern const ST7567_func_table_st    st7567_func_table_s;
extern       NRF24_instance_st       nrf24_instance_s;
extern const NVM_hw_interface_st     nvm_hw_interface_s;
extern const NVM_func_p_st           nvm_persist_block_s;
extern const WDG_HW_STM32_config_st  wdg_cfg_s;
extern const ESP01_cfg_st            esp01_cfg_s;
extern const WIFI_config_st          wifi_cfg_s;
extern       TB_config_st            tb_cfg_s;
extern const TJA1051_func_st         tja1051_func_s;
extern const TJA1051_config_st       tja1051_cfg_s;
extern const RF_MGR_cfg_st           rf_mgr_cfg_s;
extern const PDUR_rx_route_st        pdur_routing_table_s[];
extern const u16_t                   pdur_num_routes_s;
extern const MSG_SCHED_cfg_st        msg_sched_cfg_s;
extern       CPS_instance_st         cps_instance_s;
extern const CPS_cfg_st              cps_cfg_s;

void esp01_uart_byte_rx( u8_t byte );
void esp01_check_rx_timeout( void );

#endif /* INTEGRATION_STUBS_H */
