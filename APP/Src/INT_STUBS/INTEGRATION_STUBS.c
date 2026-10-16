/***************************************************************************************************
**                              INTEGRATION_STUBS                                                 **
**  Board/peripheral wiring. The vehicle simulation config is in VEHICLE_SIM_config.c.             **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
#include "PROJ_config.h"
#include "APP_PDU_IDS.h"
#include "VEHICLE_SIM_config.h"
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "RF_MGR.h"
#include "RF_MGR_NRF24.h"
#include "SENSOR_NET.h"
#include "WIFI.h"
#include "GKT_SHIFTER.h"
#include "HAL_TIM.h"
#include "HAL_SPI.h"
#include "HAL_I2C.h"
#include "HAL_UART.h"
#include "SCHED_config.h"
#include "RF_MGR.h"
#include "APP_NVM_BLOCKS.h"
#include "FLS_STM32F1.h"
#include "nvic_driver.h"
#include "TB_CBK.h"
#include "TJA1051.h"
#include "HMI_SH1106.h"
#include "MENU_NAV.h"
#include "HEADER.h"
#include "CANTP.h"
#include "UDS.h"

/***************************************************************************************************
**                              SYSTICK                                                           **
***************************************************************************************************/
const SYSTICK_cfg_st systick_cfg_s =
{
    .timeout_ms     = SCHED_TICK_RATE_MSECS,
    .systick_func_p = SCHED_tick
};

/***************************************************************************************************
**                              CHKSUM                                                            **
***************************************************************************************************/
const hw_crc_config_st hw_crc_cfg_s =
{
    .width             = HW_CRC_WIDTH_32BIT,
    .polynomial        = 0x04C11DB7UL,
    .seed              = 0xFFFFFFFFUL,
    .complement_result = TRUE
};

/***************************************************************************************************
**                              DBG_MGR                                                           **
***************************************************************************************************/
const DBG_MGR_cfg_st dbg_mgr_cfg_s =
{
    .mode_cycle_time    = SCHED_TICK_RATE_MSECS,
    .dwt_get_cnt_func_p = DWT_get_count
};

/***************************************************************************************************
**                              TIME                                                              **
***************************************************************************************************/
const TIME_cfg_st time_cfg_s =
{
    .time_increment_ms = SCHED_PERIOD_10MS,   /* TIME_increment_time() row in SCHED_config.c */
};

/***************************************************************************************************
**                              BTN_MGR                                                           **
***************************************************************************************************/
STATIC void onboard_btn_short_press( void )
{
    WIFI_restart();
}

const BTN_MGR_func_table_st btm_mgr_func_table_s[3] =
{
    { "ONBOARD", TRUE,  HAL_BRD_read_onboard_btn, onboard_btn_short_press, NULL_P },
};

/* Separate instance from the HMI panel's, so the two button sets can't form combinations with each
   other. Storage is here because BTN_MGR allocates none. */
BTN_MGR_control_st  btm_mgr_control_s[BTM_MGR_FUNC_TABLE_SIZE( btm_mgr_func_table_s )];
BTN_MGR_instance_st btm_mgr_instance_s;

/***************************************************************************************************
**                              HMI_SH1106 (OLED + encoder + buttons panel)                       **
***************************************************************************************************/
/* TIM4's encoder belongs to this panel - no standalone ROTARY_MGR instance, two on the same counter
   would both fire on every detent */
const HMI_SH1106_cfg_st hmi_sh1106_cfg_s =
{
    .i2c_write_func_p       = HAL_I2C_write_registers,
    .i2c_address            = SH1106_I2C_ADDR_DEFAULT,
    .contrast               = SH1106_DEFAULT_CONTRAST,
    /* Mounted rotated 180 degrees - both axes together, not one (that would mirror) */
    .flip_horizontal        = TRUE,
    .flip_vertical          = TRUE,

    .enc_mode               = ROTARY_MODE_HARDWARE_TIMER,
    .enc_get_count_func_p   = HAL_TIM_ENC_get_counter,
    .enc_cnt_max            = 0xFFFFu,
    .enc_reverse_direction  = FALSE,
    .enc_counts_per_detent  = 4u,           /* TIM4 decodes x4 */

    .btn_read_select_func_p  = HAL_BRD_read_panel_select_btn,
    .btn_read_confirm_func_p = HAL_BRD_read_panel_confirm_btn,
    .btn_read_back_func_p    = HAL_BRD_read_panel_back_btn,
    .btn_inverted            = TRUE,

    .tick_rate_ms           = SCHED_PERIOD_10MS,
    .refresh_period_ms      = 50,

    /* HMI_SH1106_tick() runs in the SysTick ISR */
    .critical_enter_func_p = NVIC_DisableGlobalIRQ,
    .critical_exit_func_p  = NVIC_EnableGlobalIRQ,

    .buzzer_p               = &buzzer_instance_s,
    .beep_on_rotate         = TRUE,
    .beep_on_press          = TRUE,
    .beep_ms                = BUZZER_SHORT_BEEP_MS,

    .on_input_func_p        = MENU_NAV_on_input,
    .draw_func_p            = MENU_NAV_draw,
};

/***************************************************************************************************
**                              BUZZER                                                            **
***************************************************************************************************/
const BUZZER_func_table_st buzzer_func_table_s =
{
    .buzzer_start_func_p = HAL_TIM3_start,
    .buzzer_stop_func_p  = HAL_TIM3_stop,
};

BUZZER_instance_st buzzer_instance_s;

/***************************************************************************************************
**                              WS2811                                                            **
***************************************************************************************************/
static u32_t ws2811_leds_s[3u];

WS2811_instance_st ws2811_instance_s =
{
    .led_array_p        = ws2811_leds_s,
    .num_leds           = 3u,
    .brightness         = WS2811_BRIGHTNESS_DEFAULT,
    .disable_irq_func_p = NVIC_DisableGlobalIRQ,
    .enable_irq_func_p  = NVIC_EnableGlobalIRQ,
};

/***************************************************************************************************
**                              NRF24                                                             **
***************************************************************************************************/
NRF24_instance_st nrf24_instance_s =
{
    .ce_pin_func_p         = HAL_BRD_NRF24_set_ce_pin_state,
    .cs_pin_func_p         = HAL_BRD_NRF24_spi_slave_select,
    .spi_func_p            = HAL_SPI1_write_and_read_data,
    .us_delay_func_p       = DWT_delay_us,
    .packet_tx_conf_func_p = RF_MGR_tx_complete,
    .packet_rx_func_p      = RF_MGR_frame_received
};

/***************************************************************************************************
**                              NVM                                                               **
***************************************************************************************************/
/* FBL and APP share these two pages - NVM_GEN2 keys records by block ID, and each image owns its own
   ID range, so neither can overwrite the other */
const NVM_GEN2_hw_interface_st nvm_gen2_hw_interface_s =
{
    .init_func          = FLS_STM32F1_init,
    .base_address       = NVM_BASE_ADDRESS,
    .partition_size     = FLS_STM32F1_PAGE_SIZE,
    .total_size         = NVM_TOTAL_SIZE,
    .erase_func         = FLS_STM32F1_erase_sector,
    .write_func         = FLS_STM32F1_write_data,
    .compare_func       = FLS_STM32F1_compare_data,
    .read_func          = FLS_STM32F1_recover_data
};

/***************************************************************************************************
**                              WDG                                                               **
***************************************************************************************************/
const WDG_HW_STM32_config_st wdg_cfg_s =
{
    .timeout_ms = 5000u
};

/***************************************************************************************************
**                              ESP01                                                            **
***************************************************************************************************/
/* Must be more than 1 tick - the timestamp can be a tick stale, so exactly 1 tick can fire with
   almost no real silence. The +1 guarantees a full tick of gap before a frame counts as complete. */
#define ESP01_INTER_BYTE_TIMEOUT_MS     ( SCHED_PERIOD_10MS + 1u )

const ESP01_cfg_st esp01_cfg_s =
{
    .initial_mode                 = ESP01_STA,
    .uart_tx_func_p               = HAL_USART2_send_data,
    .network_packet_rx_callback_p = WIFI_esp01_rx_handler,
    .inter_byte_timeout_ms        = ESP01_INTER_BYTE_TIMEOUT_MS,
};

/***************************************************************************************************
**                              WIFI                                                             **
***************************************************************************************************/
static void on_wifi_send_complete( pass_fail_et result )
{
    if( result == PASS )
    {
        TB_notify_event( TB_EVENT_SEND_COMPLETE );
    }
    else
    {
        /* A failed send means the link is already dead - take the connection-lost path so TB stops
           sending, rather than waiting for the 90s staleness timeout */
        TB_notify_event( TB_EVENT_CONNECTION_LOST );
    }
}

const WIFI_config_st wifi_cfg_s =
{
    .ssid                     = (const u8_t*)"BTHub6-TFH6",
    .password                 = (const u8_t*)"4YEWArmQiDHL",
    .ap_ssid                  = (const u8_t*)"HUB_AP",
    .ap_password              = (const u8_t*)"hubpassword",
    .ap_channel               = 6u,
    .ap_encryption            = 3u,   /* WPA2_PSK */
    .start_in_ap_mode         = ( WIFI_AP_BENCH_ENABLED == 1u ) ? TRUE : FALSE,
    .rx_callback_p            = tb_mqtt_rx_handler,
    .send_complete_callback_p = on_wifi_send_complete,
    .rssi_polling_enabled     = TRUE,
};

/***************************************************************************************************
**                              TB (ThingsBoard)                                                 **
***************************************************************************************************/
TB_config_st tb_cfg_s =
{
    .broker           = "thingsboard.cloud",
    .port             = 1883u,
    .client_id        = "HUB_DEVICE",
    .token            = "wHngu0Owdj1xt4pfhFdS",
    .send_func_p      = WIFI_send,
    .telemetry_topics = tb_telemetry_topics_s,
    .num_telemetry    = 0u,   /* set by TB_CBK_init() */
    .rpc_handlers     = tb_rpc_handlers_s,
    .num_rpc_handlers = 0u,   /* set by TB_CBK_init() */
};

/***************************************************************************************************
**                              TJA1051 CAN transceiver                                          **
***************************************************************************************************/
const TJA1051_func_st tja1051_func_s =
{
    .en_pin_set = HAL_BRD_TJA1051_set_en_pin,
    .s_pin_set  = NULL_P
};

const TJA1051_config_st tja1051_cfg_s =
{
    .initial_mode   = TJA1051_MODE_NORMAL,
    .event_callback = NULL_P
};

TJA1051_instance_st tja1051_main_instance_s;

/***************************************************************************************************
**                              RF_MGR                                                            **
***************************************************************************************************/
STATIC const RF_MGR_NRF24_chip_st rf_mgr_nrf24_chip_s =
{
    .nrf24_p = &nrf24_instance_s,
    .config  = NRF24_HUB_CONFIG,
};

const RF_MGR_cfg_st rf_mgr_cfg_s =
{
    .driver_p        = &RF_MGR_NRF24_driver,
    .chip_p          = &rf_mgr_nrf24_chip_s,
    .tx_timeout_ms   = 0u,
    .activity_func_p = HAL_BRD_toggle_onboard_led,
};

/***************************************************************************************************
**                              CAN / PDUR / MSG_SCHED                                           **
***************************************************************************************************/
#define CAN_SENSOR_BASE_ID  ( 0x100u )   /* slot N uses ID base + N */

/* One list drives both the PDUR routes and the MSG_SCHED table, so they can't drift apart. Keep the
   count equal to APP_NUM_SENSOR_SLOTS (APP_PDU_IDS.h). */
#define APP_SENSOR_SLOTS( ENTRY ) \
    ENTRY(0u)  ENTRY(1u)  ENTRY(2u)  ENTRY(3u)  \
    ENTRY(4u)  ENTRY(5u)  ENTRY(6u)  ENTRY(7u)  \
    ENTRY(8u)  ENTRY(9u)  ENTRY(10u) ENTRY(11u)

#define CAN_SENSOR_PDUR_ENTRY( n ) \
    [APP_PDU_SENSOR_BASE + (n)] = { .tx_id = ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), .lower_layer_tx_func = CANIF_pdur_normal_tx },

#define APP_MQB_PDUR_ENTRY( msg, can_id ) \
    [APP_PDU_MQB_TX_BASE + (msg)] = { .tx_id = (can_id), .lower_layer_tx_func = CANIF_pdur_normal_tx },

/* Same IDs as FBL - safe to reuse, BM only ever runs one of APP/FBL at a time */
#define APP_UDS_REQUEST_ID   ( 0x700u )
#define APP_UDS_RESPONSE_ID  ( 0x600u )
#define APP_CAN_RX_ID        ( 0x7E0u )
#define APP_CAN_TX_ID        ( 0x7E8u )

false_true_et app_uds_is_functional_route( UDS_route_id_t route_id )
{
    return( ( route_id == (UDS_route_id_t)APP_PDU_UDS_FUNCTIONAL ) ? TRUE : FALSE );
}

/* Not initialised on purpose: any non-zero initializer would store the whole ~1.7KB (mostly zero)
   struct in flash .data. Configured at runtime by app_cantp_instance_init() instead. */
CANTP_instance_st app_cantp_instance_s;

/* Shared by CANTP reassembly and UDS responses */
u8_t app_uds_buffer_g[ APP_UDS_BUFFER_SIZE ];

void app_cantp_instance_init( void )
{
    app_cantp_instance_s.channel              = APP_CAN_CH_MAIN;
    app_cantp_instance_s.rx_indication_func_p = PDUR_rx_indication_physical;
    app_cantp_instance_s.can_tx_func_p        = CANIF_transmit;
    app_cantp_instance_s.rx_buffer_p          = app_uds_buffer_g;
    app_cantp_instance_s.rx_buffer_size       = APP_UDS_BUFFER_SIZE;
    app_cantp_instance_s.tp_ids[0].req_id     = APP_UDS_REQUEST_ID;
    app_cantp_instance_s.tp_ids[0].resp_id    = APP_UDS_RESPONSE_ID;
    app_cantp_instance_s.tp_ids[1].req_id     = APP_CAN_RX_ID;
    app_cantp_instance_s.tp_ids[1].resp_id    = APP_CAN_TX_ID;
    app_cantp_instance_s.st_min               = 10u;
    app_cantp_instance_s.rx_block_size        = 10u;
    app_cantp_instance_s.N_Cr                 = CANTP_DEFAULT_N_CR;
    app_cantp_instance_s.N_Bs                 = CANTP_DEFAULT_N_BS;
}

/***************************************************************************************************
**                              CANIF                                                             **
***************************************************************************************************/
/* Row index = channel number. A second bus is one more row. */
const CANIF_channel_cfg_st canif_channels_s[APP_CAN_NUM_CHANNELS] =
{
    [APP_CAN_CH_MAIN] = 
    { 
        .send_frame      = HAL_CAN_send_frame,
        .ctrl_set_mode   = HAL_CAN_set_mode,
        .ctrl_get_status = HAL_CAN_get_status,
        .trcv_set_mode   = TJA1051_canif_set_mode,
        .trcv_inst_p     = &tja1051_main_instance_s,
        .cantp_p         = &app_cantp_instance_s 
    },
};

/* All routes are on APP_CAN_CH_MAIN (.channel left at 0) */
const PDUR_route_st pdur_routing_table_s[] =
{
    //APP_SENSOR_SLOTS( CAN_SENSOR_PDUR_ENTRY )

    /* Shifter frames go to GKT_SHIFTER's gateway, never straight to TCU */
    [APP_PDU_GKT_LEVER_RX] = { .rx_id = GKT_SHIFTER_STATE_CAN_ID, .upperLayerRxIndication = app_pdur_gkt_lever_rx },
    [APP_PDU_GKT_HEARTBEAT_RX] = { .rx_id = GKT_SHIFTER_HEARTBEAT_CAN_ID, .upperLayerRxIndication = app_pdur_gkt_heartbeat_rx },

    /* UDS_tx_confirmation releases the shared buffer once a response has actually gone out */
    [APP_PDU_UDS_FUNCTIONAL] = 
    { 
        .rx_id = APP_UDS_REQUEST_ID, 
        .tx_id = APP_UDS_RESPONSE_ID,
        .upperLayerRxIndication = UDS_rx_indication,
        .lower_layer_tx_func = CANIF_pdur_tp_tx,
        .txConfirmation = UDS_tx_confirmation 
    },
    [APP_PDU_UDS_PHYSICAL] = 
    { 
        .rx_id = APP_CAN_RX_ID,
        .tx_id = APP_CAN_TX_ID,
        .upperLayerRxIndication = UDS_rx_indication,
        .lower_layer_tx_func = CANIF_pdur_tp_tx,
        .txConfirmation = UDS_tx_confirmation
    },

    /* TX-only routes go through CANTP's NORMAL queue - together they send more frames per tick than
       the CAN controller has TX mailboxes */
    [APP_PDU_GKT_LED_TX]        = { .tx_id = GKT_SHIFTER_LED_CAN_ID,       .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_GKT_BACKLIGHT_TX]  = { .tx_id = GKT_SHIFTER_BACKLIGHT_CAN_ID, .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_TCU_GEAR_STATE_TX] = { .tx_id = TCU_GEAR_STATE_CAN_ID,        .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_TCU_DYNAMICS_TX]   = { .tx_id = TCU_DYNAMICS_CAN_ID,          .lower_layer_tx_func = CANIF_pdur_normal_tx },

    /* MRA_CLUSTER - frames CarCluster sends on its second bus ([CAN2] in MRA_CLUSTER.h) also go out
       on the main bus here */
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_IGNITION]       = { .tx_id = MRA_CLUSTER_IGNITION_CAN_ID,       .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_KEY_STATUS]     = { .tx_id = MRA_CLUSTER_KEY_STATUS_CAN_ID,     .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DOOR_STATUS]    = { .tx_id = MRA_CLUSTER_DOOR_STATUS_CAN_ID,    .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_RPM]            = { .tx_id = MRA_CLUSTER_RPM_CAN_ID,            .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SPEED]          = { .tx_id = MRA_CLUSTER_SPEED_CAN_ID,          .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRANS1]         = { .tx_id = MRA_CLUSTER_TRANS1_CAN_ID,         .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_GEARBOX_ALIVE]  = { .tx_id = MRA_CLUSTER_GEARBOX_ALIVE_CAN_ID,  .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DRIVE_MODE]     = { .tx_id = MRA_CLUSTER_DRIVE_MODE_CAN_ID,     .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_COOLANT_TEMP]   = { .tx_id = MRA_CLUSTER_COOLANT_TEMP_CAN_ID,   .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ABS]            = { .tx_id = MRA_CLUSTER_ABS_CAN_ID,            .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS]         = { .tx_id = MRA_CLUSTER_LIGHTS_CAN_ID,         .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS2]        = { .tx_id = MRA_CLUSTER_LIGHTS2_CAN_ID,        .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BLINKERS]       = { .tx_id = MRA_CLUSTER_BLINKERS_CAN_ID,       .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_FUEL_LEVEL]     = { .tx_id = MRA_CLUSTER_FUEL_LEVEL_CAN_ID,     .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRACTION]       = { .tx_id = MRA_CLUSTER_TRACTION_CAN_ID,       .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_AIRBAG]         = { .tx_id = MRA_CLUSTER_AIRBAG_CAN_ID,         .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DISTRONIC]      = { .tx_id = MRA_CLUSTER_DISTRONIC_CAN_ID,      .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ENGINE_WARN]    = { .tx_id = MRA_CLUSTER_ENGINE_WARN_CAN_ID,    .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_POWER_STEERING] = { .tx_id = MRA_CLUSTER_POWER_STEERING_CAN_ID, .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TPMS]           = { .tx_id = MRA_CLUSTER_TPMS_CAN_ID,           .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ASSIST_ERRORS]  = { .tx_id = MRA_CLUSTER_ASSIST_ERRORS_CAN_ID,  .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SAFETY_ERRORS]  = { .tx_id = MRA_CLUSTER_SAFETY_ERRORS_CAN_ID,  .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BRAKE_FLUID]    = { .tx_id = MRA_CLUSTER_BRAKE_FLUID_CAN_ID,    .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_STEERING_BTNS]  = { .tx_id = MRA_CLUSTER_STEERING_BTNS_CAN_ID,  .lower_layer_tx_func = CANIF_pdur_normal_tx },

    //MQB_CLUSTER_TX_MSG_LIST( APP_MQB_PDUR_ENTRY )
};

const u16_t pdur_num_routes_s = (u16_t)( sizeof(pdur_routing_table_s) / sizeof(pdur_routing_table_s[0u]) );

STATIC void can_sensor_get_data( u8_t msg_idx, u8_t* buf_p, u8_t* len_p )
{
    const SENSOR_NET_sensor_data_st* db_p = SENSOR_NET_get_sensor_db();

    buf_p[0u] = (u8_t)( (u16_t)db_p[msg_idx].temperature_centidegC >> 8u );
    buf_p[1u] = (u8_t)(  db_p[msg_idx].temperature_centidegC        & 0xFFu );
    buf_p[2u] = (u8_t)( (u16_t)db_p[msg_idx].humidity_tenths_pct   >> 8u );
    buf_p[3u] = (u8_t)(  db_p[msg_idx].humidity_tenths_pct          & 0xFFu );
    buf_p[4u] = (u8_t)(  db_p[msg_idx].battery_voltage_mv           >> 8u );
    buf_p[5u] = (u8_t)(  db_p[msg_idx].battery_voltage_mv           & 0xFFu );
    buf_p[6u] = (u8_t)db_p[msg_idx].comms_lost;
    *len_p    = 7u;
}

/* MSG_SCHED sends with the PDUR id (APP_PDU_SENSOR_BASE + n), not the CAN id the route carries */
#define CAN_SENSOR_MSG_ENTRY( n ) \
    { ( APP_PDU_SENSOR_BASE + (u32_t)(n) ), 0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },

STATIC const MSG_SCHED_msg_cfg_st can_msg_table_s[] =
{
    APP_SENSOR_SLOTS( CAN_SENSOR_MSG_ENTRY )
};

const MSG_SCHED_cfg_st msg_sched_cfg_s =
{
    .msg_table      = can_msg_table_s,
    .num_msgs       = (u8_t)( sizeof(can_msg_table_s) / sizeof(can_msg_table_s[0u]) ),
};

/****************************** END OF FILE *******************************************************/
