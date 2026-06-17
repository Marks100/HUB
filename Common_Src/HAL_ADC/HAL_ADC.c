/*! \file
*               Author: mstewart
*   \brief      HAL_ADC module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_ADC.h"

STATIC u16_t HAL_ADC_vref_mv_s;

/*!
****************************************************************************************************
*
*   \brief          Module Initialisation
*
*   \author
*
*   \return         none
*
***************************************************************************************************/
void HAL_ADC_init( void )
{
	ADC_InitTypeDef ADC_InitStructure;

	/* Enable the ADC clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC1, ENABLE );
	RCC_ADCCLKConfig( RCC_PCLK2_Div4 );

	ADC_DeInit( ADC1 );

	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 1u;
	ADC_Init( ADC1, &ADC_InitStructure );

	ADC_Cmd( ADC1, ENABLE );

	/* Enable ADC1 reset calibaration register */
	ADC_ResetCalibration( ADC1 );
	
	/* Check the end of ADC1 reset calibration register */
	while( ADC_GetResetCalibrationStatus( ADC1 ) );
	
	/* Start ADC1 calibaration */
	ADC_StartCalibration( ADC1 );

	/* Check the end of ADC1 calibration */
	while( ADC_GetCalibrationStatus( ADC1 ) != 0u );

	/* Turn on the Internal reference */
	//ADC_TempSensorVrefintCmd(ENABLE);
}

/*!
****************************************************************************************************
*
*   \brief          Module De-Initialisation
*
*   \author
*
*   \return         none
*
***************************************************************************************************/
void HAL_ADC_de_init( void )
{
    /* Reset the ADC module  */
	ADC_DeInit( ADC1 );

	/* Disable ADC system clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_ADC1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief          measure the reference voltage
*
*   \author
*
*   \return         none
*
***************************************************************************************************/
u16_t HAL_ADC_measure_vref_internal( void )
{
    u16_t result = 0u;

    /* Enable Temperature sensor */
    ADC_TempSensorVrefintCmd( ENABLE );

    ADC_RegularChannelConfig( ADC1, ADC_Channel_Vrefint, 1u, ADC_SampleTime_239Cycles5 );

    /* Start ADC1 Software Conversion */
    ADC_SoftwareStartConvCmd( ADC1, ENABLE );

    /* wait for conversion complete */
    while( ADC_GetFlagStatus( ADC1, ADC_FLAG_EOC ) == RESET );

    /* read ADC value */
    result = ADC_GetConversionValue( ADC1 );

    /* clear EOC flag */
    ADC_ClearFlag( ADC1, ADC_FLAG_EOC );

    ADC_TempSensorVrefintCmd( DISABLE );

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief          measure the NTC
*
*   \author
*
*   \return         none
*
***************************************************************************************************/
u16_t HAL_ADC_measure_NTC_temp_raw( void )
{
    u16_t result = 0u;

    ADC_RegularChannelConfig( ADC1, ADC_Channel_3, 1u, ADC_SampleTime_239Cycles5 );

    /* Start ADC1 Software Conversion */
    ADC_SoftwareStartConvCmd( ADC1, ENABLE );

    /* wait for conversion complete */
    while( ADC_GetFlagStatus( ADC1, ADC_FLAG_EOC ) == RESET );

    /* read ADC value */
    result = ADC_GetConversionValue( ADC1 );

    /* clear EOC flag */
    ADC_ClearFlag( ADC1, ADC_FLAG_EOC );

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief          measure the battery voltage
*
*   \author
*
*   \return         none
*
***************************************************************************************************/
u16_t HAL_ADC_measure_batt_voltage( void )
{
	u16_t reference;
    u32_t result;

    reference = HAL_ADC_measure_vref_internal();

    result = (u32_t)( 1200UL * 4096UL ) / (u32_t)reference;

    return ((u16_t)result);
}

STATIC u16_t HAL_ADC_read_channel( u8_t channel )
{
    u16_t result = 0u;

    ADC_RegularChannelConfig( ADC1, channel, 1u, ADC_SampleTime_239Cycles5 );
    ADC_SoftwareStartConvCmd( ADC1, ENABLE );
    while( ADC_GetFlagStatus( ADC1, ADC_FLAG_EOC ) == RESET );
    result = ADC_GetConversionValue( ADC1 );
    ADC_ClearFlag( ADC1, ADC_FLAG_EOC );

    return( result );
}

u16_t HAL_ADC_measure_steering_input( void )
{
    return HAL_ADC_read_channel( ADC_STEERING_CHANNEL );
}

u16_t HAL_ADC_measure_steering_trim( void )
{
    return HAL_ADC_read_channel( ADC_STEERING_TRIM_CHANNEL );
}

u16_t HAL_ADC_measure_throttle_input( void )
{
    return HAL_ADC_read_channel( ADC_THROTTLE_CHANNEL );
}

/****************************** END OF FILE *******************************************************/

