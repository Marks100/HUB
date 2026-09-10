/*! \file
*               Author: mstewart
*   \brief      Mode MGR module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "MODE_MGR.h"
#include "INTEGRATION_STUBS.h"
#include "MSG_SCHED.h"
#include "HMI_SH1106.h"   /* This module owns the panel's tick slot - see mode_mgr_action_schedule_normal() */
#include "CANTP.h"
#include "UDS.h"
#include "CPS.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC MODE_MGR_timer_st mode_mgr_timer_s;
STATIC MODE_MGR_mode_et  mode_mgr_mode_s;

extern NRF24_instance_st     nrf24_instance_s;
extern BUZZER_instance_st    buzzer_instance_s;
extern CPS_instance_st       cps_crank_instance_s;
extern WS2811_instance_st    ws2811_instance_s;
extern CTRL_AXIS_instance_st steering_axis_s;
extern CTRL_AXIS_instance_st throttle_axis_s;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Init routine
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_init( void )
{
	mode_mgr_mode_s = MODE_MGR_MODE_NORMAL;

	STDC_memset( &mode_mgr_timer_s, 0x00, sizeof( mode_mgr_timer_s ) );
	WS2811_set_all_led_color( &ws2811_instance_s, WS2811_WHITE );
}

/*!
****************************************************************************************************
*
*   \brief         Cyclic tick
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_tick( void )
{
	//DBG_MGR_start_cpu_load_timer();
    TIME_increment_time();

	/* CAN-TP/UDS timing (ST_min, N_Cr/N_Bs/N_Ar, S3 session timeout) is budgeted in real
	   milliseconds, so these need the base 1ms-class tick, not one of
	   mode_mgr_action_schedule_normal()'s slower interval slots - same reasoning FBL_tick() ticks
	   CANTP every cycle instead of on a slower schedule. Note this runs at APP_TIMER_TICK_RATE_MS
	   (10ms), coarser than FBL's genuine 1ms SysTick loop - fine for CANTP.h's default timing
	   budgets (N_Ar=25ms etc., still several ticks of headroom), just worth knowing if a tester
	   ever needs tighter timing than that. */
	CANTP_tick( &app_cantp_instance_s );
	UDS_tick();

	WDG_kick();

	switch( mode_mgr_mode_s )
	{
		case MODE_MGR_MODE_STARTUP:
		{
			mode_mgr_change_mode( MODE_MGR_MODE_NORMAL );
		}
		break;

		case MODE_MGR_MODE_NORMAL:
		{
			mode_mgr_action_schedule_normal();
		}	
		break;

		case MODE_MGR_SETTINGS_MODE:
		{
			mode_mgr_action_schedule_normal();
		}	
		break;

        case MODE_MGR_EXPERT_MODE:
		{
			mode_mgr_action_schedule_normal();
		}	
		break;

		default:
		break;
	}

	//DBG_MGR_stop_cpu_load_timer();
}

/*!
****************************************************************************************************
*
*   \brief         power button Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_power_cbk( void )
{
}

/*!
****************************************************************************************************
*
*   \brief         enter button Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_enter_pressed_cbk( void )
{
	BUZZER_short_beep( &buzzer_instance_s );
}

/*!
****************************************************************************************************
*
*   \brief         LCD Reset button Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_reset_pressed_cbk( void )
{
	BUZZER_long_beep( &buzzer_instance_s );

	static u8_t color = 0;

	switch( color )
	{
		case 0: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_RED );    break;
		case 1: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_GREEN );  break;
		case 2: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_BLUE );   break;
		case 3: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_YELLOW ); break;
		case 4: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_ORANGE ); break;
		case 5: WS2811_set_all_led_color( &ws2811_instance_s, WS2811_WHITE );  break;
		default: break;
	}
	
	if( color >= 5 )
	{
		color = 0u;
	}
	else
	{
		color++;
	}
	
}

/*!
****************************************************************************************************
*
*   \brief         long press enter Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_enter_long_pressed_cbk( void )
{
}

/*!
****************************************************************************************************
*
*   \brief         
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_expert_mode_entered_cbk( void )
{
    switch( mode_mgr_mode_s )
    {
        case MODE_MGR_MODE_NORMAL:
        {
            mode_mgr_change_mode( MODE_MGR_EXPERT_MODE );
        }
        break;

        case MODE_MGR_EXPERT_MODE:
        {
            mode_mgr_change_mode( MODE_MGR_MODE_NORMAL );
        }
        break;

        default:
        break;
    }
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Check if thetime interval is correct
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void mode_mgr_change_mode( MODE_MGR_mode_et mode )
{
    mode_mgr_mode_s = mode;
}

/*!
****************************************************************************************************
*
*   \brief         Check if thetime interval is correct
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
false_true_et mode_mgr_check_time_interval( u16_t interval )
{
	false_true_et status = FALSE;

	if( ( mode_mgr_timer_s.msecs % interval ) == 0u )
	{
		status = TRUE;
	}

	return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Change Mode
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_change_mode( MODE_MGR_mode_et mode )
{
	(void)mode;
}

/*!
****************************************************************************************************
*
*   \brief         Get the Mode
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
MODE_MGR_mode_et MODE_MGR_get_mode( void )
{
    return( mode_mgr_mode_s );
}

/*!
****************************************************************************************************
*
*   \brief         Handle normal mode schedulling
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void mode_mgr_action_schedule_normal( void )
{
	u16_t wheel_rpms[2];

	if( mode_mgr_check_time_interval( 10u ) == TRUE )
	{
		ESP01_tick();
        //WIFI_tick();
        TB_tick();

    	/* Drives the panel's encoder, buttons and OLED refresh in one call. Its tick_rate_ms is
    	   configured as 10 to match this slot - change one and you must change the other. */
    	HMI_SH1106_tick();
	}

	if( mode_mgr_check_time_interval( 20u ) == TRUE )
	{
		BUZZER_tick( &buzzer_instance_s );
		BTN_MGR_tick( &btm_mgr_instance_s );
		NRF24_tick( &nrf24_instance_s );
		RF_MGR_tick();
		MSG_SCHED_tick();
		CPS_tick( &cps_crank_instance_s );
	}

	if( mode_mgr_check_time_interval( 50u ) == TRUE )
	{
		/* Commits any block whose RAM mirror has been marked dirty. A plain append is a short
		   flash write; only a partition switch erases a page, and that costs ~20-40 ms in this
		   slot - rare enough to live here rather than needing its own deferred context. */
		NVM_GEN2_tick();
	}

	if( mode_mgr_check_time_interval( 100u ) == TRUE )
	{
		//WS2811_tick( &ws2811_instance_s );
	}

	if( mode_mgr_check_time_interval( 200u ) == TRUE )
	{
    }

	if( mode_mgr_timer_s.msecs >= MODE_MGR_MAX_TICK_CYCLE_VALUE )
	{
		mode_mgr_timer_s.msecs = MODE_MGR_TICK_RATE_MSECS;
	}
	else
	{
		mode_mgr_timer_s.msecs += MODE_MGR_TICK_RATE_MSECS;
	}
}

/****************************** END OF FILE *******************************************************/




