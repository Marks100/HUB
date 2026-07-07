#ifndef HAL_TIM_H
#define HAL_TIM_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_config.h"
#include "DBG_MGR.h"

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
typedef void(*HAL_TIM_func_type)( void );

typedef enum
{
    HAL_TIM_IC_EDGE_RISING = 0u,
    HAL_TIM_IC_EDGE_FALLING,
    HAL_TIM_IC_EDGE_BOTH,
} HAL_TIM_IC_edge_et;

typedef void (*HAL_TIM_IC_callback_ft)( void );

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void HAL_TIM1_init( void );
void HAL_TIM2_init( void );
void HAL_TIM3_init( void );
void HAL_TIM1_start( u16_t counter );
void HAL_TIM2_start( void );
void HAL_TIM3_start( void );
void HAL_TIM1_stop( void );
void HAL_TIM2_stop( void );
void HAL_TIM3_stop( void );
void HAL_TIM1_disable( void );
void HAL_TIM2_disable( void );
void HAL_TIM3_disable( void );
void HAL_TIM1_register_callback( HAL_TIM_func_type HAL_TIM_func_p );
void HAL_TIM2_register_callback( HAL_TIM_func_type HAL_TIM_func_p );
void HAL_TIM3_register_callback( HAL_TIM_func_type HAL_TIM_func_p );

/* Quadrature encoder interface on TIM4 */
void        HAL_TIM4_init_encoder( void );
u16_t       HAL_TIM_ENC_get_counter( void );
low_high_et HAL_TIM_ENC_get_dir( void );

/* Generic input capture — any free channel on TIM2 or TIM3 (TIM3 CH4 is reserved for the
 * buzzer PWM output, TIM4 is reserved for the rotary encoder — requests for those are ignored).
 * `channel` is 1-4. Callback fires from ISR context on each qualifying edge.
 * Starts the timer counter running. Do not combine TIM2 input capture with the TIM2
 * one-shot delay API (HAL_TIM2_start()/stop()) — they contend for the same counter. */
void HAL_TIM_IC_init( TIM_TypeDef* tim_p, u8_t channel, HAL_TIM_IC_edge_et edge, HAL_TIM_IC_callback_ft callback_p );

#endif

/****************************** END OF FILE *******************************************************/
