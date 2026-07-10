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
#include "HAL_UART.h"
#include "MODE_MGR.h"
#include "RF_MGR.h"
#include "PERSIST_BLK.h"
#include "FLS_STM32F1.h"
#include "nvic_driver.h"
#include "TB_CBK.h"
#include "TJA1051.h"
#include "CPS.h"

/***************************************************************************************************
**                              APP HEADER                                                        **
**  Placed in .app_header section at APP_FLASH_START (0x08005000).                               **
**  The FBL checks the presence_pattern before jumping to APP.                                    **
**  The crc field is patched by app_crc_injector after linking.                                   **
***************************************************************************************************/
typedef struct 
{
    u32_t presence_pattern;
    u32_t crc;
    u8_t  reserved[248];
} APP_header_st;

__attribute__((section(".app_header"), used)) const APP_header_st app_header_s = 
{
    .presence_pattern = 0xDEADBEEFU,
    .crc              = 0x00000000U,
};

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

/***************************************************************************************************
**                              ROTARY_MGR                                                        **
***************************************************************************************************/
const ROTARY_MGR_func_p_st rotary_mgr_func_table_s =
{
    .rotary_mgr_ccw_func = MODE_MGR_ccw_scroll_cbk,
    .rotary_mgr_cw_func  = MODE_MGR_cw_scroll_cbk,
    .get_cnt_func_p      = HAL_TIM_ENC_get_counter,
    .get_dir_func_p      = HAL_TIM_ENC_get_dir
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
**                              ST7567                                                            **
***************************************************************************************************/
const ST7567_func_table_st st7567_func_table_s =
{
    .run_func_p     = NULL_P,
    .a0_pin_func_p  = HAL_BRD_set_lcd_a0_pin,
    .cs_pin_func_p  = HAL_BRD_set_lcd_cs_pin,
    .rst_pin_func_p = HAL_BRD_set_lcd_rst_pin,
    .spi_func_p     = HAL_SPI1_send_byte,
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

/* Must be strictly > 1 tick period: timestamp can be stale by up to 1 tick, so an exact
   1x window can fire with near-zero actual silence. +1ms guarantees at least 1 full tick
   of real inter-byte gap before the callback fires. */
#define ESP01_INTER_BYTE_TIMEOUT_MS     ( APP_TIMER_TICK_RATE_MS + 1u )

static u8_t                    esp01_rx_buf_s[ESP01_MAX_RX_SIZE];
static volatile u16_t          esp01_rx_len_s       = 0u;
static volatile u64_t          esp01_last_byte_ms_s = 0u;
static volatile false_true_et  esp01_rx_active_s    = FALSE;

/* Called from UART ISR - buffer the byte and record when it arrived */
OPTIMISE_O3 HOT void esp01_uart_byte_rx( u8_t byte )
{
    if( esp01_rx_len_s < ESP01_MAX_RX_SIZE )
    {
        esp01_rx_buf_s[esp01_rx_len_s++] = byte;
    }
    else
    {
        esp01_rx_len_s = 0u;
    }

    esp01_last_byte_ms_s = TIME_get_cumulative_run_time_ms();
    esp01_rx_active_s    = TRUE;
}

/* Called from tick context (before ESP01_tick) - fires callback once the inter-byte gap expires */
void esp01_check_rx_timeout( void )
{
    if( ( esp01_rx_active_s == TRUE ) &&
        ( TIME_has_time_elapsed_ms( esp01_last_byte_ms_s, ESP01_INTER_BYTE_TIMEOUT_MS ) == TRUE ) )
    {
        ESP01_uart_frame_ready_callback( esp01_rx_buf_s, esp01_rx_len_s );
        esp01_rx_len_s    = 0u;
        esp01_rx_active_s = FALSE;
    }
}

static void esp01_uart_tx( u8_t* data_p, u16_t len )
{
    HAL_USART2_send_data( (const char*)data_p, len );
}

const ESP01_cfg_st esp01_cfg_s =
{
    .initial_mode                 = ESP01_STA,
    .uart_tx_func_p               = esp01_uart_tx,
    .network_packet_rx_callback_p = WIFI_esp01_rx_handler,
};

/***************************************************************************************************
**                              WIFI                                                             **
***************************************************************************************************/
static void on_wifi_send_complete( pass_fail_et result )
{
    (void)result;
    TB_notify_event( TB_EVENT_SEND_COMPLETE );
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
    .rssi_polling_enabled     = FALSE,
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
STATIC void pdur_hal_can_tx( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len, u8_t channel )
{
    (void)id_type;
    (void)frame_type;
    (void)channel;
    HAL_CAN_send_frame( id, data_p, (u8_t)len );
}

/* Base CAN ID for sensor telemetry frames — slot N uses ID (base + N) */
#define CAN_SENSOR_BASE_ID  ( 0x100u )

/* One TX-only PDUR route per sensor slot */
#define CAN_SENSOR_PDUR_ENTRY( n ) \
    { 0u, 0xFFFFFFFFu, ( CAN_SENSOR_BASE_ID + (u32_t)(n) ), 0u, 0u, 0u, NULL_P, pdur_hal_can_tx, NULL_P }

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