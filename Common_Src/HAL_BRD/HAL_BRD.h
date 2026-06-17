#ifndef HAL_BRD_H
#define HAL_BRD_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "MODE_MGR.h"
#include "SYSTICK.h"
#include "HAL_config.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef void(*HAL_BRD_nrf_func_type)( void );

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern u8_t debug_mode;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void        HAL_BRD_init( void );
low_high_et HAL_BRD_read_pin_state( GPIO_TypeDef* port, u16_t pin );
void        HAL_BRD_set_pin_state( GPIO_TypeDef* port, u16_t pin, low_high_et state );
low_high_et HAL_BRD_read_debug_mode_pin( void );
low_high_et HAL_BRD_read_button_2( void );
void        HAL_BRD_reset( void );
void        HAL_BRD_nrf_register_cbk( HAL_BRD_nrf_func_type func_p );

void        HAL_BRD_set_batt_monitor_state( disable_enable_et state );
void        HAL_BRD_set_rf_enable_pin( disable_enable_et state );
void        HAL_BRD_toggle_onboard_led( void );
void        HAL_BRD_set_onboard_led( off_on_et state );
void        HAL_BRD_set_debug_mode_LED( off_on_et state );
void        HAL_BRD_toggle_debug_mode_led( void );
void        HAL_BRD_toggle_pin_state( GPIO_TypeDef* port, u16_t pin );
void        HAL_BRD_RFM69_set_enable_pin_state( low_high_et state );
void        HAL_BRD_RFM69_set_reset_pin_state( low_high_et state );
void        HAL_BRD_NRF24_set_ce_pin_state( low_high_et state );
void        HAL_BRD_set_temp_sensor_enable_pin( off_on_et state );
void        HAL_BRD_set_NRF_power_pin_state( off_on_et state );
void        HAL_BRD_NRF24_spi_slave_select( low_high_et state );
void        HAL_BRD_NRF24_set_ce_pin_state( low_high_et state );
void        HAL_BRD_set_BMP280_power_pin_state( off_on_et state );
low_high_et HAL_BRD_NRF24_read_irq_pin( void );
void        HAL_BRD_setup_pins_for_low_power( void );

void HAL_BRD_get_SW_version_num( u8_t* version_num_p );
void HAL_BRD_get_HW_version_num( u8_t* version_num_p );

/* Button reads */
low_high_et HAL_BRD_read_S1_pin( void );
low_high_et HAL_BRD_read_S2_pin( void );
low_high_et HAL_BRD_read_rotary_sw_pin( void );
low_high_et HAL_BRD_read_lcd_reset_sw_pin( void );

/* LCD control pins */
void HAL_BRD_set_lcd_a0_pin( low_high_et state );
void HAL_BRD_set_lcd_cs_pin( low_high_et state );
void HAL_BRD_set_lcd_rst_pin( low_high_et state );

/* WS2811 bit-bang pulses — call with interrupts disabled, tuned for 72 MHz */
void HAL_BRD_WS2811_zero_pulse_direct( void );
void HAL_BRD_WS2811_one_pulse_direct( void );

void HAL_BRD_toggle_led1_pin( void );

#endif
