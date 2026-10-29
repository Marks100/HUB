/***************************************************************************************************
**                              INTEGRATION_STUBS                                                 **
**  Board/peripheral wiring. The vehicle simulation config is in VEHICLE_SIM_config.c.             **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
#include "PROJ_config.h"
#include "SCHED_config.h"
#include "VEHICLE_SIM_config.h"
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "HAL_TIM.h"
#include "HAL_SPI.h"
#include "HAL_I2C.h"
#include "HAL_UART.h"
#include "nvic_driver.h"
#include "FLS_STM32F1.h"
#include "RF_MGR_NRF24.h"
#include "SENSOR_NET.h"
#include "AP_MSG.h"
#include "GKT_SHIFTER.h"
#include "TB_CBK.h"
#include "MENU_NAV.h"
#include "HEADER.h"
#include "TIME_MGR.h"

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
    .time_increment_ms = SCHED_PERIOD_10MS   /* the TIME_increment_time() row in SCHED_config.c */
};

/***************************************************************************************************
**                              BTN_MGR                                                           **
***************************************************************************************************/
const BTN_MGR_func_table_st btm_mgr_func_table_s[3] =
{
    { "ONBOARD", TRUE, HAL_BRD_read_onboard_btn, WIFI_restart, NULL_P },
};

/* Separate instance from the HMI panel's, so the two button sets can't form combinations with each
   other. Storage is here because BTN_MGR allocates none. */
BTN_MGR_control_st  btm_mgr_control_s[BTM_MGR_FUNC_TABLE_SIZE( btm_mgr_func_table_s )];
BTN_MGR_instance_st btm_mgr_instance_s;

/***************************************************************************************************
**                              HMI_SH1106 (OLED + encoder + buttons panel)                       **
***************************************************************************************************/
/* TIM4's encoder belongs to this panel - a second ROTARY_MGR instance on it would fire on every detent */
const HMI_SH1106_cfg_st hmi_sh1106_cfg_s =
{
    .i2c_write_func_p        = HAL_I2C_write_registers,
    .i2c_address             = SH1106_I2C_ADDR_DEFAULT,
    .contrast                = SH1106_DEFAULT_CONTRAST,
    /* Mounted rotated 180 degrees - both axes together, not one (that would mirror) */
    .flip_horizontal         = TRUE,
    .flip_vertical           = TRUE,

    .enc_mode                = ROTARY_MODE_HARDWARE_TIMER,
    .enc_get_count_func_p    = HAL_TIM_ENC_get_counter,
    .enc_cnt_max             = 0xFFFFu,
    .enc_reverse_direction   = FALSE,
    .enc_counts_per_detent   = 4u,   /* TIM4 decodes x4 */

    .btn_read_select_func_p  = HAL_BRD_read_panel_select_btn,
    .btn_read_confirm_func_p = HAL_BRD_read_panel_confirm_btn,
    .btn_read_back_func_p    = HAL_BRD_read_panel_back_btn,
    .btn_inverted            = TRUE,

    .tick_rate_ms            = SCHED_PERIOD_10MS,
    .refresh_period_ms       = 50u,

    /* HMI_SH1106_tick() runs in the SysTick ISR */
    .critical_enter_func_p   = NVIC_DisableGlobalIRQ,
    .critical_exit_func_p    = NVIC_EnableGlobalIRQ,

    .buzzer_p                = &buzzer_instance_s,
    .beep_on_rotate          = TRUE,
    .beep_on_press           = TRUE,
    .beep_ms                 = BUZZER_SHORT_BEEP_MS,

    .on_input_func_p         = MENU_NAV_on_input,
    .draw_func_p             = MENU_NAV_draw
};

/***************************************************************************************************
**                              BUZZER                                                            **
***************************************************************************************************/
const BUZZER_func_table_st buzzer_func_table_s =
{
    .buzzer_start_func_p = HAL_TIM3_start,
    .buzzer_stop_func_p  = HAL_TIM3_stop
};

BUZZER_instance_st buzzer_instance_s;

/***************************************************************************************************
**                              WS2811                                                            **
***************************************************************************************************/
STATIC u32_t ws2811_leds_s[3u];

WS2811_instance_st ws2811_instance_s =
{
    .led_array_p        = ws2811_leds_s,
    .num_leds           = 3u,
    .brightness         = WS2811_BRIGHTNESS_DEFAULT,
    .disable_irq_func_p = NVIC_DisableGlobalIRQ,
    .enable_irq_func_p  = NVIC_EnableGlobalIRQ
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
    .init_func      = FLS_STM32F1_init,
    .base_address   = NVM_BASE_ADDRESS,
    .partition_size = FLS_STM32F1_PAGE_SIZE,
    .total_size     = NVM_TOTAL_SIZE,
    .erase_func     = FLS_STM32F1_erase_sector,
    .write_func     = FLS_STM32F1_write_data,
    .compare_func   = FLS_STM32F1_compare_data,
    .read_func      = FLS_STM32F1_recover_data
};

/***************************************************************************************************
**                              WDG                                                               **
***************************************************************************************************/
const WDG_HW_STM32_config_st wdg_cfg_s =
{
    .timeout_ms = 5000u
};

/***************************************************************************************************
**                              ESP01                                                             **
***************************************************************************************************/
/* Must be more than 1 tick - the timestamp can be a tick stale, so exactly 1 tick can fire with
   almost no real silence. The +1 guarantees a full tick of gap before a frame counts as complete. */
#define ESP01_INTER_BYTE_TIMEOUT_MS     ( SCHED_PERIOD_10MS + 1u )

const ESP01_cfg_st esp01_cfg_s =
{
    .initial_mode                 = ESP01_STA,
    .uart_tx_func_p               = HAL_USART2_send_data,
    .network_packet_rx_callback_p = WIFI_esp01_rx_handler,
    .inter_byte_timeout_ms        = ESP01_INTER_BYTE_TIMEOUT_MS
};

/***************************************************************************************************
**                              WIFI                                                              **
***************************************************************************************************/
STATIC void on_wifi_send_complete( pass_fail_et result )
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
    .ap_ip                    = (const u8_t*)AP_MSG_AP_IP,
    .mode                     = (WIFI_mode_et)WIFI_STARTUP_MODE,
    .client_link_id           = 4u,   /* top of the 0-4 range - server clients take the free links from 0 */
    .rx_callback_p            = tb_mqtt_rx_handler,
    .server_port              = AP_MSG_SERVER_PORT,   /* text sent to it is shown on the Messages screen */
    .server_rx_callback_p     = AP_MSG_on_rx,
    .server_on_station        = ( WIFI_SERVER_ON_STATION == 1u ) ? TRUE : FALSE,
    .send_complete_callback_p = on_wifi_send_complete,
    .rssi_polling_enabled     = TRUE
};

/***************************************************************************************************
**                              TB (ThingsBoard)                                                  **
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
    .num_rpc_handlers = 0u    /* set by TB_CBK_init() */
};

/***************************************************************************************************
**                              TJA1051 CAN transceiver                                           **
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
    .config  = NRF24_HUB_CONFIG
};

const RF_MGR_cfg_st rf_mgr_cfg_s =
{
    .driver_p        = &RF_MGR_NRF24_driver,
    .chip_p          = &rf_mgr_nrf24_chip_s,
    .tx_timeout_ms   = 0u,
    .activity_func_p = HAL_BRD_toggle_onboard_led
};

/***************************************************************************************************
**                              CAN IDs                                                           **
***************************************************************************************************/
/* Same IDs as FBL - safe to reuse, BM only ever runs one of APP/FBL at a time */
#define APP_UDS_REQUEST_ID      ( 0x700u )
#define APP_UDS_RESPONSE_ID     ( 0x600u )
#define APP_CAN_RX_ID           ( 0x7E0u )
#define APP_CAN_TX_ID           ( 0x7E8u )

/***************************************************************************************************
**                              CANTP                                                             **
***************************************************************************************************/
/* Not initialised on purpose: any non-zero initializer would store the whole ~1.7KB (mostly zero)
   struct in flash .data. Configured at runtime by app_cantp_instance_init() instead. */
CANTP_instance_st app_cantp_instance_s;

/* Shared by CANTP reassembly and UDS responses */
u8_t app_uds_buffer_g[ APP_UDS_BUFFER_SIZE ];

/* PDUR's lower-layer TX shape, bound to CAN1's instance */
STATIC pass_fail_et app_cantp_transmit( PDUR_pdu_id_t tx_pdu_id, const u8_t* data_p, u16_t len )
{
    return( CANTP_tx_request( &app_cantp_instance_s, tx_pdu_id, data_p, len ) );
}

void app_cantp_instance_init( void )
{
    app_cantp_instance_s.rx_indication_func_p = PDUR_rx_indication;
    app_cantp_instance_s.tx_confirm_func_p    = PDUR_tx_confirmation;
    app_cantp_instance_s.can_tx_func_p        = CANIF_transmit;
    app_cantp_instance_s.rx_buffer_p          = app_uds_buffer_g;
    app_cantp_instance_s.rx_buffer_size       = APP_UDS_BUFFER_SIZE;

    /* { N-PDU received on, N-PDU sent on, PDUR route a request goes up to, PDUR route a response is sent on } */
    app_cantp_instance_s.tp_ids[0]            = (CANTP_tp_id_pair_st){ APP_CANIF_PDU_UDS_FUNC_RX, APP_CANIF_PDU_UDS_FUNC_TX,
                                                                       APP_PDU_UDS_FUNCTIONAL, APP_PDU_UDS_FUNCTIONAL };
    app_cantp_instance_s.tp_ids[1]            = (CANTP_tp_id_pair_st){ APP_CANIF_PDU_UDS_PHYS_RX, APP_CANIF_PDU_UDS_PHYS_TX,
                                                                       APP_PDU_UDS_PHYSICAL, APP_PDU_UDS_PHYSICAL };
    app_cantp_instance_s.st_min               = 10u;
    app_cantp_instance_s.rx_block_size        = 10u;
    app_cantp_instance_s.N_Cr                 = CANTP_DEFAULT_N_CR;
    app_cantp_instance_s.N_Bs                 = CANTP_DEFAULT_N_BS;
}

/* CAN2: the HUB as client to the nodes, with its own reassembly buffer. It talks to one virtual node (one pair) -
   DIAG_GW hands it the replies on its receive N-PDU, and everything it sends goes down as the one transmit N-PDU */
CANTP_instance_st app_cantp2_instance_s;
u8_t              app_cantp2_rx_buffer_g[ APP_NODE_RESPONSE_BUFFER_SIZE ];

/* The transmit N-PDU covers every node request ID - each frame goes to the selected node's */
STATIC pass_fail_et app_cantp2_can_tx( u16_t npdu_id, const u8_t* data_p, u16_t len )
{
    pass_fail_et result = CANIF_set_dynamic_tx_id( npdu_id, APP_NODE_REQUEST_BASE_ID + UDS_CLIENT_get_node() );

    if( result == PASS )
    {
        result = CANIF_transmit( npdu_id, data_p, len );
    }

    return( result );
}

/* PDUR's lower-layer TX shape, bound to CAN2's instance */
STATIC pass_fail_et app_cantp2_transmit( PDUR_pdu_id_t tx_pdu_id, const u8_t* data_p, u16_t len )
{
    return( CANTP_tx_request( &app_cantp2_instance_s, tx_pdu_id, data_p, len ) );
}

void app_cantp2_instance_init( void )
{
    app_cantp2_instance_s.rx_indication_func_p = PDUR_rx_indication;
    app_cantp2_instance_s.tx_confirm_func_p    = PDUR_tx_confirmation;
    app_cantp2_instance_s.can_tx_func_p        = app_cantp2_can_tx;
    app_cantp2_instance_s.rx_buffer_p          = app_cantp2_rx_buffer_g;
    app_cantp2_instance_s.rx_buffer_size       = APP_NODE_RESPONSE_BUFFER_SIZE;
    app_cantp2_instance_s.tp_ids[0]            = (CANTP_tp_id_pair_st){ APP_CANIF_PDU_HUB_RX, APP_CANIF_PDU_HUB_TX,
                                                                        APP_PDU_NODE_DIAG, APP_PDU_NODE_DIAG };
    app_cantp2_instance_s.st_min               = 10u;
    app_cantp2_instance_s.rx_block_size        = 10u;
    app_cantp2_instance_s.N_Cr                 = CANTP_DEFAULT_N_CR;
    app_cantp2_instance_s.N_Bs                 = CANTP_DEFAULT_N_BS;
}

/***************************************************************************************************
**                              MCP2515 (CAN2)                                                    **
***************************************************************************************************/
/* Shares SPI1 with the NRF24, so only ever ticked from the scheduler. Bit timing is a placeholder until
   fitted: 8 MHz crystal, 500 kbit/s. */
const MCP2515_func_table_st mcp2515_can2_func_s =
{
    .spi_transfer_func_p  = HAL_SPI1_write_and_read_data,
    .cs_set_func_p        = HAL_BRD_MCP2515_spi_slave_select,
    .int_pin_get_func_p   = NULL_P,
    .rx_indication_func_p = CANIF_rx_indication
};

const MCP2515_cfg_st mcp2515_can2_cfg_s =
{
    .cnf1               = 0x00u,
    .cnf2               = 0x91u,
    .cnf3               = 0x02u,
    .initial_mode       = MCP2515_MODE_NORMAL,
    .tx_timeout_retries = 200u,
    .rx_channel         = APP_CAN_CH_CAN2
};

MCP2515_instance_st mcp2515_can2_instance_s;

/***************************************************************************************************
**                              DIAG_GW (CAN1 tester <-> CAN2 node)                               **
***************************************************************************************************/
/* DIAG_GW relays the tester's and the nodes' frames between the two buses. A node's reply to the HUB's own
   request goes to the HUB's CANTP from here rather than through PDUR, because the decision is made before any
   CANTP sees a frame. Everything runs in one scheduler pass. */

/* Whichever node answered, the HUB's CANTP sees the virtual node's receive N-PDU */
STATIC void app_diag_gw_to_hub( const DIAG_GW_frame_st* frame_p )
{
    CANTP_rx_frame_received( &app_cantp2_instance_s, APP_CANIF_PDU_HUB_RX, frame_p->data, frame_p->dlc );
}

/* TEMPORARY bench counters for the gateway - remove them and app_diag_gw_tx() once it is proven */
volatile u32_t app_diag_gw_to_node_count_g   = 0u;
volatile u32_t app_diag_gw_to_tester_count_g = 0u;
volatile u32_t app_diag_gw_tx_fail_count_g   = 0u;

/* CANIF_transmit, counting the frames sent or queued per direction and the ones CANIF refused */
STATIC pass_fail_et app_diag_gw_tx( PDUR_pdu_id_t tx_pdu_id, const u8_t* data_p, u16_t len )
{
    pass_fail_et result = CANIF_transmit( tx_pdu_id, data_p, len );

    if( result != PASS )
    {
        app_diag_gw_tx_fail_count_g++;
    }
    else if( tx_pdu_id == (PDUR_pdu_id_t)APP_CANIF_PDU_GW_NODE_TX )
    {
        app_diag_gw_to_node_count_g++;
    }
    else
    {
        app_diag_gw_to_tester_count_g++;
    }

    return( result );
}

/* The tester owns a node until it has been quiet longer than S3 (5 s) and P2* (5.5 s); the HUB then keeps it for
   at most 1 s per reply. Placeholders until the nodes' own values are known. */
const DIAG_GW_cfg_st app_diag_gw_cfg_s =
{
    .tester_idle_ms   = 5500u,
    .local_ms         = 1000u,
    .time_ms_func_p   = TIME_get_cumulative_run_time_ms,
    .set_tx_id_func_p = CANIF_set_dynamic_tx_id,
    .tx_func_p        = app_diag_gw_tx,
    .to_local_func_p  = app_diag_gw_to_hub,
    .node_tx_pdu_id   = APP_CANIF_PDU_GW_NODE_TX,
    .tester_tx_pdu_id = APP_CANIF_PDU_GW_TESTER_TX,
    .request_base_id  = APP_NODE_REQUEST_BASE_ID,
    .response_base_id = APP_NODE_RESPONSE_BASE_ID
};

/***************************************************************************************************
**                              CANIF                                                             **
***************************************************************************************************/
/* Plain-frame queues, one pair per channel */
STATIC CANIF_channel_state_st canif_main_state_s;
STATIC CANIF_channel_state_st canif_can2_state_s;

STATIC const CANIF_rx_pdu_st canif_main_rx_pdus_s[] =
{
    { APP_UDS_REQUEST_ID,           CANIF_TO_CANTP, APP_CANIF_PDU_UDS_FUNC_RX },
    { APP_CAN_RX_ID,                CANIF_TO_CANTP, APP_CANIF_PDU_UDS_PHYS_RX },
    { GKT_SHIFTER_STATE_CAN_ID,     CANIF_TO_PDUR,  APP_PDU_GKT_LEVER_RX },
    { GKT_SHIFTER_HEARTBEAT_CAN_ID, CANIF_TO_PDUR,  APP_PDU_GKT_HEARTBEAT_RX },
    /* tester -> node, through the gateway */
    { APP_NODE_REQUEST_BASE_ID,     CANIF_TO_PDUR,  APP_PDU_GW_TESTER, APP_NODE_REQUEST_LAST_ID },
};

/* DIAG_GW decides per frame whether a reply is the HUB's (its CANTP) or the tester's (CAN1) */
STATIC const CANIF_rx_pdu_st canif_can2_rx_pdus_s[] =
{
    { APP_NODE_RESPONSE_BASE_ID, CANIF_TO_PDUR, APP_PDU_GW_NODE, APP_NODE_RESPONSE_LAST_ID },
};

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
        .rx_pdus_p       = canif_main_rx_pdus_s,
        .num_rx_pdus     = (u8_t)( sizeof( canif_main_rx_pdus_s ) / sizeof( canif_main_rx_pdus_s[0u] ) ),
        .state_p         = &canif_main_state_s,
        .cantp_p         = &app_cantp_instance_s
    },
    [APP_CAN_CH_CAN2] =
    {
        .send_frame_inst = MCP2515_send_frame,
        .send_inst_p     = &mcp2515_can2_instance_s,
        .rx_pdus_p       = canif_can2_rx_pdus_s,
        .num_rx_pdus     = (u8_t)( sizeof( canif_can2_rx_pdus_s ) / sizeof( canif_can2_rx_pdus_s[0u] ) ),
        .state_p         = &canif_can2_state_s
    }
};

/* The map from PDU ID to the channel and CAN ID it is sent on - the only place a transmit CAN ID is known.
   A plain-frame route's transmit PDU ID is its PDUR route ID. */
const CANIF_tx_pdu_st canif_tx_pdus_s[] =
{
    /* CANTP N-PDUs and the gateway: an entry with a last ID covers every node ID in its range */
    { APP_CANIF_PDU_UDS_FUNC_TX,  APP_CAN_CH_MAIN, 0u, APP_UDS_RESPONSE_ID },
    { APP_CANIF_PDU_UDS_PHYS_TX,  APP_CAN_CH_MAIN, 0u, APP_CAN_TX_ID },
    { APP_CANIF_PDU_HUB_TX,       APP_CAN_CH_CAN2, 0u, APP_NODE_REQUEST_BASE_ID,  APP_NODE_REQUEST_LAST_ID },
    { APP_CANIF_PDU_GW_NODE_TX,   APP_CAN_CH_CAN2, 0u, APP_NODE_REQUEST_BASE_ID,  APP_NODE_REQUEST_LAST_ID },
    { APP_CANIF_PDU_GW_TESTER_TX, APP_CAN_CH_MAIN, 0u, APP_NODE_RESPONSE_BASE_ID, APP_NODE_RESPONSE_LAST_ID },

    /* Plain-frame routes */
    /* Sensor slots, disabled - remove the // to enable. 0x105 clashes with MRA_CLUSTER_RPM_CAN_ID */
    //{ APP_PDU_SENSOR_BASE + 0u,  APP_CAN_CH_MAIN, 0u, 0x100u },
    //{ APP_PDU_SENSOR_BASE + 1u,  APP_CAN_CH_MAIN, 0u, 0x101u },
    //{ APP_PDU_SENSOR_BASE + 2u,  APP_CAN_CH_MAIN, 0u, 0x102u },
    //{ APP_PDU_SENSOR_BASE + 3u,  APP_CAN_CH_MAIN, 0u, 0x103u },
    //{ APP_PDU_SENSOR_BASE + 4u,  APP_CAN_CH_MAIN, 0u, 0x104u },
    //{ APP_PDU_SENSOR_BASE + 5u,  APP_CAN_CH_MAIN, 0u, 0x105u },
    //{ APP_PDU_SENSOR_BASE + 6u,  APP_CAN_CH_MAIN, 0u, 0x106u },
    //{ APP_PDU_SENSOR_BASE + 7u,  APP_CAN_CH_MAIN, 0u, 0x107u },
    //{ APP_PDU_SENSOR_BASE + 8u,  APP_CAN_CH_MAIN, 0u, 0x108u },
    //{ APP_PDU_SENSOR_BASE + 9u,  APP_CAN_CH_MAIN, 0u, 0x109u },
    //{ APP_PDU_SENSOR_BASE + 10u, APP_CAN_CH_MAIN, 0u, 0x10Au },
    //{ APP_PDU_SENSOR_BASE + 11u, APP_CAN_CH_MAIN, 0u, 0x10Bu },
    { APP_PDU_GKT_LED_TX,                                   APP_CAN_CH_MAIN, 0u, GKT_SHIFTER_LED_CAN_ID },
    { APP_PDU_GKT_BACKLIGHT_TX,                             APP_CAN_CH_MAIN, 0u, GKT_SHIFTER_BACKLIGHT_CAN_ID },
    { APP_PDU_TCU_GEAR_STATE_TX,                            APP_CAN_CH_MAIN, 0u, TCU_GEAR_STATE_CAN_ID },
    { APP_PDU_TCU_DYNAMICS_TX,                              APP_CAN_CH_MAIN, 0u, TCU_DYNAMICS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_IGNITION,       APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_IGNITION_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_KEY_STATUS,     APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_KEY_STATUS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DOOR_STATUS,    APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_DOOR_STATUS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_RPM,            APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_RPM_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SPEED,          APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_SPEED_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRANS1,         APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_TRANS1_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_GEARBOX_ALIVE,  APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_GEARBOX_ALIVE_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DRIVE_MODE,     APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_DRIVE_MODE_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_COOLANT_TEMP,   APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_COOLANT_TEMP_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ABS,            APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_ABS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS,         APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_LIGHTS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS2,        APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_LIGHTS2_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BLINKERS,       APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_BLINKERS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_FUEL_LEVEL,     APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_FUEL_LEVEL_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRACTION,       APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_TRACTION_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_AIRBAG,         APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_AIRBAG_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DISTRONIC,      APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_DISTRONIC_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ENGINE_WARN,    APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_ENGINE_WARN_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_POWER_STEERING, APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_POWER_STEERING_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TPMS,           APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_TPMS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ASSIST_ERRORS,  APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_ASSIST_ERRORS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SAFETY_ERRORS,  APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_SAFETY_ERRORS_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BRAKE_FLUID,    APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_BRAKE_FLUID_CAN_ID },
    { APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_STEERING_BTNS,  APP_CAN_CH_MAIN, 0u, MRA_CLUSTER_STEERING_BTNS_CAN_ID },

    /* MQB cluster frames, disabled - remove the // to enable. Same order as MQB_CLUSTER_TX_MSG_LIST */
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ENGINE_RPM,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_ENGINE_RPM_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_SPEED_ODOMETER,    APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_SPEED_ODOMETER_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_COOLANT_TEMP,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_COOLANT_TEMP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_OIL_TEMP,          APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_OIL_TEMP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BOOST_PRESSURE,    APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_BOOST_PRESSURE_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FUEL_LEVEL,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_FUEL_LEVEL_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GEAR_SELECTOR,     APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_GEAR_SELECTOR_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GEAR_DISPLAY,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_GEAR_DISPLAY_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_START_STOP,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_START_STOP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_LIGHTING,          APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_LIGHTING_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FOG_LIGHTS,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_FOG_LIGHTS_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_DOOR_STATUS,       APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_DOOR_STATUS_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AMBIENT_TEMP,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_AMBIENT_TEMP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BACKLIGHT,         APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_BACKLIGHT_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_WASHER_FLUID,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_WASHER_FLUID_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_HANDBRAKE_BRAKE,   APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_HANDBRAKE_BRAKE_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ABS_ESP,           APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_ABS_ESP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_WHEEL_SPEEDS,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_WHEEL_SPEEDS_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_STEERING_ANGLE,    APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_STEERING_ANGLE_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_EPS_WARNING,       APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_EPS_WARNING_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AIRBAG,            APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_AIRBAG_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_SEATBELT,          APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_SEATBELT_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_TPMS,              APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_TPMS_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BRAKE_PAD_WEAR,    APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_BRAKE_PAD_WEAR_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ACC,               APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_ACC_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FRONT_ASSIST,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_FRONT_ASSIST_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_LANE_ASSIST,       APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_LANE_ASSIST_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FATIGUE,           APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_FATIGUE_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_KEYLESS,           APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_KEYLESS_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NM_SYNC,           APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_NM_SYNC_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GATEWAY_HEARTBEAT, APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_GATEWAY_HEARTBEAT_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_HVAC_TEMP,         APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_HVAC_TEMP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_UNMAPPED,          APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_UNMAPPED_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_IGNITION,          APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_IGNITION_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AUDIO_TRACK,       APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_AUDIO_TRACK_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NAV,               APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_NAV_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_PHONE_CALL,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_PHONE_CALL_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NEEDLE_SWEEP,      APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_NEEDLE_SWEEP_CAN_ID },
    //{ APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_TRIP_RESET,        APP_CAN_CH_MAIN, 0u, MQB_CLUSTER_TRIP_RESET_CAN_ID },
};

const u16_t canif_num_tx_pdus_s = (u16_t)( sizeof( canif_tx_pdus_s ) / sizeof( canif_tx_pdus_s[0u] ) );

/***************************************************************************************************
**                              PDUR                                                              **
***************************************************************************************************/
/* Routes by PDU ID alone - no CAN ID, channel or medium. What a route sends with and receives from is the
   lower layer's: CANIF's receive and transmit tables, CANTP's pairs. */
const PDUR_route_st pdur_routing_table_s[] =
{
    /* Sensor slots, disabled - remove the // to enable */
    //[APP_PDU_SENSOR_BASE + 0u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 1u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 2u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 3u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 4u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 5u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 6u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 7u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 8u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 9u]  = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 10u] = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_SENSOR_BASE + 11u] = { .lower_layer_tx_func = CANIF_transmit },

    /* Shifter frames go to GKT_SHIFTER's gateway, never straight to TCU */
    [APP_PDU_GKT_LEVER_RX]     = { .upperLayerRxIndication = app_pdur_gkt_lever_rx },
    [APP_PDU_GKT_HEARTBEAT_RX] = { .upperLayerRxIndication = app_pdur_gkt_heartbeat_rx },

    /* UDS_tx_confirmation releases the shared buffer once a response has actually gone out */
    [APP_PDU_UDS_FUNCTIONAL] =
    {
        .upperLayerRxIndication = UDS_rx_indication,
        .lower_layer_tx_func    = app_cantp_transmit,
        .txConfirmation         = UDS_tx_confirmation
    },
    [APP_PDU_UDS_PHYSICAL] =
    {
        .upperLayerRxIndication = UDS_rx_indication,
        .lower_layer_tx_func    = app_cantp_transmit,
        .txConfirmation         = UDS_tx_confirmation
    },

    /* The HUB's own requests to the nodes on CAN2 (app_cantp2_can_tx picks the real node ID) */
    [APP_PDU_NODE_DIAG] =
    {
        .upperLayerRxIndication = UDS_CLIENT_rx_indication,
        .lower_layer_tx_func    = app_cantp2_transmit,
        .txConfirmation         = UDS_CLIENT_tx_confirmation
    },

    /* The gateway: the CANIF receive tables map the tester's request IDs (CAN1) and the nodes' response IDs
       (CAN2) to these two routes, and DIAG_GW is told the ID so it knows the node */
    [APP_PDU_GW_TESTER] = { .upperLayerRxIndicationId = DIAG_GW_tester_rx },
    [APP_PDU_GW_NODE]   = { .upperLayerRxIndicationId = DIAG_GW_node_rx },

    /* Plain-frame routes - CANIF sends by PDU ID (its transmit table) and queues what the controller's
       mailboxes can't take at once */
    [APP_PDU_GKT_LED_TX]                                   = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_GKT_BACKLIGHT_TX]                             = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_TCU_GEAR_STATE_TX]                            = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_TCU_DYNAMICS_TX]                              = { .lower_layer_tx_func = CANIF_transmit },

    /* MRA_CLUSTER - frames CarCluster sends on its second bus ([CAN2] in MRA_CLUSTER.h) also go out
       on the main bus here */
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_IGNITION]       = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_KEY_STATUS]     = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DOOR_STATUS]    = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_RPM]            = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SPEED]          = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRANS1]         = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_GEARBOX_ALIVE]  = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DRIVE_MODE]     = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_COOLANT_TEMP]   = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ABS]            = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS]         = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_LIGHTS2]        = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BLINKERS]       = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_FUEL_LEVEL]     = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TRACTION]       = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_AIRBAG]         = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_DISTRONIC]      = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ENGINE_WARN]    = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_POWER_STEERING] = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_TPMS]           = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_ASSIST_ERRORS]  = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_SAFETY_ERRORS]  = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_BRAKE_FLUID]    = { .lower_layer_tx_func = CANIF_transmit },
    [APP_PDU_MRA_TX_BASE + MRA_CLUSTER_MSG_STEERING_BTNS]  = { .lower_layer_tx_func = CANIF_transmit },

    /* MQB cluster frames, disabled - remove the // to enable */
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ENGINE_RPM]        = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_SPEED_ODOMETER]    = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_COOLANT_TEMP]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_OIL_TEMP]          = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BOOST_PRESSURE]    = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FUEL_LEVEL]        = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GEAR_SELECTOR]     = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GEAR_DISPLAY]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_START_STOP]        = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_LIGHTING]          = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FOG_LIGHTS]        = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_DOOR_STATUS]       = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AMBIENT_TEMP]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BACKLIGHT]         = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_WASHER_FLUID]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_HANDBRAKE_BRAKE]   = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ABS_ESP]           = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_WHEEL_SPEEDS]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_STEERING_ANGLE]    = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_EPS_WARNING]       = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AIRBAG]            = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_SEATBELT]          = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_TPMS]              = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_BRAKE_PAD_WEAR]    = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_ACC]               = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FRONT_ASSIST]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_LANE_ASSIST]       = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_FATIGUE]           = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_KEYLESS]           = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NM_SYNC]           = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_GATEWAY_HEARTBEAT] = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_HVAC_TEMP]         = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_UNMAPPED]          = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_IGNITION]          = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_AUDIO_TRACK]       = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NAV]               = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_PHONE_CALL]        = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_NEEDLE_SWEEP]      = { .lower_layer_tx_func = CANIF_transmit },
    //[APP_PDU_MQB_TX_BASE + MQB_CLUSTER_MSG_TRIP_RESET]        = { .lower_layer_tx_func = CANIF_transmit },
};

const u16_t pdur_num_routes_s = (u16_t)( sizeof( pdur_routing_table_s ) / sizeof( pdur_routing_table_s[0u] ) );

/***************************************************************************************************
**                              UDS_CLIENT                                                        **
***************************************************************************************************/
/* Read by itself from the selected node (UDS_CLIENT_set_node) until it answers;
   UDS_CLIENT_get_did( APP_NODE_PART_NUMBER_DID, ... ) returns it. Every request asks DIAG_GW for that node's
   pair first, so it never interrupts a tester. */
STATIC u8_t app_node_part_number_s[ APP_NODE_PART_NUMBER_MAX_LEN ];

STATIC const UDS_CLIENT_did_st app_uds_client_dids_s[] =
{
    { .did = APP_NODE_PART_NUMBER_DID, .data_p = app_node_part_number_s, .max_len = APP_NODE_PART_NUMBER_MAX_LEN },
};

/* P2/P2* placeholders until the node's own values are known: ISO 14229-2 defaults plus margin */
const UDS_CLIENT_cfg_st app_uds_client_cfg_s =
{
    .route_id            = APP_PDU_NODE_DIAG,
    .num_nodes           = (u8_t)( APP_NODE_REQUEST_LAST_ID - APP_NODE_REQUEST_BASE_ID + 1u ),
    .tp_send_func_p      = PDUR_tx,
    .reserve_func_p      = DIAG_GW_local_reserve,
    .p2_ms               = 150u,
    .p2_star_ms          = 5500u,
    .tick_period_ms      = SCHED_PERIOD_10MS,
    .auto_read_dids_p    = app_uds_client_dids_s,
    .num_auto_read_dids  = (u8_t)( sizeof( app_uds_client_dids_s ) / sizeof( app_uds_client_dids_s[0u] ) ),
    .auto_read_period_ms = 1000u
};

/***************************************************************************************************
**                              MSG_SCHED (CAN sensor messages)                                   **
***************************************************************************************************/
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
STATIC const MSG_SCHED_msg_cfg_st can_msg_table_s[] =
{
    { APP_PDU_SENSOR_BASE + 0u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 1u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 2u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 3u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 4u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 5u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 6u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 7u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 8u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 9u,  0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 10u, 0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
    { APP_PDU_SENSOR_BASE + 11u, 0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },
};

const MSG_SCHED_cfg_st msg_sched_cfg_s =
{
    .msg_table = can_msg_table_s,
    .num_msgs  = (u8_t)( sizeof( can_msg_table_s ) / sizeof( can_msg_table_s[0u] ) )
};

/****************************** END OF FILE *******************************************************/
