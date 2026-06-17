/***************************************************************************************************
**                              INTEGRATION_STUBS                                                 **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
#include "HAL_BRD.h"
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

/***************************************************************************************************
**                              APP HEADER                                                        **
**  Placed in .app_header section at APP_FLASH_START (0x08005000).                               **
**  The FBL checks the presence_pattern before jumping to APP.                                    **
**  The crc field is patched by app_crc_injector after linking.                                   **
***************************************************************************************************/
typedef struct {
    u32_t presence_pattern;
    u32_t crc;
    u8_t  reserved[248];
} APP_header_st;

__attribute__((section(".app_header"), used))
const APP_header_st app_header_s = {
    .presence_pattern = 0xDEADBEEFU,
    .crc              = 0x00000000U,
};

/***************************************************************************************************
**                              SYSTICK                                                           **
***************************************************************************************************/
static volatile u8_t run_flag_s = FALSE;

static void app_on_tick( void )
{
    run_flag_s = TRUE;
}

bool_et app_tick_pending( void )
{
    bool_et pending = (bool_et)run_flag_s;
    run_flag_s = FALSE;
    return pending;
}

const SYSTICK_cfg_st systick_cfg_s =
{
    .timeout_ms     = 10u,
    .systick_func_p = app_on_tick
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
const BTN_MGR_func_table_st btm_mgr_func_table_s[2] =
{
    { "DBG_S1", FALSE, HAL_BRD_read_S1_pin, MODE_MGR_ccw_scroll_cbk, NULL_P },
    { "DBG_S2", FALSE, HAL_BRD_read_S2_pin, MODE_MGR_cw_scroll_cbk,  NULL_P },
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
    .zero_pulse_func_p  = HAL_BRD_WS2811_zero_pulse_direct,
    .one_pulse_func_p   = HAL_BRD_WS2811_one_pulse_direct,
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
NRF24_instance_st nrf24_instance_s =
{
    .ce_pin_func_p         = HAL_BRD_NRF24_set_ce_pin_state,
    .cs_pin_func_p         = HAL_BRD_NRF24_spi_slave_select,
    .spi_func_p            = HAL_SPI1_write_and_read_data,
    .us_delay_func_p       = DWT_delay_us,
    .packet_tx_conf_func_p = RF_MGR_tx_complete,
    .packet_rx_func_p      = RF_MGR_get_rx_frame
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
**                              CTRL_AXIS                                                         **
***************************************************************************************************/
const CTRL_AXIS_cfg_st steering_axis_cfg_s =
{
    .min_counts           = 7u,
    .max_counts           = 3560u,
    .centre_counts        = 2020u,
    .deadband_counts      = 80u,
    .input_inverted       = TRUE,
    .output_limit_percent = 40u,
    .trim_cfg             = { .trim_enabled = FALSE }
};

const CTRL_AXIS_func_table_st steering_axis_funcs_s =
{
    .read_adc_func_p      = HAL_ADC_measure_steering_input,
    .read_trim_adc_func_p = HAL_ADC_measure_steering_trim,
};

CTRL_AXIS_instance_st steering_axis_s;

const CTRL_AXIS_cfg_st throttle_axis_cfg_s =
{
    .min_counts           = 1100u,
    .max_counts           = 2900u,
    .centre_counts        = 2020u,
    .deadband_counts      = 80u,
    .input_inverted       = TRUE,
    .output_limit_percent = 100u,
    .trim_cfg             = { .trim_enabled = FALSE }
};

const CTRL_AXIS_func_table_st throttle_axis_funcs_s =
{
    .read_adc_func_p      = HAL_ADC_measure_throttle_input,
    .read_trim_adc_func_p = NULL_P,
};

CTRL_AXIS_instance_st throttle_axis_s;

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
static u8_t  esp01_rx_buf_s[ESP01_MAX_RX_SIZE];
static u16_t esp01_rx_idx_s = 0u;

void esp01_uart_byte_rx( u8_t byte )
{
    if( esp01_rx_idx_s < ESP01_MAX_RX_SIZE )
    {
        esp01_rx_buf_s[esp01_rx_idx_s++] = byte;

        if( ( esp01_rx_idx_s >= 2u ) &&
            ( esp01_rx_buf_s[esp01_rx_idx_s - 2u] == '\r' ) &&
            ( esp01_rx_buf_s[esp01_rx_idx_s - 1u] == '\n' ) )
        {
            ESP01_uart_frame_ready_callback( esp01_rx_buf_s, esp01_rx_idx_s );
            esp01_rx_idx_s = 0u;
        }
    }
    else
    {
        esp01_rx_idx_s = 0u;
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
    .rx_callback_p            = tb_mqtt_rx_handler,
    .send_complete_callback_p = on_wifi_send_complete,
};

/***************************************************************************************************
**                              TB (ThingsBoard)                                                 **
***************************************************************************************************/
TB_config_st tb_cfg_s =
{
    .broker           = "your-tb-host.com",
    .port             = 1883u,
    .client_id        = "HUB_DEVICE",
    .token            = "YOUR_ACCESS_TOKEN",
    .send_func_p      = WIFI_send,
    .telemetry_topics = tb_telemetry_topics_s,
    .num_telemetry    = 0u,   /* set by TB_CBK_init() */
    .rpc_handlers     = tb_rpc_handlers_s,
    .num_rpc_handlers = 0u,   /* set by TB_CBK_init() */
};

/****************************** END OF FILE *******************************************************/
