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
 * INTEGRATION_STUBS.h chain) so the CPS input ISR can call CPS_tooth_event() directly,
 * with zero indirection/trampoline between the edge and the driver. */
extern CPS_instance_st cps_instance_s;

STATIC HAL_BRD_nrf_func_type HAL_BRD_nrf_func_p;
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
	 *   0 (highest) - CPS input pin (EXTI2, configured below)
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
	GPIO_InitStructure.GPIO_Pin = PANEL_2_BTN_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( PANEL_2_BTN_PORT, &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin = PANEL_3_BTN_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( PANEL_3_BTN_PORT, &GPIO_InitStructure );
	
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

	/* Configure the GPIO_LED pin and set LOW immediately */
	GPIO_InitStructure.GPIO_Pin = ONBOARD_LED_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( ONBOARD_LED_PORT, &GPIO_InitStructure );
	HAL_BRD_set_onboard_led( OFF );

	/* configure the debug mode led ( this lets us know we are in debug mode and will only be turned
	on in debug mode */
	GPIO_InitStructure.GPIO_Pin = DEBUG_MODE_LED_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( DEBUG_MODE_LED_PORT, &GPIO_InitStructure );

	/* TJA1051 EN pin — hold low (standby) until CAN_MGR_init() drives it high */
	GPIO_InitStructure.GPIO_Pin   = TJA1051_EN_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( TJA1051_EN_PORT, &GPIO_InitStructure );
	HAL_BRD_TJA1051_set_en_pin( LOW );

	/* CPS input pin — edge-triggered on EXTI2, serviced directly by EXTI2_IRQHandler below */
	GPIO_InitStructure.GPIO_Pin   = CPS_INPUT_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( CPS_INPUT_PORT, &GPIO_InitStructure );

	GPIO_EXTILineConfig( GPIO_PortSourceGPIOC, GPIO_PinSource2 );

	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line    = CPS_INPUT_EXTI_LINE;
	EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init( &EXTI_InitStructure );

	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel                   = EXTI2_IRQn;
	/* Priority 0 — the highest in the system. Every other peripheral ISR is priority 1+
	 * (see HAL_TIM.c, HAL_CAN.c, HAL_UART.c) and SysTick is priority 2, so this pin can
	 * always preempt them and feed CPS with minimum latency. Requires
	 * NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4) to have already run (see main.c). */
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init( &NVIC_InitStruct );
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
*   \brief         Sets the state of the debug mode the led
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_set_debug_mode_LED( off_on_et state )
{
	HAL_BRD_set_pin_state( DEBUG_MODE_LED_PORT, DEBUG_MODE_LED_PIN, (low_high_et)state );
}

/*!
****************************************************************************************************
*
*   \brief         Toggles the debug led
*
*   \author        MS
*
*   \return        None
*
***************************************************************************************************/
void HAL_BRD_toggle_debug_mode_led( void )
{
    HAL_BRD_toggle_pin_state( DEBUG_MODE_LED_PORT, DEBUG_MODE_LED_PIN );
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

low_high_et HAL_BRD_read_S1_pin( void )
{
	return HAL_BRD_read_pin_state( PANEL_2_BTN_PORT, PANEL_2_BTN_PIN );
}

low_high_et HAL_BRD_read_S2_pin( void )
{
	return HAL_BRD_read_pin_state( PANEL_3_BTN_PORT, PANEL_3_BTN_PIN );
}

/*!
****************************************************************************************************
*
*   \brief         Reads the state of the onboard button
*
*   \author        MS
*
*   \return        low_high_et state of the onboard button pin
*
***************************************************************************************************/
low_high_et HAL_BRD_read_onboard_btn( void )
{
	return HAL_BRD_read_pin_state( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN );
}

void HAL_BRD_set_lcd_a0_pin( low_high_et state )
{
	HAL_BRD_set_pin_state( LCD_A0_PORT, LCD_A0_PIN, state );
}

void HAL_BRD_set_lcd_cs_pin( low_high_et state )
{
	HAL_BRD_set_pin_state( LCD_CS_PORT, LCD_CS_PIN, state );
}

void HAL_BRD_set_lcd_rst_pin( low_high_et state )
{
	HAL_BRD_set_pin_state( LCD_RST_PORT, LCD_RST_PIN, state );
}


// void HAL_BRD_WS2811_zero_pulse_direct( void )
// {
// 	/* T0H ~0.4 us: set then hold ~28 cycles */
// 	WS2811_PORT->BSRR = WS2811_PIN;
// 	__asm volatile(
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\n"
// 	);
// 	/* T0L ~0.85 us: clear; remainder consumed by caller loop overhead */
// 	WS2811_PORT->BRR = WS2811_PIN;
// }

// void HAL_BRD_WS2811_one_pulse_direct( void )
// {
// 	/* T1H ~0.8 us: set then hold ~57 cycles */
// 	WS2811_PORT->BSRR = WS2811_PIN;
// 	__asm volatile(
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 		"nop\nnop\nnop\nnop\nnop\nnop\nnop\n"
// 	);
// 	/* T1L ~0.45 us: clear; remainder consumed by caller loop overhead */
// 	WS2811_PORT->BRR = WS2811_PIN;
// }

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
}

/*!
****************************************************************************************************
*
*   \brief         Interrupt Handler ( 2 ) — CPS input pin
*
*   \author        MS
*
*   \return        none
*
*   \note          Calls CPS_tooth_event() directly, by name — no registered callback, no
*                  dispatch table, no trampoline. This pin is dedicated to CPS, so there is
*                  nothing generic to abstract.
*
***************************************************************************************************/
void EXTI2_IRQHandler(void)
{
	if( EXTI_GetFlagStatus( CPS_INPUT_EXTI_LINE ) != RESET )
	{
		EXTI_ClearITPendingBit( CPS_INPUT_EXTI_LINE );
		CPS_tooth_event( &cps_instance_s );
	}
}

/****************************** END OF FILE *******************************************************/

