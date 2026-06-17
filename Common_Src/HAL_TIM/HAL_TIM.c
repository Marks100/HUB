/*! \file
*               Author: mstewart
*   \brief      HAL_TIM module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_TIM.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC HAL_TIM_func_type HAL_TIM1_func_p = NULL_P;
STATIC HAL_TIM_func_type HAL_TIM2_func_p = NULL_P;
STATIC HAL_TIM_func_type HAL_TIM3_func_p = NULL_P;
STATIC HAL_TIM_func_type HAL_TIM4_func_p = NULL_P;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Timer 1 Init
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM1_init( void )
{
	/* Enable the timer peripheral clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_TIM1, ENABLE );

    /* TIM1 setup as a timer */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /* set everything back to default values */
    TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

    TIM_TimeBaseStructure.TIM_Prescaler         = 8000u;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period            = 65535;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseStructure);

	TIM_ClearITPendingBit( TIM1, TIM_IT_Update );
    TIM_ITConfig( TIM1, TIM_IT_Update, ENABLE );

	/* Timer and NVIC both kept disabled until HAL_TIM1_start() is called */
	TIM_Cmd( TIM1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 2 Init
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM2_init( void )
{
	/* Enable the TIM2 clock */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2, ENABLE );

	/* TIM2 */
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

	/* set everything back to default values */
	TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

	TIM_TimeBaseStructure.TIM_Prescaler = 1000u;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 19u;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV4;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0u;
	TIM_TimeBaseInit( TIM2, &TIM_TimeBaseStructure );
	TIM_ITConfig( TIM2, TIM_IT_Update, ENABLE );
	
	/* Keep disabled for now until needed */
	TIM_Cmd( TIM2, DISABLE );

	NVIC_InitTypeDef NVIC_InitStruct;

	/* Add IRQ vector to NVIC */
	NVIC_InitStruct.NVIC_IRQChannel = TIM2_IRQn;
	/* Set priority */
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x00;
	/* Set sub priority */
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	/* Add to NVIC */
	NVIC_Init( &NVIC_InitStruct );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 Init
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM3_init( void )
{
	/* Enable the TIM3 clock */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM3, ENABLE );

	/* TIM3 */
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

	/* set everything back to default values */
	TIM_TimeBaseStructInit( &TIM_TimeBaseStructure );

	TIM_TimeBaseStructure.TIM_Prescaler = 2376u;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 100-1u;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit( TIM3, &TIM_TimeBaseStructure );
	/* TIM_ITConfig( TIM3, TIM_IT_Update, ENABLE ); */
	
	TIM_Cmd( TIM3, ENABLE );

	TIM_OCInitTypeDef TIM_OCInitStructure;

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 1u;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;

	TIM_OC4Init( TIM3, &TIM_OCInitStructure );
	TIM_OC4PreloadConfig( TIM3, TIM_OCPreload_Enable );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 1 Register callback
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM1_register_callback( HAL_TIM_func_type HAL_TIM_func_p )
{
	if( HAL_TIM_func_p != NULL_P )
	{
		HAL_TIM1_func_p = HAL_TIM_func_p;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 2 Register callback
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM2_register_callback( HAL_TIM_func_type HAL_TIM_func_p )
{
	if( HAL_TIM_func_p != NULL_P )
	{
		HAL_TIM2_func_p = HAL_TIM_func_p;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 Register callback
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM3_register_callback( HAL_TIM_func_type HAL_TIM_func_p )
{
	if( HAL_TIM_func_p != NULL_P )
	{
		HAL_TIM3_func_p = HAL_TIM_func_p;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 Register callback
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM4_register_callback( HAL_TIM_func_type HAL_TIM_func_p )
{
	if( HAL_TIM_func_p != NULL_P )
	{
		HAL_TIM4_func_p = HAL_TIM_func_p;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 1 Start
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM1_start( u16_t counter )
{
	counter == 0u ? counter = 1u : counter --;

	TIM_ClearITPendingBit( TIM1, TIM_IT_Update );
	TIM_SetCounter( TIM1, 0u );
	TIM_SetAutoreload( TIM1, counter );

	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel                   = TIM1_UP_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelCmd                = ENABLE;
	NVIC_Init( &NVIC_InitStruct );

	TIM_Cmd( TIM1, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 1 Stop
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM1_stop( void )
{
	TIM_Cmd( TIM1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 1 Disable
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM1_disable( void )
{
	HAL_TIM1_stop();

	NVIC_InitTypeDef NVIC_InitStruct;
	NVIC_InitStruct.NVIC_IRQChannel                   = TIM1_UP_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority        = 0x00;
	NVIC_InitStruct.NVIC_IRQChannelCmd                = DISABLE;
	NVIC_Init( &NVIC_InitStruct );

	/* Disable the timer peripheral clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_TIM1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 2 Start
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM2_start( void )
{
	TIM_ClearITPendingBit( TIM2, TIM_IT_Update );
	TIM_SetCounter( TIM2, 0u );
	TIM_Cmd( TIM2, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 2 Stop
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM2_stop( void )
{
	TIM_Cmd( TIM2, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 Start
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM3_start( void )
{
	TIM_ClearITPendingBit( TIM3, TIM_IT_Update );
	TIM_SetCounter( TIM3, 0u );
	TIM_Cmd( TIM3, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 Stop
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM3_stop( void )
{
	TIM_Cmd( TIM3, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 4 Start
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM4_start( void )
{
	TIM_ClearITPendingBit( TIM4, TIM_IT_Update );
	TIM_SetCounter( TIM4, 0u );

	TIM_ITConfig( TIM4, TIM_IT_Update, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 4 Stop
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM4_stop( void )
{
	TIM_Cmd( TIM4, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Timer 1 IRQ
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void TIM1_UP_IRQHandler( void )
{
    if( TIM_GetITStatus( TIM1, TIM_IT_Update ) )
	{
    	TIM_ClearITPendingBit( TIM1, TIM_IT_Update );
		HAL_TIM1_disable();

		if( HAL_TIM1_func_p != NULL_P )
		{
			HAL_TIM1_func_p();
		}
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 2 IRQ
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void TIM2_IRQHandler ( void )
{
	if( TIM_GetITStatus( TIM2, TIM_IT_Update ) )
	{
		HAL_TIM2_stop();
		TIM_ClearITPendingBit( TIM2, TIM_IT_Update );

		if( HAL_TIM2_func_p != NULL_P )
		{
			HAL_TIM2_func_p();
		}
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 3 IRQ
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void TIM3_IRQHandler ( void )
{
	if( TIM_GetITStatus( TIM3, TIM_IT_Update ) )
	{
		TIM_ClearITPendingBit( TIM3, TIM_IT_Update );
	}
}

/*!
****************************************************************************************************
*
*   \brief         Timer 4 IRQ
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void TIM4_IRQHandler ( void )
{
	if( TIM_GetITStatus( TIM4, TIM_IT_Update ) )
	{
		TIM_ClearITPendingBit( TIM4, TIM_IT_Update );
	}
}

/*!
****************************************************************************************************
*
*   \brief         Initialise TIM4 in quadrature encoder interface mode
*                  CH1=PB6, CH2=PB7 — configure these pins as floating inputs before calling
*
*   \return        none
*
***************************************************************************************************/
void HAL_TIM4_init_encoder( void )
{
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM4, ENABLE );

	TIM_TimeBaseInitTypeDef tb;
	TIM_TimeBaseStructInit( &tb );
	tb.TIM_Period      = 0xFFFF;
	tb.TIM_Prescaler   = 0u;
	tb.TIM_ClockDivision    = TIM_CKD_DIV1;
	tb.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit( TIM4, &tb );

	TIM_EncoderInterfaceConfig( TIM4, TIM_EncoderMode_TI12,
	                            TIM_ICPolarity_Rising,
	                            TIM_ICPolarity_Rising );

	TIM_SetCounter( TIM4, 0x8000u );
	TIM_Cmd( TIM4, ENABLE );
}

u16_t HAL_TIM_ENC_get_counter( void )
{
	return (u16_t)TIM_GetCounter( TIM4 );
}

/* Returns HIGH if TIM4 is counting down (CW), LOW if counting up (CCW) */
low_high_et HAL_TIM_ENC_get_dir( void )
{
	return ( TIM4->CR1 & TIM_CR1_DIR ) ? HIGH : LOW;
}

/****************************** END OF FILE *******************************************************/