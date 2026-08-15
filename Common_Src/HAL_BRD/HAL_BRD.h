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
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void        HAL_BRD_init( void );
low_high_et HAL_BRD_read_pin_state( GPIO_TypeDef* port, u16_t pin );
void        HAL_BRD_set_pin_state( GPIO_TypeDef* port, u16_t pin, low_high_et state );
void        HAL_BRD_toggle_pin_state( GPIO_TypeDef* port, u16_t pin );
void        HAL_BRD_reset( void );
void        HAL_BRD_toggle_onboard_led( void );
void        HAL_BRD_set_onboard_led( off_on_et state );
void        HAL_BRD_setup_pins_for_low_power( void );

#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
/*!< NRF24 radio + onboard button - SUPER_PILL only, no radio or dedicated button on BLUE_PILL. */
void        HAL_BRD_nrf_register_cbk( HAL_BRD_nrf_func_type func_p );
void        HAL_BRD_NRF24_set_ce_pin_state( low_high_et state );
void        HAL_BRD_NRF24_spi_slave_select( low_high_et state );
low_high_et HAL_BRD_NRF24_read_irq_pin( void );
low_high_et HAL_BRD_read_onboard_btn( void );
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

/*!< SH1106 panel buttons - LOW while pressed (pull-up + switch to GND), so register these with
 *   inverted = TRUE. */
low_high_et HAL_BRD_read_panel_select_btn( void );
low_high_et HAL_BRD_read_panel_confirm_btn( void );
low_high_et HAL_BRD_read_panel_back_btn( void );

/* TJA1051 CAN transceiver */
void HAL_BRD_TJA1051_set_en_pin( low_high_et state );

/* WS2811 bit-bang pulses — call with interrupts disabled, tuned for 72 MHz */
void HAL_BRD_WS2811_zero_pulse_direct( void );
void HAL_BRD_WS2811_one_pulse_direct( void );

#endif
