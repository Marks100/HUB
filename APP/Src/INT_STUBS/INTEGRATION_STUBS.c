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
#include "APP_NVM_BLOCKS.h"
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
/* Both reserved pages (NVM_BASE_ADDRESS/NVM_TOTAL_SIZE, PROJ_config.h), handed to NVM_GEN2 as its
   two partitions - APP no longer offsets itself a page up to stay clear of FBL. It does not need
   to: NVM_GEN2 packs blocks into a shared log keyed by block ID rather than assigning each one a
   page, FBL and APP own separate ID ranges (NVM_GEN2_BLOCK_ID_FBL_* / _APP_*), and compaction
   carries records it has no config for across verbatim. So the two images write into the same two
   pages without either being able to overwrite the other. See PROJ_config.h's NVM_BASE_ADDRESS
   comment and NVM_GEN2/README.md.
   The NVM_GEN2_block_cfg_st for each of APP's blocks lives in APP_NVM_BLOCKS.h/.c instead of here -
   default_data/version/event_fn describe the block's data, not this board's hardware, so only the
   hardware interface itself (below) and the register_block() calls in app_main() belong here. */
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
/* HAL_CAN has no async TX-complete notification of its own - unlike CANTP's TP sessions, a raw
   frame send is a single fire-and-forget submission, so only the immediate accept/reject
   (HAL_CAN_send_frame's own pass_fail_et) is meaningful here; there is no txConfirmation adapter to
   wire up for these routes. */
STATIC pass_fail_et pdur_hal_can_tx( u32_t id, PDUR_medium_et medium, u8_t* data_p, u16_t len )
{
    return( HAL_CAN_send_frame( id, (u8_t)medium, data_p, (u8_t)len ) );
}

/* Base CAN ID for sensor telemetry frames — slot N uses ID (base + N) */
#define CAN_SENSOR_BASE_ID  ( 0x100u )

/* Single source of truth for which sensor slots exist. pdur_routing_table_s and can_msg_table_s
   (below) are two separate arrays that both need one entry per slot in the same order - before this
   macro they were two independently hand-written lists of CAN_SENSOR_PDUR_ENTRY(n)/
   CAN_SENSOR_MSG_ENTRY(n) calls that had to be edited in lockstep by hand, with nothing catching a
   slot added to one but not the other. Expanding both tables from this one list instead means
   adding/removing a slot is a one-line change here, not a two-place edit that can silently drift.
   Must still be kept equal to APP_NUM_SENSOR_SLOTS below (the enum needs a plain integer, which
   can't be derived from this list without more preprocessor machinery than 12 fixed slots justify). */
#define APP_SENSOR_SLOTS( ENTRY ) \
    ENTRY(0u)  ENTRY(1u)  ENTRY(2u)  ENTRY(3u)  \
    ENTRY(4u)  ENTRY(5u)  ENTRY(6u)  ENTRY(7u)  \
    ENTRY(8u)  ENTRY(9u)  ENTRY(10u) ENTRY(11u)

/* Must match the number of ENTRY(n) calls in APP_SENSOR_SLOTS above. */
#define APP_NUM_SENSOR_SLOTS ( 12u )

/* One TX-only PDUR route per sensor slot - designated array-index initializer so this always lands
   on the matching app_pdu_id_et slot (below) regardless of macro invocation order. Trailing comma
   in the macro body (not between invocations) since APP_SENSOR_SLOTS expands these back-to-back. */
#define CAN_SENSOR_PDUR_ENTRY( n ) \
    [APP_PDU_SENSOR_BASE + (n)] = { .tx_id = ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), .lower_layer_tx_func = pdur_hal_can_tx },

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

/* APP's own logical PDU IDs - these, not the raw CAN IDs above, are what pdur_routing_table_s's
   array position means and what app_uds_tx()/CAN_SENSOR_MSG_ENTRY dispatch on. Values double as
   array indices (designated-index initializers pin each route to its enum value explicitly, so
   reordering this enum without reordering the table - or vice versa - is a compile error from a
   duplicate/out-of-range index, not a silent mismatch). */
typedef enum
{
    APP_PDU_SENSOR_BASE    = 0u,                    /* sensor slots occupy +0 .. +(APP_NUM_SENSOR_SLOTS-1) */
    APP_PDU_HEARTBEAT      = APP_PDU_SENSOR_BASE + APP_NUM_SENSOR_SLOTS,
    APP_PDU_UDS_FUNCTIONAL,   /* rx: APP_UDS_REQUEST_ID (0x700) / tx: APP_UDS_RESPONSE_ID (0x600) */
    APP_PDU_UDS_PHYSICAL,     /* rx: APP_CAN_RX_ID (0x7E0) / tx: APP_CAN_TX_ID (0x7E8) */
    APP_PDU_NUM_ROUTES
} app_pdu_id_et;

/* TEMP DEBUG TRACE - remove once the CAN-flash hang is found. Read this one value after a hang
   (e.g. Trace32 Var.View app_can_trace_g, or Data.dump &app_can_trace_g) to see how far the CAN
   RX chain got: 1 = ISR wrapper entered, 2 = CANTP_rx_frame_received returned (ISR side done),
   3 = CANTP handed a reassembled frame to PDUR, 4 = PDUR_rx_indication returned, 5 = UDS sent a
   response. If it stops at 1, the hang is inside CANTP_rx_frame_received itself (still in ISR
   context). If it stops at 2, the hang is in CANTP's own tick-driven reassembly (main loop, not
   the ISR) before PDUR ever sees it - despite the ISR context evidence, since that would mean the
   ISR itself returned fine. */
volatile u32_t app_can_trace_g = 0u;

/* Not STATIC - passed to HAL_CAN_set_rx_callback() from main.c, the same way MODE_MGR_tick
   (MODE_MGR.h) is referenced by name from systick_cfg_s below, just in the opposite direction. */
void app_can_rx_wrapper( u32_t id, u8_t id_type, u8_t* data_p, u8_t dlc )
{
    app_can_trace_g = 1u;
    CANTP_rx_frame_received( &app_cantp_instance_s, id, (CANTP_id_type_et)id_type, data_p, (u16_t)dlc );
    app_can_trace_g = 2u;
}

STATIC void app_cantp_send_wrapper( CANTP_can_msg_format_st* msg_p )
{
    if( msg_p != NULL_P )
    {
        (void)HAL_CAN_send_frame( msg_p->Id, (u8_t)msg_p->id_type, msg_p->Data, msg_p->DLC );
    }
}

/* CANTP_tx_request()'s message_sent_noti_p shape - fires once a queued transfer genuinely finishes
   (PASS) or times out waiting for Flow Control (FAIL), asynchronously from CANTP_tick(), not from
   the PDUR_tx() call that queued it. Only tells us the physical id back, so PDUR_lookup_tx_pdu_id()
   resolves which route that was before forwarding to PDUR_tx_confirmation() - see PDUR_pdu_id_t's
   comment in PDUR.h for why PDUR itself never sees a physical id directly. */
STATIC void app_cantp_tx_confirmation( u32_t id, pass_fail_et status )
{
    PDUR_pdu_id_t pdu_id = PDUR_lookup_tx_pdu_id( id, PDUR_MEDIUM_CAN_STD );
    PDUR_tx_confirmation( pdu_id, status );
}

/* PDUR's lower_layer_tx_func_t shape - routes UDS responses through CANTP so multi-frame
   responses get segmented, matching FBL's fbl_cantp_tx_request. Always requests TP (ISO-TP)
   framing: both routes that use this function are UDS request/response pairs - see
   PDUR_lower_layer_tx_func_t's comment in PDUR.h for why that's a per-route function choice
   instead of a runtime parameter. Returns CANTP_tx_request()'s own accept/reject instead of
   discarding it, and wires app_cantp_tx_confirmation as the completion notify CANTP already
   supported but nothing previously registered. */
STATIC pass_fail_et app_cantp_tx_request_wrapper( u32_t id, PDUR_medium_et medium, u8_t* data_p, u16_t len )
{
    return( CANTP_tx_request( &app_cantp_instance_s, id, (CANTP_id_type_et)medium, TP, data_p, len, app_cantp_tx_confirmation ) );
}

/* Adapter: CANTP_message_rx_func_p hands PDUR a physical CAN ID - PDUR_rx_indication() no longer
   accepts one (see PDUR_pdu_id_t's comment in PDUR.h), so this resolves it to a logical route via
   PDUR_lookup_rx_pdu_id() first. STANDARD_ID/EXTENDED_ID (0/1) line up numerically with
   PDUR_MEDIUM_CAN_STD/_CAN_EXT (0/1), so the cast carries the right value without a translation
   table. A frame CANTP hands up always matches one of pdur_routing_table_s's two UDS rx_ids (that's
   the only reason CANTP called back at all), so the lookup can't genuinely miss here - but
   PDUR_rx_indication() bounds-checks anyway, so a miss would just no-op rather than misbehave. */
STATIC void app_cantp_rx_indication_wrapper( u32_t id, CANTP_id_type_et id_type, u8_t* data_p, u16_t len )
{
    PDUR_pdu_id_t pdu_id;

    app_can_trace_g = 3u;
    pdu_id = PDUR_lookup_rx_pdu_id( id, (PDUR_medium_et)id_type );
    (void)PDUR_rx_indication( pdu_id, data_p, len );
    app_can_trace_g = 4u;
}

/* Not STATIC: this is UDS_init_cfg_st.tp_send_func_p, assigned directly in main.c's
   app_uds_init_cfg_s (see INTEGRATION_STUBS.h's prototype) rather than through a UDS_func_p_st
   wrapper object - see UDS_init_cfg_st's comment in UDS.h for why that wrapper went away. route_id
   is whatever UDS_rx_indication() was called with for the request this response answers (UDS.c
   just stores and returns it, see UDS_ctrl_st.req_route_id), so a request received via
   APP_PDU_UDS_PHYSICAL gets its reply sent via APP_PDU_UDS_PHYSICAL too, not a single fixed route
   as before. */
void app_uds_tx( UDS_route_id_t route_id, u8_t* data_p, u16_t len )
{
    app_can_trace_g = 5u;
    (void)PDUR_tx( (PDUR_pdu_id_t)route_id, data_p, len );
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
    app_cantp_instance_s.tp_ids[0].req_id        = APP_UDS_REQUEST_ID;
    app_cantp_instance_s.tp_ids[0].resp_id       = APP_UDS_RESPONSE_ID;
    app_cantp_instance_s.tp_ids[1].req_id        = APP_CAN_RX_ID;
    app_cantp_instance_s.tp_ids[1].resp_id       = APP_CAN_TX_ID;
    app_cantp_instance_s.st_min                  = 10u;
    app_cantp_instance_s.rx_block_size           = 10u;
    app_cantp_instance_s.N_Cr                    = CANTP_DEFAULT_N_CR;
    app_cantp_instance_s.N_Bs                    = CANTP_DEFAULT_N_BS;
    app_cantp_instance_s.N_Ar                    = CANTP_DEFAULT_N_AR;
}

/* SessionControl (0x10), SecurityAccess (0x27), ECU Reset (0x11) and TesterPresent (0x3E) are all
   rows in APP's service table now (APP/Src/UDS_CFG/UDS_config.c, see UDS_service_table_st's
   comment in UDS.h for the shape each takes) - SecurityAccess unlocked in EXTENDED session is what
   that table requires before UDS.c will honour a session-control request into PROGRAMMING, the
   PROGRAMMING row's own on_transition callback (uds_handle_programming_session_notify(), same
   file) is what used to be .session_change_notify here, and the old .perform_soft_reset/
   .perform_hard_reset moved into APP's SID 0x11 row (ecu_reset_cfg_s, same file) - see
   UDS_ecu_reset_cfg_st's comment in UDS.h. app_uds_tx (tp_send_func_p) and NULL_P
   (message_received_notify) are now assigned directly in main.c's app_uds_init_cfg_s instead of a
   UDS_func_p_st wrapper object - see UDS_init_cfg_st's comment in UDS.h. */

const PDUR_route_st pdur_routing_table_s[] =
{
    APP_SENSOR_SLOTS( CAN_SENSOR_PDUR_ENTRY )
    /* TX-only PDUR route for the cyclic heartbeat frame */
    [APP_PDU_HEARTBEAT] = { .tx_id = APP_HEARTBEAT_CAN_ID, .lower_layer_tx_func = pdur_hal_can_tx },
    /* Functional (0x700->0x600) and physical (0x7E0->0x7E8) UDS request/response routes - TX goes
       via app_cantp_tx_request_wrapper, which always requests TP (ISO-TP) framing. */
    [APP_PDU_UDS_FUNCTIONAL] = { .rx_id = APP_UDS_REQUEST_ID, .tx_id = APP_UDS_RESPONSE_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = app_cantp_tx_request_wrapper },
    [APP_PDU_UDS_PHYSICAL] = { .rx_id = APP_CAN_RX_ID, .tx_id = APP_CAN_TX_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = app_cantp_tx_request_wrapper },
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

/* One on-event MSG_SCHED entry per sensor slot. MSG_SCHED calls PDUR_tx() with this value directly
   (MSG_SCHED.c), so it's a PDUR_pdu_id_t - APP_PDU_SENSOR_BASE + n - not the physical CAN_SENSOR_BASE_ID
   + n value the route itself carries. Built from the same APP_SENSOR_SLOTS list as
   CAN_SENSOR_PDUR_ENTRY above, so slot n here and slot n's PDUR route are guaranteed to be the same
   n, not just conventionally kept in step by hand. Trailing comma in the macro body, not between
   invocations - see CAN_SENSOR_PDUR_ENTRY's comment. */
#define CAN_SENSOR_MSG_ENTRY( n ) \
    { ( APP_PDU_SENSOR_BASE + (u32_t)(n) ), 0u, 0u, MSG_SCHED_TX_ON_EVENT, can_sensor_get_data },

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
    APP_SENSOR_SLOTS( CAN_SENSOR_MSG_ENTRY )
    //{ APP_PDU_HEARTBEAT, APP_HEARTBEAT_PERIOD_MS, 0u, MSG_SCHED_TX_CYCLIC, app_heartbeat_get_data },
};

const MSG_SCHED_cfg_st msg_sched_cfg_s =
{
    .msg_table      = can_msg_table_s,
    .num_msgs       = (u8_t)( sizeof(can_msg_table_s) / sizeof(can_msg_table_s[0u]) ),
    .get_time_ms_fn = TIME_get_cumulative_run_time_ms,
};

/***************************************************************************************************
**                              CPS — Crank Position Sensor                                        **
**  Bench-test input: a plain square wave, 60 pulses = 1 revolution (CPS_GAP_NONE/total_teeth=60) - **
**  stands in for a real 60-2 wheel's tooth rate without the actual missing-tooth gap, since a       **
**  plain signal generator can't produce one. At this tooth count, Hz numerically ≈ RPM (60 teeth /  **
**  60 sec-per-min cancel out) - e.g. ~800 Hz for 800 RPM idle, ~8000 Hz for 8000 RPM. Swap gap_type **
**  to CPS_GAP_2_MISSING (still 60 total_teeth) once a gap-capable signal source (real sensor, or a  **
**  programmable pulse generator) is available. Pin setup + EXTI3_IRQHandler live in HAL_BRD.c,      **
**  which calls CPS_tooth_event() directly — no generic dispatch layer. Wired directly in main():    **
**  CPS_init(&cps_crank_instance_s, &cps_crank_cfg_s, SystemCoreClock), which itself runs AFTER      **
**  HAL_BRD_init() (unlike a plain GPIO peripheral, order here doesn't matter for safety - see       **
**  interrupt_enable_func_p below). Ticked via CPS_tick() from MODE_MGR. Watch                        **
**  cps_crank_instance_s.rpm live in the debugger, or call CPS_get_rpm().                            **
**  CPS_tooth_event() does not NULL/state-guard instance_p (see its own doc, CPS.c) - that's traded  **
**  for HAL_BRD_init() leaving EXTI3's NVIC line disabled (EXTI itself still armed, so a real edge    **
**  in the meantime just sets EXTI->PR and waits) and interrupt_enable_func_p below unmasking it      **
**  only once CPS_init() has fully finished - order-independent by construction, not by convention.  **
***************************************************************************************************/
CPS_instance_st cps_crank_instance_s;

/* Mask/unmask just the crank tooth-edge line (EXTI3) around CPS_tick()'s ring-buffer read —
 * narrower than a global __disable_irq(), so it doesn't add latency to any other interrupt
 * in the system. See critical_enter_func_p/critical_exit_func_p doc in CPS.h: without this,
 * a tooth edge landing mid-average (very likely at high input frequencies, since
 * CPS_tooth_event() runs at priority 0 and can preempt CPS_tick() at any point) corrupts
 * the RPM average with a torn mix of old/new samples. */
STATIC void cps_crank_critical_enter( void )
{
    NVIC_DisableIRQ( EXTI3_IRQn );
}

STATIC void cps_crank_critical_exit( void )
{
    NVIC_EnableIRQ( EXTI3_IRQn );
}

const CPS_cfg_st cps_crank_cfg_s =
{
    .total_teeth                     = 60u,           /* stand-in for a 60-2 wheel's tooth rate */
    .gap_type                        = CPS_GAP_NONE,   /* plain square wave, no gap to sync on */
    .capture_edge                    = CPS_EDGE_RISING,
    .filter_depth                    = CPS_RPM_FILTER_DEPTH_MAX, /* max averaging (8 samples) —
                                                 * smooths out sample-to-sample jitter at high
                                                 * input frequencies */
    .stall_timeout_us                = 500000u,
    .rpm_max_credible                = 0xFFFFFFFFu, /* deliberately unclamped - bench-testing to
                                                 * find the sensor/ISR's real ceiling, a credible-
                                                 * range reject would hide exactly the reading being
                                                 * looked for. Put a real ceiling back once this
                                                 * engine's actual redline is known. */
    .get_timer_ticks_func_p          = DWT_get_count, /* raw cycle counter — no conversion in the ISR */
    .revolution_sync_callback_func_p = NULL_P, /* never fires under CPS_GAP_NONE — no gap to find */
    .stall_callback_func_p           = NULL_P,
    .rpm_implausible_callback_func_p = NULL_P, /* rpm_max_credible is unclamped above, so this can
                                                 * never actually fire — wire it up once a real
                                                 * credible ceiling is set */
    .critical_enter_func_p           = cps_crank_critical_enter,
    .critical_exit_func_p            = cps_crank_critical_exit,
    .interrupt_enable_func_p         = HAL_BRD_cps_crank_interrupt_enable, /* NVIC starts disabled
                                                 * in HAL_BRD_init() specifically so this can be the
                                                 * thing that arms it, once CPS_init() has finished -
                                                 * see CPS.h's interrupt_enable_func_p doc */
};

/****************************** END OF FILE *******************************************************/