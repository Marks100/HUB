/*! \file
*               Author: mstewart
*   \brief      HAL_BRD module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_BRD.h"
#include "CPS.h"

/* Owned by INTEGRATION_STUBS.c — redeclared here (rather than pulling in the whole
 * INTEGRATION_STUBS.h chain) so the ABS input ISRs can call CPS_tooth_event() directly,
 * with zero indirection/trampoline between the edge and the driver. */
extern CPS_instance_st cps_instance_s;
extern CPS_instance_st cps_instance_2_s;

#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
STATIC HAL_BRD_nrf_func_type HAL_BRD_nrf_func_p;
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

/*!
****************************************************************************************************
*
*   \brief         Initialise the Pins,
*   			   lets just do gpio pins here and let other modules handle themselves
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_init( void )
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Establish the board's interrupt priority scheme before any NVIC_Init() call.
	 * Full 4 preemption-priority bits, 0 subpriority bits — every peripheral gets a
	 * distinct preemption level rather than an undefined tie at the NVIC's reset default.
	 *   0 (highest) - ABS wheel-speed sensor inputs (EXTI3, EXTI9_5, configured below)
	 *   1           - every other peripheral ISR (see HAL_TIM.c, HAL_CAN.c, HAL_UART.c)
	 *   lowest      - SysTick (sets its own priority in SYSTICK_init() — see systick_driver.h) */
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );

	RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO,  ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );

	/* Disable the JTAG as this saves us some pins :) */
	GPIO_PinRemapConfig( GPIO_Remap_SWJ_JTAGDisable, ENABLE );

	/* Configure the GPIOs */
	/* SH1106 panel buttons - pulled up internally, switch pulls to GND, so LOW means pressed.
	   All three share a port and mode, so one struct fill covers them. */
	GPIO_InitStructure.GPIO_Pin   = PANEL_SELECT_BTN_PIN | PANEL_CONFIRM_BTN_PIN | PANEL_BACK_BTN_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( PANEL_SELECT_BTN_PORT, &GPIO_InitStructure );

	/* Rotary encoder channels - TIM4 CH1/CH2 in quadrature mode. HAL_TIM4_init_encoder() sets up
	   the timer but deliberately leaves the pins to the board layer, and this init runs before it.
	   Pulled up rather than left floating: a mechanical encoder switches its channels to the
	   common GND pin, so with a floating input an open contact has no defined level and the
	   counter picks up noise instead of detents. If the encoder module carries its own pull-ups
	   the internal ones simply sit in parallel, which is harmless. */
	GPIO_InitStructure.GPIO_Pin   = ENC_CH1_PIN | ENC_CH2_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( ENC_PORT, &GPIO_InitStructure );
	
#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
	/* Configure the NRF24 CS pin */
	GPIO_InitStructure.GPIO_Pin = NRF_CS_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( NRF_CS_PORT, &GPIO_InitStructure );
	HAL_BRD_NRF24_spi_slave_select( LOW );

	/* Configure the NRF24 CE pin */
	GPIO_InitStructure.GPIO_Pin = NRF_CE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( NRF_CE_PORT, &GPIO_InitStructure );
	HAL_BRD_NRF24_set_ce_pin_state( LOW );
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

	/* Configure the GPIO_LED pin and set LOW immediately */
	GPIO_InitStructure.GPIO_Pin = ONBOARD_LED_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( ONBOARD_LED_PORT, &GPIO_InitStructure );
	HAL_BRD_set_onboard_led( OFF );
}

/*!
****************************************************************************************************
*
*   \brief         Resets the board
*
*   \author        MS
*
*   \return        low_high_et
*
***************************************************************************************************/
void HAL_BRD_TJA1051_set_en_pin( low_high_et state )
{
	HAL_BRD_set_pin_state( TJA1051_EN_PORT, TJA1051_EN_PIN, state );
}

void HAL_BRD_reset( void )
{
	NVIC_SystemReset();
}

#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
/***************************************************************************************************
**                              NRF24 (SUPER_PILL only - no radio on BLUE_PILL)                    **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Registers the function to call for the NRF24 ISR
*
*   \author        MS
*
*   \return        low_high_et
*
***************************************************************************************************/
void HAL_BRD_nrf_register_cbk( HAL_BRD_nrf_func_type func_p )
{
	HAL_BRD_nrf_func_p = func_p;
}

/*!
****************************************************************************************************
*
*   \brief         Reads Any PORT and any PIN
*
*   \author        MS
*
*   \return        low_high_et state of the PIN
*
***************************************************************************************************/
low_high_et HAL_BRD_read_pin_state( GPIO_TypeDef* port, u16_t pin )
{
	low_high_et returnType = (low_high_et)GPIO_ReadInputDataBit( port, pin );

	return( returnType );
}

/*!
****************************************************************************************************
*
*   \brief         SETS Any PIN on any PORT
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_set_pin_state( GPIO_TypeDef* port, u16_t pin, low_high_et state )
{
	GPIO_WriteBit( port, pin, (BitAction)state );
}

/*!
****************************************************************************************************
*
*   \brief         Toggles Any PIN on any PORT
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_toggle_pin_state( GPIO_TypeDef* port, u16_t pin )
{
    /* Firstly read the PIN state */
	low_high_et state = HAL_BRD_read_pin_state( port, pin );

    HAL_BRD_set_pin_state( port, pin, !state );
}

/*!
****************************************************************************************************
*
*   \brief         Sets thestate of the onboard LED
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_set_onboard_led( off_on_et state )
{	
	HAL_BRD_set_pin_state( ONBOARD_LED_PORT, ONBOARD_LED_PIN, (low_high_et)state );
}

/*!
****************************************************************************************************
*
*   \brief         Toggles the onboard LED
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_toggle_onboard_led( void )
{
	HAL_BRD_toggle_pin_state( ONBOARD_LED_PORT, ONBOARD_LED_PIN );
}

/*!
****************************************************************************************************
*
*   \brief         Sets the rf CE pin state
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_NRF24_set_ce_pin_state( low_high_et state )
{
	HAL_BRD_set_pin_state( NRF_CE_PORT, NRF_CE_PIN, (low_high_et)state );
}

/*!
****************************************************************************************************
*
*   \brief         Sets the SPI chip select pin state for the RF module
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_NRF24_spi_slave_select( low_high_et state )
{
	HAL_BRD_set_pin_state( NRF_CS_PORT, NRF_CS_PIN, (low_high_et)state );
}

/*!
****************************************************************************************************
*
*   \brief         Reads the state of the IRQ Pin for the NRF module
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
low_high_et HAL_BRD_NRF24_read_irq_pin( void )
{
	low_high_et state = HAL_BRD_read_pin_state( NRF_IRQ_PORT, NRF_IRQ_PIN );

	return( state );
}
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

/*!
****************************************************************************************************
*
*   \brief         Reads the state of the IRQ Pin for the NRF module
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_setup_pins_for_low_power( void )
{
	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( GPIOA, &GPIO_InitStructure );
	GPIO_Init( GPIOB, &GPIO_InitStructure );
	GPIO_Init( GPIOC, &GPIO_InitStructure );
}

#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
/*!
****************************************************************************************************
*
*   \brief         Reads the state of the onboard button
*
*   \author        MS
*
*   \return        low_high_et state of the onboard button pin
*
*   \note          SUPER_PILL only - BLUE_PILL has no ONBOARD_BTN_PORT/PIN defined.
*
***************************************************************************************************/
low_high_et HAL_BRD_read_onboard_btn( void )
{
	return HAL_BRD_read_pin_state( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN );
}
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */

/*!
****************************************************************************************************
*
*   \brief         SH1106 panel button reads
*
*   \author        MS
*
*   \return        low_high_et raw pin state - LOW while pressed, since the switches pull to GND
*                  against the internal pull-up. Register these with inverted = TRUE so BTN_MGR
*                  reads them the right way round.
*
***************************************************************************************************/
low_high_et HAL_BRD_read_panel_select_btn( void )
{
	return HAL_BRD_read_pin_state( PANEL_SELECT_BTN_PORT, PANEL_SELECT_BTN_PIN );
}

low_high_et HAL_BRD_read_panel_confirm_btn( void )
{
	return HAL_BRD_read_pin_state( PANEL_CONFIRM_BTN_PORT, PANEL_CONFIRM_BTN_PIN );
}

low_high_et HAL_BRD_read_panel_back_btn( void )
{
	return HAL_BRD_read_pin_state( PANEL_BACK_BTN_PORT, PANEL_BACK_BTN_PIN );
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 0 )
*
*   \author        MS
*
*   \return        low_high_et
*
***************************************************************************************************/
void EXTI0_IRQHandler(void)
{
	/* Make sure that interrupt flag is set */
	if ( EXTI_GetFlagStatus( EXTI_Line0 ) != RESET )
	{
		/* Now we keep track of the interrupt edge */
		/* Clear interrupt flag */
		EXTI_ClearITPendingBit( EXTI_Line0 );
	}
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 1 )
*
*   \author        MS
*
*   \return        low_high_et
*
***************************************************************************************************/
void EXTI1_IRQHandler(void)
{
	/* Make sure that interrupt flag is set */
	if ( EXTI_GetFlagStatus( EXTI_Line1 ) != RESET )
	{
		/* Now we keep track of the interrupt edge */
		/* Clear interrupt flag */
		EXTI_ClearITPendingBit( EXTI_Line1 );
	}
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 10 - 15 )
*
*   \author        MS
*
*   \return        low_high_et
*
***************************************************************************************************/
void EXTI15_10_IRQHandler(void)
{
#if ( HW_VARIANT == HW_VARIANT_SUPER_PILL )
	if( EXTI_GetFlagStatus( NRF24_IRQ_EXT_LINE ) != RESET )
	{
		if( HAL_BRD_nrf_func_p != NULL_P )
		{
			/* Execute the callback */
			HAL_BRD_nrf_func_p();

			/* Unregister the callback again */
			HAL_BRD_nrf_func_p = NULL_P;
		}

		EXTI_ClearITPendingBit( NRF24_IRQ_EXT_LINE );
	}
#endif /* HW_VARIANT == HW_VARIANT_SUPER_PILL */
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 3 ) — ABS #1 input pin
*
*   \author        MS
*
*   \return        none
*
*   \note          Calls CPS_tooth_event() directly, by name — no registered callback, no
*                  dispatch table, no trampoline. This pin is dedicated to ABS #1, so there
*                  is nothing generic to abstract.
*   \note          Direct EXTI->PR access (write-1-to-clear) instead of the SPL's
*                  EXTI_ClearITPendingBit() — this build has no LTO, so that would be a real,
*                  avoidable non-inlined function call on the hottest ISR in the system.
*
***************************************************************************************************/
void EXTI3_IRQHandler(void)
{
	//EXTI->PR = ABS1_INPUT_EXTI_LINE;   /* write-1-to-clear */
	//CPS_tooth_event( &cps_instance_s );
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 5-9, shared vector ) — ABS #2 input pin
*
*   \author        MS
*
*   \return        none
*
*   \note          Calls CPS_tooth_event() directly, same zero-indirection reasoning as
*                  EXTI3_IRQHandler above. This is nominally a shared vector (lines 5-9),
*                  but nothing else in this system uses any of those lines, so it is
*                  unconditionally treated as line 9 — no need to check EXTI->PR for other
*                  bits that will never be set.
*
***************************************************************************************************/
void EXTI9_5_IRQHandler(void)
{
	//EXTI->PR = ABS2_INPUT_EXTI_LINE;   /* write-1-to-clear */
	//CPS_tooth_event( &cps_instance_2_s );
}

/****************************** END OF FILE *******************************************************/

