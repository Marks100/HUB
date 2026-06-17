#ifndef MODE_MGR_H
#define MODE_MGR_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "SYSTICK.h"
#include "NVM.h"
#include "WDG.h"
#include "HAL_SPI.h"
#include "HAL_ADC.h"
#include "HAL_UART.h"
#include "HAL_TIM.h"
#include "BTN_MGR.h"
#include "DBG_MGR.h"
#include "BUZZER.h"
#include "CTRL_AXIS.h"
//#include "LCD.h"
//#include "SCREEN_MGR.h"
#include "ROTARY_MGR.h"
#include "TIME.h"
#include "WDG.h"
#include "RF_MGR.h"
#include "WS2811.h"
#include "ST7567.h"
#include "NRF24.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define MODE_MGR_MAX_TICK_CYCLE_VALUE 	( 1000u )
#define MODE_MGR_TICK_RATE_MSECS		( APP_TIMER_TICK_RATE_MS  )
#define MODE_MGR_EXPERT_MODE_TIMEOUT    ( 100u * ( 1000u / MODE_MGR_TICK_RATE_MSECS ) )
#define MODE_MGR_SNA                    ( 127u )
#define MODE_MGR_NUM_TEMP_SAMPLES       ( 10u )

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef enum
{
	MODE_MGR_MODE_STARTUP = 0u,
    MODE_MGR_MODE_NORMAL,
    MODE_MGR_SETTINGS_MODE,
    MODE_MGR_EXPERT_MODE
} MODE_MGR_mode_et;

typedef struct 
{
    u32_t msecs;
} MODE_MGR_timer_st;

typedef struct
{
    false_true_et active;
    u16_t         active_timeout;
    u8_t          triggers;
    u16_t         trigger_timeout;
} MODE_MGR_expert_mode_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void  MODE_MGR_init( void );
void  MODE_MGR_tick( void );
void  MODE_MGR_handle_settings( void );
void  MODE_MGR_power_cbk( void );
void  MODE_MGR_ccw_scroll_cbk( void );
void  MODE_MGR_cw_scroll_cbk( void );
void  MODE_MGR_enter_pressed_cbk( void );
void  MODE_MGR_reset_pressed_cbk( void );
void  MODE_MGR_enter_long_pressed_cbk( void );
void  MODE_MGR_change_mode( MODE_MGR_mode_et mode );
void  MODE_MGR_expert_mode_entered_cbk( void );
s8_t  MODE_MGR_get_temperature( void );
MODE_MGR_mode_et MODE_MGR_get_mode( void );

false_true_et mode_mgr_check_time_interval( u16_t interval );
void          mode_mgr_change_mode( MODE_MGR_mode_et mode );
void          mode_mgr_action_schedule_normal( void );

#endif /* MODE_MGR_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
