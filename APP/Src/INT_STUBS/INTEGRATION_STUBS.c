/***************************************************************************************************
**                              INTEGRATION_STUBS                                                 **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "WIFI.h"
#include "HAL_ADC.h"
#include "HAL_TIM.h"
#include "HAL_SPI.h"
#include "HAL_I2C.h"
#include "HAL_UART.h"
#include "MODE_MGR.h"
#include "RF_MGR.h"
#include "PERSIST_BLK.h"
#include "FLS_STM32F1.h"
#include "nvic_driver.h"
#include "TB_CBK.h"
#include "TJA1051.h"
#include "CPS.h"
#include "HMI_SH1106.h"
#include "MENU_NAV.h"
#include "HEADER.h"
#include "CANTP.h"
#include "UDS.h"
#include "SHARED_RAM.h"
#include "MCU_JUMP.h"

/* APP_header_st/app_header_s live in HEADER.c (xCOMMON_MODULES/Src/HEADER) - shared across every
   project (STM32, S32K144, ...) that uses app_crc_injector/app_signer, since the byte layout those
   tools patch is the same regardless of target. This project's own contribution is placing the
   .app_header section in APP/linker_script/STM32F103C8_flash.ld at APP_FLASH_START (0x08005000),
   padded to 256 bytes so .isr_vector/code always starts at header+0x100. The FBL checks
   presence_pattern before jumping to APP. */

/***************************************************************************************************
**                              SYSTICK                                                           **
***************************************************************************************************/
const SYSTICK_cfg_st systick_cfg_s =
{
    .timeout_ms     = APP_TIMER_TICK_RATE_MS,
    .systick_func_p = MODE_MGR_tick
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
    .mode_cycle_time    = APP_TIMER_TICK_RATE_MS,
    .dwt_get_cnt_func_p = DWT_get_count
};

/***************************************************************************************************
**                              TIME                                                              **
***************************************************************************************************/
const TIME_cfg_st time_cfg_s =
{
    .time_increment_ms = APP_TIMER_TICK_RATE_MS,
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

/* System buttons get their own BTN_MGR instance. Any panel (see HMI) owns a separate instance
   with its own buttons, so the two sets are scanned independently and neither can form a
   combination with the other. Storage is declared here because BTN_MGR allocates none itself. */
BTN_MGR_control_st  btm_mgr_control_s[BTM_MGR_FUNC_TABLE_SIZE( btm_mgr_func_table_s )];
BTN_MGR_instance_st btm_mgr_instance_s;

/***************************************************************************************************
**                              HMI_SH1106 (OLED + encoder + buttons panel)                       **
**  This board's wiring for the panel. What is actually on screen and how you move between        **
**  screens is MENU_NAV's - see APP/Src/MENU_NAV/. Nothing menu-related lives here, only the       **
**  same kind of board wiring every other cfg struct in this file has.                             **
***************************************************************************************************/
/* TIM4's encoder belongs to this panel. There is only one on the board, so there is deliberately
   no standalone ROTARY_MGR instance - two instances polling the same counter would each see every
   detent and both fire. */

/* Press feedback is board policy, not navigation logic - a different board might want an LED
   flash or nothing at all. Keeping it in this wrapper is what lets MENU_NAV stay unaware that a
   buzzer exists; it only owns whether the user wants one and for how long, via the Buzzer and
   Beep Time screens. */
STATIC void hmi_on_input( HMI_SH1106_input_et input )
{
    if( MENU_NAV_buzzer_enabled() == TRUE )
    {
        BUZZER_beep( &buzzer_instance_s, (u64_t)MENU_NAV_get_beep_duration_ms() );
    }

    MENU_NAV_on_input( input );
}

const HMI_SH1106_cfg_st hmi_sh1106_cfg_s =
{
    /* Display - HAL_I2C_write_registers matches i2c_write_func_p's signature exactly
       (dev_addr, reg_addr, data_p, len -> pass_fail_et), so no adapter is needed */
    .i2c_write_func_p       = HAL_I2C_write_registers,
    .i2c_address            = SH1106_I2C_ADDR_DEFAULT,
    .contrast               = SH1106_DEFAULT_CONTRAST,
    /* Panel is mounted rotated 180 degrees on this board - flipping both axes together rotates
       the image, rather than mirroring it the way flipping just one axis would. */
    .flip_horizontal        = TRUE,
    .flip_vertical          = TRUE,

    /* Encoder - hardware quadrature on TIM4. HAL_TIM4_init_encoder() configures
       TIM_EncoderMode_TI12 (full x4 decode - counts both edges of both channels), so one
       mechanical detent reports as 4 raw counts, not 1. */
    .mode                   = ROTARY_MODE_HARDWARE_TIMER,
    .enc_get_count_func_p   = HAL_TIM_ENC_get_counter,
    .reverse_direction      = FALSE,
    .enc_counts_per_detent  = 4u,

    /* Buttons - active low against internal pull-ups */
    .btn_read_select_func_p  = HAL_BRD_read_panel_select_btn,
    .btn_read_confirm_func_p = HAL_BRD_read_panel_confirm_btn,
    .btn_read_back_func_p    = HAL_BRD_read_panel_back_btn,
    .btn_inverted            = TRUE,

    /* Behaviour - tick_rate_ms must match the MODE_MGR slot HMI_SH1106_tick() runs in */
    .tick_rate_ms           = 10u,

    /* Repaint twice a second regardless, so a screen showing live values (the status page reading
       the input counters) keeps up without having to notice its own data changing. */
    .refresh_period_ms      = 500u,

    /* HMI_SH1106_tick() runs from MODE_MGR_tick(), the SysTick ISR - guards
       HMI_SH1106_get_stats() against reading the counters mid-increment */
    .stats_critical_enter_func_p = NVIC_DisableGlobalIRQ,
    .stats_critical_exit_func_p  = NVIC_EnableGlobalIRQ,

    /* Input goes through a local wrapper purely to add the buzzer; drawing goes straight to
       MENU_NAV, which has nothing board-specific to add. */
    .on_input_func_p        = hmi_on_input,
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
    //.zero_pulse_func_p  = HAL_BRD_WS2811_zero_pulse_direct,
    //.one_pulse_func_p   = HAL_BRD_WS2811_one_pulse_direct,
    .disable_irq_func_p = NVIC_DisableGlobalIRQ,
    .enable_irq_func_p  = NVIC_EnableGlobalIRQ,
};

/***************************************************************************************************
**                              NRF24                                                             **
***************************************************************************************************/
STATIC void on_packet_rx( void )
{
    RF_MGR_get_rx_frame();
    HAL_BRD_toggle_onboard_led();
}

NRF24_instance_st nrf24_instance_s =
{
    .ce_pin_func_p         = HAL_BRD_NRF24_set_ce_pin_state,
    .cs_pin_func_p         = HAL_BRD_NRF24_spi_slave_select,
    .spi_func_p            = HAL_SPI1_write_and_read_data,
    .us_delay_func_p       = DWT_delay_us,
    .packet_tx_conf_func_p = RF_MGR_tx_complete,
    .packet_rx_func_p      = on_packet_rx
};

/***************************************************************************************************
**                              NVM                                                               **
***************************************************************************************************/
const NVM_hw_interface_st nvm_hw_interface_s =
{
    .init_func       = FLS_STM32F1_init,
    .get_flash_size  = FLS_STM32F1_get_nvm_total_size,
    .get_flash_base  = FLS_STM32F1_get_nvm_base_address,
    .get_sector_size = FLS_STM32F1_get_sector_size,
    .erase_func      = FLS_STM32F1_erase_sector,
    .write_func      = FLS_STM32F1_write_data,
    .compare_func    = NULL,
    .recover_func    = NULL
};

const NVM_func_p_st nvm_persist_block_s =
{
    .default_data     = &PERSIST_GENERIC_DEFAULT_DATA_BLK_s,
    .current_data     = &PERSIST_generic_data_blk_g,
    .data_len         = sizeof(PERSIST_generic_data_blk_st),
    .expected_version = 1u,
    .event_fn         = NULL
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

/* ESP01 now owns its RX buffer directly (ESP01_uart_byte_rx appends into it byte-by-byte,
 * ESP01_tick() detects the inter-byte gap and analyses it) - no separate UART-owned buffer here.
 *
 * Must be strictly > 1 tick period: timestamp can be stale by up to 1 tick, so an exact
 * 1x window can fire with near-zero actual silence. +1ms guarantees at least 1 full tick
 * of real inter-byte gap before a frame is considered complete. */
#define ESP01_INTER_BYTE_TIMEOUT_MS     ( APP_TIMER_TICK_RATE_MS + 1u )

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
        /* Send failed (e.g. ESP8266 "link is not valid" - the TCP connection has already
           died and the driver didn't know). Feed this into the same connection-lost path
           used for a refused CONNACK, so TB stops trying to send on a dead link instead of
           silently discarding the failure and only recovering once the 90s staleness
           timeout in tb_handle_connected_state() eventually trips. */
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

/***************************************************************************************************
**                              RF_MGR                                                            **
***************************************************************************************************/
const RF_MGR_cfg_st rf_mgr_cfg_s =
{
    .mode    = RF_MGR_MODE_RX,
    .channel = 0u
};

/***************************************************************************************************
**                              CAN / PDUR / MSG_SCHED                                           **
***************************************************************************************************/
STATIC void pdur_hal_can_tx( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len )
{
    (void)frame_type;
    HAL_CAN_send_frame( id, id_type, data_p, (u8_t)len );
}

/* Base CAN ID for sensor telemetry frames — slot N uses ID (base + N) */
#define CAN_SENSOR_BASE_ID  ( 0x100u )

/* One TX-only PDUR route per sensor slot */
#define CAN_SENSOR_PDUR_ENTRY( n ) \
    { 0u, 0xFFFFFFFFu, ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), 0u, 0u, NULL_P, pdur_hal_can_tx }

/* Cyclic hub heartbeat/status frame - byte0 rolling counter (proves the frame is still live,
   not just present), byte1 current MODE_MGR mode, byte2 current RF_MGR link state. */
#define APP_HEARTBEAT_CAN_ID    ( 0x200u )
#define APP_HEARTBEAT_PERIOD_MS ( 1000u )

/* UDS diagnostics over CAN-TP - functional (0x700/0x600) and physical (0x7E0/0x7E8) request/
   response pair, same IDs FBL uses (FBL/Src/UDS_CFG/UDS_config.h) - safe to reuse rather than
   needing a second set, since BM only ever runs one of APP/FBL at a time, never both, so a tester
   never has to know which stage it's actually talking to. */
#define APP_UDS_REQUEST_ID   ( 0x700u )
#define APP_UDS_RESPONSE_ID  ( 0x600u )
#define APP_CAN_RX_ID        ( 0x7E0u )
#define APP_CAN_TX_ID        ( 0x7E8u )

/* Not STATIC - passed to HAL_CAN_set_rx_callback() from main.c, the same way MODE_MGR_tick
   (MODE_MGR.h) is referenced by name from systick_cfg_s below, just in the opposite direction. */
void app_can_rx_wrapper( u32_t id, u8_t id_type, u8_t* data_p, u8_t dlc )
{
    CANTP_rx_frame_received( &app_cantp_instance_s, id, (CANTP_id_type_et)id_type, data_p, (u16_t)dlc );
}

STATIC void app_cantp_send_wrapper( CANTP_can_msg_format_st* msg_p )
{
    if( msg_p != NULL_P )
    {
        (void)HAL_CAN_send_frame( msg_p->Id, (u8_t)msg_p->id_type, msg_p->Data, msg_p->DLC );
    }
}

/* PDUR's lower_layer_tx_func_t shape - routes UDS responses through CANTP so multi-frame
   responses get segmented, matching FBL's fbl_cantp_tx_request_wrapper. channel is unused now
   that a CANTP instance IS a physical channel (see CANTP_instance_st) - kept as a parameter only
   because PDUR_lower_layer_tx_func_t's shape is shared with non-CANTP routes. */
STATIC void app_cantp_tx_request_wrapper( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len )
{
    (void)CANTP_tx_request( &app_cantp_instance_s, id, (CANTP_id_type_et)id_type, (CANTP_frame_type_et)frame_type, data_p, len, NULL_P );
}

/* Adapter: CANTP_message_rx_func_p carries CANTP_id_type_et (a CANTP-local enum), but PDUR_rx_indication
   deliberately stays CANTP-agnostic and takes a plain u8_t id_type (PDUR routes CANTP, HAL_CAN, LINTP,
   etc. uniformly) - the two aren't the same type, so a direct function-pointer assignment between them
   isn't valid without this cast. */
STATIC void app_cantp_rx_indication_wrapper( u32_t id, CANTP_id_type_et id_type, u8_t* data_p, u16_t len )
{
    PDUR_rx_indication( id, (u8_t)id_type, data_p, len );
}

STATIC void app_uds_tx( u8_t* data_p, u16_t len )
{
    PDUR_tx( APP_UDS_RESPONSE_ID, 0u, data_p, len );
}

/*!
****************************************************************************************************
*   \brief         UDS session change notification
*   \details       Entering PROGRAMMING means a tester wants FBL entry - set the FBL request flag so
*                  BM boots FBL after the soft reset that transition schedules (see UDS.c's
*                  uds_apply_session_change()/uds_invoke_pending_action(); this callback runs before
*                  the reset is scheduled, which is the whole reason it exists).
*
*                  Whether the transition is allowed at all is not decided here - UDS_CFG/
*                  UDS_config.c's session table owns that, and only permits EXTENDED -> PROGRAMMING
*                  with security unlocked. By the time this runs the request has already been
*                  authorised, so it just records the intent. Same module, opposite direction:
*                  FBL's own fbl_uds_session_notify clears the flag on the way back to DEFAULT.
***************************************************************************************************/
STATIC void app_uds_session_notify( UDS_session_et old_session, UDS_session_et new_session )
{
    if( ( new_session == UDS_SES_PROGRAMMING ) && ( old_session != UDS_SES_PROGRAMMING ) )
    {
        SHARED_RAM_set_fbl_request( TRUE );
    }
}

/* Deliberately NOT a designated initializer: CANTP_instance_st embeds the RX/TX queues and TP
   session arrays (~1.7KB, almost entirely zero) alongside these config fields. Giving the struct
   an initializer with even one non-zero field forces the compiler to store the WHOLE object -
   zero regions included - as flash-resident .data instead of letting the zero-only bulk of it
   land in .bss for free. Left plain (zero-initialized in .bss, same as any other global) and
   configured at runtime instead - see app_cantp_instance_init(), called once from main.c before
   CANTP_init(). .fd_enable (FALSE) needs no explicit assignment - already zero from .bss. */
CANTP_instance_st app_cantp_instance_s;

void app_cantp_instance_init( void )
{
    app_cantp_instance_s.CANTP_message_rx_func_p = app_cantp_rx_indication_wrapper;
    app_cantp_instance_s.tx_func_p               = app_cantp_send_wrapper;
    app_cantp_instance_s.tp_buffer               = pdur_buffer_s;
    app_cantp_instance_s.tp_ids[0]               = APP_UDS_REQUEST_ID;
    app_cantp_instance_s.tp_ids[1]               = APP_CAN_RX_ID;
    app_cantp_instance_s.st_min                  = 10u;
    app_cantp_instance_s.rx_block_size           = 10u;
    app_cantp_instance_s.N_Cr                    = CANTP_DEFAULT_N_CR;
    app_cantp_instance_s.N_Bs                    = CANTP_DEFAULT_N_BS;
    app_cantp_instance_s.N_Ar                    = CANTP_DEFAULT_N_AR;
    app_cantp_instance_s.uds_req_id              = APP_UDS_REQUEST_ID;
    app_cantp_instance_s.uds_resp_id             = APP_UDS_RESPONSE_ID;
}

/* 0x10/0x11/0x3E (session control, ECU reset, tester present) are handled entirely inside UDS.c
   regardless of the service table (see uds_process_rx_message()'s routing). The service table
   itself (APP/Src/UDS_CFG/UDS_config.c) now carries SecurityAccess (0x27) - required, unlocked in
   EXTENDED session, before UDS.c will honour a session-control request into PROGRAMMING. */
const UDS_func_p_st app_uds_func_table_s =
{
    .tp_send_func_p          = app_uds_tx,
    .perform_soft_reset      = MCU_JUMP_software_reset,
    .perform_hard_reset      = MCU_JUMP_software_reset,
    .s3_timeout_notify       = NULL_P,   /* Standard ISO 14229 behaviour - see UDS.h */
    .session_change_notify   = app_uds_session_notify,
    .message_received_notify = NULL_P,
    .tester_present_notify   = NULL_P,
};

const PDUR_rx_route_st pdur_routing_table_s[] =
{
    CAN_SENSOR_PDUR_ENTRY(  0u ),
    CAN_SENSOR_PDUR_ENTRY(  1u ),
    CAN_SENSOR_PDUR_ENTRY(  2u ),
    CAN_SENSOR_PDUR_ENTRY(  3u ),
    CAN_SENSOR_PDUR_ENTRY(  4u ),
    CAN_SENSOR_PDUR_ENTRY(  5u ),
    CAN_SENSOR_PDUR_ENTRY(  6u ),
    CAN_SENSOR_PDUR_ENTRY(  7u ),
    CAN_SENSOR_PDUR_ENTRY(  8u ),
    CAN_SENSOR_PDUR_ENTRY(  9u ),
    CAN_SENSOR_PDUR_ENTRY( 10u ),
    CAN_SENSOR_PDUR_ENTRY( 11u ),
    /* TX-only PDUR route for the cyclic heartbeat frame */
    { 0u, 0xFFFFFFFFu, APP_HEARTBEAT_CAN_ID, 0u, 0u, NULL_P, pdur_hal_can_tx },
    /* Functional (0x700->0x600) and physical (0x7E0->0x7E8) UDS request/response routes */
    { APP_UDS_REQUEST_ID, 0xFFFFFFFFu, APP_UDS_RESPONSE_ID, 0u, 2u, UDS_rx_indication, app_cantp_tx_request_wrapper },
    { APP_CAN_RX_ID,      0xFFFFFFFFu, APP_CAN_TX_ID,       0u, 2u, UDS_rx_indication, app_cantp_tx_request_wrapper },
};

const u16_t pdur_num_routes_s = (u16_t)( sizeof(pdur_routing_table_s) / sizeof(pdur_routing_table_s[0u]) );

/* Packs one sensor DB slot into the 7-byte CAN frame.
   msg_idx maps directly to the sensor slot (MSG_SCHED table is 1:1 with sensor slots). */
STATIC void can_sensor_get_data( u8_t msg_idx, u8_t* buf_p, u8_t* len_p )
{
    const RF_MGR_sensor_data_st* db_p = RF_MGR_get_sensor_db();

    buf_p[0u] = (u8_t)( (u16_t)db_p[msg_idx].temperature_centidegC >> 8u );
    buf_p[1u] = (u8_t)(  db_p[msg_idx].temperature_centidegC        & 0xFFu );
    buf_p[2u] = (u8_t)( (u16_t)db_p[msg_idx].humidity_tenths_pct   >> 8u );
    buf_p[3u] = (u8_t)(  db_p[msg_idx].humidity_tenths_pct          & 0xFFu );
    buf_p[4u] = (u8_t)(  db_p[msg_idx].battery_voltage_mv           >> 8u );
    buf_p[5u] = (u8_t)(  db_p[msg_idx].battery_voltage_mv           & 0xFFu );
    buf_p[6u] = (u8_t)db_p[msg_idx].comms_lost;
    *len_p    = 7u;
}

/* One on-event MSG_SCHED entry per sensor slot */
#define CAN_SENSOR_MSG_ENTRY( n ) \
    { ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), 0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data }

STATIC u8_t app_heartbeat_ctr_s = 0u;

STATIC void app_heartbeat_get_data( u8_t msg_idx, u8_t* buf_p, u8_t* len_p )
{
    (void)msg_idx;

    buf_p[0u] = app_heartbeat_ctr_s++;
    buf_p[1u] = (u8_t)MODE_MGR_get_mode();
    buf_p[2u] = (u8_t)RF_MGR_get_state();
    *len_p    = 3u;
}

STATIC const MSG_SCHED_msg_cfg_st can_msg_table_s[] =
{
    CAN_SENSOR_MSG_ENTRY(  0u ),
    CAN_SENSOR_MSG_ENTRY(  1u ),
    CAN_SENSOR_MSG_ENTRY(  2u ),
    CAN_SENSOR_MSG_ENTRY(  3u ),
    CAN_SENSOR_MSG_ENTRY(  4u ),
    CAN_SENSOR_MSG_ENTRY(  5u ),
    CAN_SENSOR_MSG_ENTRY(  6u ),
    CAN_SENSOR_MSG_ENTRY(  7u ),
    CAN_SENSOR_MSG_ENTRY(  8u ),
    CAN_SENSOR_MSG_ENTRY(  9u ),
    CAN_SENSOR_MSG_ENTRY( 10u ),
    CAN_SENSOR_MSG_ENTRY( 11u ),
    //{ APP_HEARTBEAT_CAN_ID, APP_HEARTBEAT_PERIOD_MS, 0u, MSG_SCHED_TX_CYCLIC, app_heartbeat_get_data },
};

const MSG_SCHED_cfg_st msg_sched_cfg_s =
{
    .msg_table      = can_msg_table_s,
    .num_msgs       = (u8_t)( sizeof(can_msg_table_s) / sizeof(can_msg_table_s[0u]) ),
    .get_time_ms_fn = TIME_get_cumulative_run_time_ms,
};

/***************************************************************************************************
**                              CPS — 2x ABS wheel-speed sensors                                   **
**  Uniform 48-tooth ABS rings (no missing-tooth gap) — CPS_GAP_NONE runs from the very first      **
**  interval, no gap-sync wait, no CPS_STATE_SYNCING. Pin setup + EXTI3_IRQHandler /                **
**  EXTI9_5_IRQHandler live in HAL_BRD.c, which call CPS_tooth_event() directly — no generic        **
**  dispatch layer. Wired directly in main(): CPS_init(&cps_instance_s, &cps_cfg_s,                 **
**  SystemCoreClock) / CPS_init(&cps_instance_2_s, &cps_cfg_2_s, SystemCoreClock). Ticked           **
**  directly via CPS_tick() from MODE_MGR. Watch cps_instance_s.rpm / cps_instance_2_s.rpm live     **
**  in the debugger, or call CPS_get_rpm().                                                        **
***************************************************************************************************/
CPS_instance_st cps_instance_s;
CPS_instance_st cps_instance_2_s;

/* Mask/unmask just the ABS #1 tooth-edge line (EXTI3) around CPS_tick()'s ring-buffer read —
 * narrower than a global __disable_irq(), so it doesn't add latency to any other interrupt
 * in the system. See critical_enter_func_p/critical_exit_func_p doc in CPS.h: without this,
 * a tooth edge landing mid-average (very likely at high input frequencies, since
 * CPS_tooth_event() runs at priority 0 and can preempt CPS_tick() at any point) corrupts
 * the RPM average with a torn mix of old/new samples. */
STATIC void cps_critical_enter( void )
{
    NVIC_DisableIRQ( EXTI3_IRQn );
}

STATIC void cps_critical_exit( void )
{
    NVIC_EnableIRQ( EXTI3_IRQn );
}

/* Same as above, but for ABS #2's tooth-edge line (EXTI9). Each instance masks only its own
 * line — never the other sensor's — so a burst of edges on one wheel never delays the other. */
STATIC void cps_critical_enter_2( void )
{
    NVIC_DisableIRQ( EXTI9_5_IRQn );
}

STATIC void cps_critical_exit_2( void )
{
    NVIC_EnableIRQ( EXTI9_5_IRQn );
}

const CPS_cfg_st cps_cfg_s =
{
    .total_teeth                     = CPS_TEETH_ABS_48,
    .gap_type                        = CPS_GAP_NONE,
    .capture_edge                    = CPS_EDGE_RISING,
    .filter_depth                    = CPS_RPM_FILTER_DEPTH_MAX, /* max averaging (8 samples) —
                                                 * smooths out sample-to-sample jitter at high
                                                 * input frequencies */
    .stall_timeout_us                = 500000u,
    .rpm_max_credible                = 0xFFFFFFFFu, /* TODO: set a real credible ceiling for this
                                                 * vehicle's wheel/tire combo once known */
    .get_timer_ticks_func_p          = DWT_get_count, /* raw cycle counter — no conversion in the ISR */
    .revolution_sync_callback_func_p = NULL_P, /* never fires under CPS_GAP_NONE — no gap to find */
    .stall_callback_func_p           = NULL_P,
    .rpm_implausible_callback_func_p = NULL_P, /* rpm_max_credible is unclamped above, so this can
                                                 * never actually fire — wire it up once a real
                                                 * credible ceiling is set */
    .critical_enter_func_p           = cps_critical_enter,
    .critical_exit_func_p            = cps_critical_exit,
};

const CPS_cfg_st cps_cfg_2_s =
{
    .total_teeth                     = CPS_TEETH_ABS_48,
    .gap_type                        = CPS_GAP_NONE,
    .capture_edge                    = CPS_EDGE_RISING,
    .filter_depth                    = CPS_RPM_FILTER_DEPTH_MAX,
    .stall_timeout_us                = 500000u,
    .rpm_max_credible                = 0xFFFFFFFFu, /* TODO: set a real credible ceiling for this
                                                 * vehicle's wheel/tire combo once known */
    .get_timer_ticks_func_p          = DWT_get_count,
    .revolution_sync_callback_func_p = NULL_P,
    .stall_callback_func_p           = NULL_P,
    .rpm_implausible_callback_func_p = NULL_P,
    .critical_enter_func_p           = cps_critical_enter_2,
    .critical_exit_func_p            = cps_critical_exit_2,
};

/***************************************************************************************************
**                              SLIP_DETECT                                                        **
**  Compares ABS #1 vs ABS #2 wheel RPM directly (both 48-tooth rings, same tyre size, per          **
**  SLIP_DETECT_AXLE_SAME) — the ratio math cancels out circumference, so no TYRE_CALC/SPEED_CONV   **
**  conversion is needed here. Ticked alongside both CPS_tick() calls in MODE_MGR.                  **
***************************************************************************************************/
SLIP_DETECT_instance_st slip_detect_instance_s;

/* TIME_get_cumulative_run_time_ms() returns u64_t; SLIP_DETECT_cfg_st's get_time_ms_func_p is
 * u32_t, matching the wraparound-safe-arithmetic reasoning used throughout this codebase (e.g.
 * ESC.c) — truncating to the low 32 bits is fine since only elapsed-time differences matter,
 * never the absolute value. */
STATIC u32_t slip_detect_get_time_ms( void )
{
    return( (u32_t)TIME_get_cumulative_run_time_ms() );
}

const SLIP_DETECT_cfg_st slip_detect_cfg_s =
{
    .axle_relation               = SLIP_DETECT_AXLE_SAME,
    .min_speed                   = 50u,  /* RPM — placeholder gate below which readings are
                                          * dominated by noise rather than real slip. Roughly
                                          * walking-pace wheel speed for a typical road wheel;
                                          * revisit once this vehicle's actual tyre size/gearing
                                          * is known. */
    .slip_enter_ratio_pct        = 10u,  /* 10% speed difference flags slip */
    .slip_clear_ratio_pct        = 5u,   /* must drop back to <=5% before clearing (hysteresis) */
    .slip_reinvoke_interval_ms   = 2000u, /* re-fire the onset callback every 2s while slip
                                          * persists, as a "still slipping" heartbeat */
    .get_time_ms_func_p          = slip_detect_get_time_ms,
    .slip_detected_callback_func_p = NULL_P,
    .slip_cleared_callback_func_p  = NULL_P,
};

/***************************************************************************************************
**                              REF_SPEED_CALC + vehicle speed                                     **
**  Fuses ABS #1 + #2 RPM into one reference RPM (currently just the two of them — degrades to a   **
**  plain average once they diverge, per REF_SPEED_CALC's own doc; becomes a genuine outlier-       **
**  rejecting fusion once a 3rd wheel/reference is added), then converts to kph via TYRE_CALC's     **
**  circumference. Distinct purpose from SLIP_DETECT above: that flags/reports pairwise divergence, **
**  this produces the actual speed estimate to use downstream.                                      **
**  vehicle_tyre_circumference_mm_s is computed once in main() via TYRE_CALC_get_circumference_mm() **
**  — TODO: placeholder 225/35/R19 size below, replace with this vehicle's real tyre size.           **
**  vehicle_reference_rpm_s / vehicle_speed_kph_s updated every MODE_MGR tick (mode_mgr_action_      **
**  schedule_normal, alongside the CPS_tick()/SLIP_DETECT_update() calls).                          **
***************************************************************************************************/
REF_SPEED_CALC_instance_st ref_speed_calc_instance_s;

const REF_SPEED_CALC_cfg_st ref_speed_calc_cfg_s =
{
    .reject_ratio_pct = 15u, /* a wheel >15% off the group median is excluded from the fused
                              * reference — deliberately looser than slip_detect's 10% enter
                              * threshold, since this is "is this wheel usable at all", not
                              * "should we flag a pairwise fault" */
    .min_speed        = 50u, /* RPM — same placeholder gate as slip_detect_cfg_s; see its comment */
    .ramp_cfg         =
    {
        /* TODO: rough placeholder assuming ~2m wheel circumference and ~1.2g max plausible
         * tyre accel/decel (~400 RPM/sec) — recompute once this vehicle's real tyre size is
         * known. Expressed per-call here (this is ticked every 10ms from MODE_MGR, i.e. 100
         * calls/sec), so 400 RPM/sec / 100 = 4 RPM per call. See REF_SPEED_CALC.h doc on
         * ramp_cfg: this is what catches all wheels slipping/locking together in sync, which
         * median-rejection alone can't. */
        .step_up   = 4,
        .step_down = 4,
    },
};

u16_t vehicle_tyre_circumference_mm_s = 0u; /* set once in main() via TYRE_CALC */
u32_t vehicle_reference_rpm_s         = 0u; /* updated every MODE_MGR tick */
u16_t vehicle_speed_kph_s             = 0u; /* updated every MODE_MGR tick */

/****************************** END OF FILE *******************************************************/