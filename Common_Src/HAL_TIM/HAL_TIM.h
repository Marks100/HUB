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

#endif

/****************************** END OF FILE *******************************************************/
