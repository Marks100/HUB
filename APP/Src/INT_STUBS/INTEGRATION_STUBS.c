/***************************************************************************************************
**                              INTEGRATION_STUBS                                                 **
**  Generic board/peripheral wiring for this project. The bench vehicle-simulation stack (APS      **
**  pedal, CPS crank sensor, TCU/GKT_SHIFTER/MQB_CLUSTER gearbox+dash, ENGINE_SIM) lives in its     **
**  own file - see APP/Src/VEHICLE_SIM_CFG/VEHICLE_SIM_config.c - split out from here since it's a  **
**  cohesive concern of its own, distinct from plain HAL/peripheral bring-up. The pieces that        **
**  stayed are pdur_routing_table_s's APP_PDU_GKT_LEVER_RX/APP_PDU_GKT_HEARTBEAT_RX routes below -   **
**  splitting a couple of rows out of that array (which also carries the unrelated sensor/UDS        **
**  routes) would fragment one cohesive table across two files for no real gain, so only their       **
**  handlers (app_pdur_gkt_lever_rx()/app_pdur_gkt_heartbeat_rx(), non-STATIC) live in                **
**  VEHICLE_SIM_config.c instead - both real CAN IDs the physical shifter sends route to             **
**  GKT_SHIFTER's own gateway from there.                                                            **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
#include "APP_PDU_IDS.h"       /* app_pdu_id_et - the route ids pdur_routing_table_s is indexed by */
#include "VEHICLE_SIM_config.h"   /* app_pdur_gkt_lever_rx() - see this file's own header comment */
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "PDUR.h"
#include "MSG_SCHED.h"
#include "WIFI.h"
#include "GKT_SHIFTER.h" 
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
    .enc_cnt_max            = 0xFFFFu,              /* TIM4 is 16-bit, ARR = 0xFFFF (HAL_TIM4_init_encoder) */
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

    /* Feedback - the panel beeps this board's buzzer for the gestures enabled below. beep_ms is
       only the power-on value; MENU_NAV's Buzzer screen changes it at runtime via
       HMI_SH1106_set_beep_ms(). */
    .buzzer_p               = &buzzer_instance_s,
    .beep_on_rotate         = TRUE,
    .beep_on_press          = TRUE,
    .beep_ms                = BUZZER_SHORT_BEEP_MS,

    /* Both straight to MENU_NAV - nothing board-specific to add to either */
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

/* APP_CAN_CH_MAIN's transceiver - a second bus gets its own instance (and func table for its pins) */
TJA1051_instance_st tja1051_main_instance_s;

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
/* Base CAN ID for sensor telemetry frames — slot N uses ID (base + N) */
#define CAN_SENSOR_BASE_ID  ( 0x100u )

/* Single source of truth for which sensor slots exist. pdur_routing_table_s and can_msg_table_s
   (below) are two separate arrays that both need one entry per slot in the same order - before this
   macro they were two independently hand-written lists of CAN_SENSOR_PDUR_ENTRY(n)/
   CAN_SENSOR_MSG_ENTRY(n) calls that had to be edited in lockstep by hand, with nothing catching a
   slot added to one but not the other. Expanding both tables from this one list instead means
   adding/removing a slot is a one-line change here, not a two-place edit that can silently drift.
   Must still be kept equal to APP_NUM_SENSOR_SLOTS (APP_PDU_IDS.h) - the enum there needs a plain
   integer, which can't be derived from this list without more preprocessor machinery than 12 fixed
   slots justify. */
#define APP_SENSOR_SLOTS( ENTRY ) \
    ENTRY(0u)  ENTRY(1u)  ENTRY(2u)  ENTRY(3u)  \
    ENTRY(4u)  ENTRY(5u)  ENTRY(6u)  ENTRY(7u)  \
    ENTRY(8u)  ENTRY(9u)  ENTRY(10u) ENTRY(11u)

/* APP_NUM_SENSOR_SLOTS (APP_PDU_IDS.h) must match the number of ENTRY(n) calls in APP_SENSOR_SLOTS
   above. */

/* One TX-only PDUR route per sensor slot - designated array-index initializer so this always lands
   on the matching app_pdu_id_et slot (below) regardless of macro invocation order. Trailing comma
   in the macro body (not between invocations) since APP_SENSOR_SLOTS expands these back-to-back. */
#define CAN_SENSOR_PDUR_ENTRY( n ) \
    [APP_PDU_SENSOR_BASE + (n)] = { .tx_id = ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), .lower_layer_tx_func = CANIF_pdur_normal_tx },

/* One TX-only PDUR route per MQB_CLUSTER frame - designated array-index initializer, so each lands on
   the slot MQB_CLUSTER_tx_msg_et says it belongs to (APP_PDU_MQB_TX_BASE + enumerator, exactly what
   MQB_CLUSTER sends to via mqb_cluster_cfg_s.tx_pdu_id_base). Trailing comma in the macro body, same as
   CAN_SENSOR_PDUR_ENTRY, since MQB_CLUSTER_TX_MSG_LIST expands these back-to-back. */
#define APP_MQB_PDUR_ENTRY( msg, can_id ) \
    [APP_PDU_MQB_TX_BASE + (msg)] = { .tx_id = (can_id), .lower_layer_tx_func = CANIF_pdur_normal_tx },

/* UDS diagnostics over CAN-TP - functional (0x700/0x600) and physical (0x7E0/0x7E8) request/
   response pair, same IDs FBL uses (FBL/Src/UDS_CFG/UDS_config.h) - safe to reuse rather than
   needing a second set, since BM only ever runs one of APP/FBL at a time, never both, so a tester
   never has to know which stage it's actually talking to. */
#define APP_UDS_REQUEST_ID   ( 0x700u )
#define APP_UDS_RESPONSE_ID  ( 0x600u )
#define APP_CAN_RX_ID        ( 0x7E0u )
#define APP_CAN_TX_ID        ( 0x7E8u )

/* APP's logical PDU IDs (app_pdu_id_et) live in APP_PDU_IDS.h - see there. */

/* No adapter functions between the CAN layers - every signature lines up:
   - HAL_CAN RX ISR -> CANIF_rx_indication() directly (HAL_CAN_set_rx_callback() in main.c, with this
     controller's channel), CANIF -> the channel's CANTP instance.
   - CANTP -> PDUR: each instance's rx_indication_func_p is PDUR_rx_indication_physical() (every
     reassembled TP message AND every non-TP frame on the bus - one whose id isn't in
     pdur_routing_table_s on that channel is a silent no-op, the normal case for most traffic).
   - PDUR -> CANTP: CAN routes point at CANIF_pdur_tp_tx (UDS request/response - ISO-TP, completion
     back through PDUR_tx_confirmation_physical() to the route's txConfirmation) or
     CANIF_pdur_normal_tx (every plain frame - CANTP's NORMAL queue, so nothing in APP sends to the
     3 bxCAN mailboxes directly and a burst can't lose what doesn't fit).
   - CANTP -> driver: can_tx_func_p is CANIF_transmit. */

/* UDS_init_cfg_st.is_functional_route_fp, assigned in main.c's app_uds_init_cfg_s - see there */
false_true_et app_uds_is_functional_route( UDS_route_id_t route_id )
{
    return( ( route_id == (UDS_route_id_t)APP_PDU_UDS_FUNCTIONAL ) ? TRUE : FALSE );
}

/* UDS_init_cfg_st.tp_send_func_p is assigned PDUR_tx directly in main.c's app_uds_init_cfg_s -
   PDUR_pdu_id_t and UDS_route_id_t are both u16_t and PDUR_tx's pass_fail_et return now matches
   the typedef exactly (see UDS_init_cfg_st's comment in UDS.h), so no adapter is needed here.
   route_id is whatever UDS_rx_indication() was called with for the request this response answers
   (UDS.c just stores and returns it, see UDS_ctrl_st.req_route_id), so a request received via
   APP_PDU_UDS_PHYSICAL gets its reply sent via APP_PDU_UDS_PHYSICAL too, not a single fixed route. */

/* Deliberately NOT a designated initializer: CANTP_instance_st embeds the RX/TX queues and TP
   session arrays (~1.7KB, almost entirely zero) alongside these config fields. Giving the struct
   an initializer with even one non-zero field forces the compiler to store the WHOLE object -
   zero regions included - as flash-resident .data instead of letting the zero-only bulk of it
   land in .bss for free. Left plain (zero-initialized in .bss, same as any other global) and
   configured at runtime instead - see app_cantp_instance_init(), called once from main.c before
   CANTP_init(). .fd_enable (FALSE) needs no explicit assignment - already zero from .bss. */
CANTP_instance_st app_cantp_instance_s;

/* The one message buffer CANTP reassembles requests into and UDS builds responses in - see
   APP_UDS_BUFFER_SIZE in INTEGRATION_STUBS.h for what it is sized for */
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
**  What each CAN channel is made of - pure configuration. Every driver implements CANIF's generic  **
**  interface (CANIF_types.h) and does its own chip-specific mode mapping, so there are no adapters.**
***************************************************************************************************/
/* Row index = channel number (app_can_channel_et, APP_PDU_IDS.h) - also each route's PDUR .channel
   and each CANTP instance's .channel, which CANIF_init() checks. A second bus is one more row: its
   controller driver's three functions (e.g. an MCP2515's), its transceiver's set-mode + instance,
   and its own CANTP instance. */
const CANIF_channel_cfg_st canif_channels_s[APP_CAN_NUM_CHANNELS] =
{
    [APP_CAN_CH_MAIN] = { .send_frame      = HAL_CAN_send_frame,
                          .ctrl_set_mode   = HAL_CAN_set_mode,
                          .ctrl_get_status = HAL_CAN_get_status,
                          .trcv_set_mode   = TJA1051_canif_set_mode,
                          .trcv_inst_p     = &tja1051_main_instance_s,
                          .cantp_p         = &app_cantp_instance_s },
};

/* SessionControl (0x10), SecurityAccess (0x27), ECU Reset (0x11) and TesterPresent (0x3E) are all
   rows in APP's service table now (APP/Src/UDS_CFG/UDS_config.c, see UDS_service_table_st's
   comment in UDS.h for the shape each takes) - SecurityAccess unlocked in EXTENDED session is what
   that table requires before UDS.c will honour a session-control request into PROGRAMMING, the
   PROGRAMMING row's own on_transition callback (uds_handle_programming_session_notify(), same
   file) is what used to be .session_change_notify here, and the old .perform_soft_reset/
   .perform_hard_reset moved into APP's SID 0x11 row (ecu_reset_cfg_s, same file) - see
   UDS_ecu_reset_cfg_st's comment in UDS.h. PDUR_tx (tp_send_func_p) and NULL_P
   (message_received_notify) are now assigned directly in main.c's app_uds_init_cfg_s instead of a
   UDS_func_p_st wrapper object - see UDS_init_cfg_st's comment in UDS.h. */

/* Every route below is on APP_CAN_CH_MAIN by leaving .channel at its zero default. A route for
   another bus sets .channel = APP_CAN_CH_<bus>; the adapters it points at are already channel-aware. */
const PDUR_route_st pdur_routing_table_s[] =
{
    //APP_SENSOR_SLOTS( CAN_SENSOR_PDUR_ENTRY )
    /* RX-only routes (no lower_layer_tx_func) for every real CAN ID the physical F30 shifter itself
       sends. Both go to GKT_SHIFTER (the shifter's CAN gateway - see GKT_SHIFTER.c's own header
       comment), never straight to TCU. TCU's own TCU_GEAR_STATE_CAN_ID broadcast has no matching RX
       route here - see app_tcu_gear_state_local_relay() (VEHICLE_SIM_CFG/VEHICLE_SIM_config.c) for why. */
    [APP_PDU_GKT_LEVER_RX] = { .rx_id = GKT_SHIFTER_STATE_CAN_ID, .upperLayerRxIndication = app_pdur_gkt_lever_rx },
    [APP_PDU_GKT_HEARTBEAT_RX] = { .rx_id = GKT_SHIFTER_HEARTBEAT_CAN_ID, .upperLayerRxIndication = app_pdur_gkt_heartbeat_rx },
    /* Functional (0x700->0x600) and physical (0x7E0->0x7E8) UDS request/response routes - TX goes
       via CANIF_pdur_tp_tx, which always requests TP (ISO-TP) framing.
       UDS_tx_confirmation tells UDS each response has actually gone out - it holds the shared
       buffer and any scheduled reset/FBL entry until then. */
    [APP_PDU_UDS_FUNCTIONAL] = { .rx_id = APP_UDS_REQUEST_ID, .tx_id = APP_UDS_RESPONSE_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = CANIF_pdur_tp_tx,
      .txConfirmation = UDS_tx_confirmation },
    [APP_PDU_UDS_PHYSICAL] = { .rx_id = APP_CAN_RX_ID, .tx_id = APP_CAN_TX_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = CANIF_pdur_tp_tx,
      .txConfirmation = UDS_tx_confirmation },
    /* TX-only routes (no upperLayerRxIndication) for every frame TCU/GKT_SHIFTER/MQB_CLUSTER send.
       Each of those modules keeps its own period/encoder in its own tick() - unlike the sensor
       frames above, which MSG_SCHED schedules - but hands every finished frame to PDUR_tx() with the
       route id from its cfg (VEHICLE_SIM_config.c), so this table is the one place a CAN ID or
       medium is chosen. These go through CANTP's NORMAL TX queue (CANIF_pdur_normal_tx), not
       straight to the hardware - together they send more frames per tick than the CAN controller has
       TX mailboxes. The sensor slot and MQB_CLUSTER routes (disabled below) use the same queue. */
    [APP_PDU_GKT_LED_TX]        = { .tx_id = GKT_SHIFTER_LED_CAN_ID,       .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_GKT_BACKLIGHT_TX]  = { .tx_id = GKT_SHIFTER_BACKLIGHT_CAN_ID, .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_TCU_GEAR_STATE_TX] = { .tx_id = TCU_GEAR_STATE_CAN_ID,        .lower_layer_tx_func = CANIF_pdur_normal_tx },
    [APP_PDU_TCU_DYNAMICS_TX]   = { .tx_id = TCU_DYNAMICS_CAN_ID,          .lower_layer_tx_func = CANIF_pdur_normal_tx },
    /* MQB_CLUSTER's routes are generated from the same list the module builds its own enum from
       (MQB_CLUSTER_TX_MSG_LIST, MQB_CLUSTER.h), so a message added there gets its route here with no
       edit to this table - and the CAN ID can't disagree with the module's. */
    //MQB_CLUSTER_TX_MSG_LIST( APP_MQB_PDUR_ENTRY )
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