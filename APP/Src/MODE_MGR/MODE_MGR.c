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

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC MODE_MGR_timer_st mode_mgr_timer_s;
STATIC MODE_MGR_mode_et  mode_mgr_mode_s;

extern NRF24_instance_st     nrf24_instance_s;
extern BUZZER_instance_st    buzzer_instance_s;
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
*   \brief         Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_ccw_scroll_cbk( void )
{
	BUZZER_short_beep( &buzzer_instance_s );
	ST7567_cursor_up();
}

/*!
****************************************************************************************************
*
*   \brief         Callback
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void MODE_MGR_cw_scroll_cbk( void )
{
	BUZZER_short_beep( &buzzer_instance_s );
	ST7567_cursor_down();
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
		//esp01_check_rx_timeout();
		//ESP01_tick();
        //WIFI_tick();
        //TB_tick();

		//ST7567_tick();
		//NRF24_tick( &nrf24_instance_s );
    	ROTARY_MGR_tick();
		CPS_tick( &cps_instance_s );
		CPS_tick( &cps_instance_2_s );

		wheel_rpms[0] = (u16_t)CPS_get_rpm( &cps_instance_s );
		wheel_rpms[1] = (u16_t)CPS_get_rpm( &cps_instance_2_s );

		/* Same-axle pair, same tyre size — raw RPM ratio works directly, no TYRE_CALC/
		 * SPEED_CONV conversion needed (see slip_detect_cfg_s comment). */
		SLIP_DETECT_update( &slip_detect_instance_s, wheel_rpms[0], wheel_rpms[1] );

		/* Fuse both wheels into one reference RPM (excludes whichever is off the group
		 * median beyond ref_speed_calc_cfg_s.reject_ratio_pct), then convert to real
		 * road speed via TYRE_CALC's circumference — see comment above ref_speed_calc_cfg_s. */
		vehicle_reference_rpm_s = REF_SPEED_CALC_get_reference_rpm( &ref_speed_calc_instance_s, wheel_rpms, 2u );
		vehicle_speed_kph_s     = SPEED_CONV_rpm_to_kph( (u16_t)vehicle_reference_rpm_s, vehicle_tyre_circumference_mm_s );
	}

	if( mode_mgr_check_time_interval( 20u ) == TRUE )
	{
		BUZZER_tick( &buzzer_instance_s );
		BTN_MGR_tick();
		NRF24_tick( &nrf24_instance_s );
		RF_MGR_tick();
		MSG_SCHED_tick();
	}

	if( mode_mgr_check_time_interval( 50u ) == TRUE )
	{
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




