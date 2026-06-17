#ifndef HAL_ADC_H
#define HAL_ADC_H

#include "STDC.h"
#include "HAL_config.h"
/***************************************************************************************************
**                              Defines                                                          **
***************************************************************************************************/
#define VCC_REFERENCE_VOLTAGE 3300.0f
#define ADC_MAX_COUNT		  4096.0f
#define ADC_BANDGAP_VOLTAGE   1400.0f

/***************************************************************************************************
**                              enumerated Types                                                  **
***************************************************************************************************/
typedef enum
{
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
} ADC_channel_et;

/***************************************************************************************************
**                                 Functions                                                      **
***************************************************************************************************/
void HAL_ADC_init( void ) ;
void HAL_ADC_de_init( void );
u16_t HAL_ADC_measure_batt_voltage( void );
u16_t HAL_ADC_measure_NTC_temp_raw( void );
u16_t HAL_ADC_measure_STM32_temp_raw( void );
u16_t HAL_ADC_measure_vref_internal( void );
u16_t HAL_ADC_measure_steering_input( void );
u16_t HAL_ADC_measure_steering_trim( void );
u16_t HAL_ADC_measure_throttle_input( void );

#endif

/****************************** END OF FILE *******************************************************/
