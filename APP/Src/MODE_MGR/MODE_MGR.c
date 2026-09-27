/*! \file
*               Author: mstewart
*   \brief      Mode MGR module - the application's periodic tick and slot scheduler
*
*   \details    MODE_MGR_tick() runs from the SysTick interrupt every MODE_MGR_TICK_RATE_MSECS.
*               Timing-critical services (TIME, CANTP, UDS) run every tick; everything else runs
*               in fixed-interval slots from mode_mgr_action_schedule_normal(). A slot is due when
*               the cycle counter is a multiple of its interval, so every slot interval must be a
*               multiple of the tick rate and divide MODE_MGR_CYCLE_MS exactly - both are checked
*               at compile time below.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "MODE_MGR.h"
#include "INTEGRATION_STUBS.h"
#include "TIME.h"
#include "WDG.h"
#include "BTN_MGR.h"
#include "BUZZER.h"
#include "WS2811.h"
#include "NRF24.h"
#include "RF_MGR.h"
#include "MSG_SCHED.h"
#include "HMI_SH1106.h"   /* This module owns the panel's tick slot - see mode_mgr_action_schedule_normal() */
#include "CANIF.h"
#include "UDS.h"
#include "CPS.h"
#include "VEHICLE_SIM_config.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Length of one full schedule cycle - the cycle counter wraps at this, so it must be a common
   multiple of every slot interval */
#define MODE_MGR_CYCLE_MS           ( 1000u )

/* Slot intervals */
#define MODE_MGR_SLOT_10MS          ( 10u )
#define MODE_MGR_SLOT_20MS          ( 20u )
#define MODE_MGR_SLOT_50MS          ( 50u )
#define MODE_MGR_SLOT_100MS         ( 100u )

/* Compile-time check that a slot interval can actually be honoured - a multiple of the tick rate
   (otherwise it is never, or only sometimes, due) that divides the cycle exactly (otherwise its
   period jitters every time the cycle counter wraps). Fails the build with a negative array size. */
#define MODE_MGR_CHECK_SLOT( name, ms )                                                       \
	typedef char mode_mgr_slot_check_##name[ ( ( ( (ms) % MODE_MGR_TICK_RATE_MSECS ) == 0u ) &&  \
	                                           ( ( MODE_MGR_CYCLE_MS % (ms) ) == 0u ) ) ? 1 : -1 ]

MODE_MGR_CHECK_SLOT( 10ms,  MODE_MGR_SLOT_10MS );
MODE_MGR_CHECK_SLOT( 20ms,  MODE_MGR_SLOT_20MS );
MODE_MGR_CHECK_SLOT( 50ms,  MODE_MGR_SLOT_50MS );
MODE_MGR_CHECK_SLOT( 100ms, MODE_MGR_SLOT_100MS );

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC false_true_et mode_mgr_slot_due( u16_t interval_ms );
STATIC void          mode_mgr_action_schedule_normal( void );

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC u16_t            mode_mgr_cycle_ms_s;   /*!< Position in the schedule cycle, 0 .. MODE_MGR_CYCLE_MS - tick */
STATIC MODE_MGR_mode_et mode_mgr_mode_s;

extern NRF24_instance_st  nrf24_instance_s;
extern BUZZER_instance_st buzzer_instance_s;
extern CPS_instance_st    cps_crank_instance_s;
extern WS2811_instance_st ws2811_instance_s;

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
*   \note          Starts in MODE_MGR_MODE_STARTUP - the first tick moves to NORMAL
*
***************************************************************************************************/
void MODE_MGR_init( void )
{
	mode_mgr_mode_s     = MODE_MGR_MODE_STARTUP;
	mode_mgr_cycle_ms_s = 0u;

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
*   \note          Runs from the SysTick interrupt (lowest priority) every MODE_MGR_TICK_RATE_MSECS
*   \note          The watchdog is kicked last, so a kick means a whole pass completed - a module
*                  that hangs part-way through stops the kicks
*
***************************************************************************************************/
void MODE_MGR_tick( void )
{
	//DBG_MGR_start_cpu_load_timer();
	TIME_increment_time();

	/* CAN-TP/UDS timing (ST_min, N_Cr/N_Bs, S3 session timeout) is budgeted in real
	   milliseconds, so these need the base 1ms-class tick, not one of
	   mode_mgr_action_schedule_normal()'s slower interval slots - same reasoning FBL_tick() ticks
	   CANTP every cycle instead of on a slower schedule. Note this runs at APP_TIMER_TICK_RATE_MS
	   (10ms), coarser than FBL's genuine 1ms SysTick loop - fine for CANTP.h's default timing
	   budgets (N_Bs=250ms, N_Cr=500ms), but it caps multi-frame TX at one Consecutive Frame per
	   10ms, and is worth knowing if a tester ever needs tighter timing than that.
	   CANIF_tick() ticks every CAN channel's CANTP instance, so a new bus needs no change here. */
	CANIF_tick();
	UDS_tick();

	switch( mode_mgr_mode_s )
	{
		case MODE_MGR_MODE_STARTUP:
		{
			mode_mgr_mode_s = MODE_MGR_MODE_NORMAL;
		}
		break;

		/* Every running mode shares the one schedule for now - give a mode its own
		   mode_mgr_action_schedule_*() here when it needs to behave differently */
		case MODE_MGR_MODE_NORMAL:
		case MODE_MGR_MODE_SETTINGS:
		case MODE_MGR_MODE_EXPERT:
		{
			mode_mgr_action_schedule_normal();
		}
		break;

		default:
		break;
	}

	(void)WDG_kick();

	//DBG_MGR_stop_cpu_load_timer();
}

/*!
****************************************************************************************************
*
*   \brief         Change mode
*
*   \author        MS
*
*   \param[in]     mode - Mode to switch to, takes effect from the next tick
*
*   \return        none
*
***************************************************************************************************/
void MODE_MGR_change_mode( MODE_MGR_mode_et mode )
{
	mode_mgr_mode_s = mode;
}

/*!
****************************************************************************************************
*
*   \brief         Get the current mode
*
*   \author        MS
*
*   \return        Current mode
*
***************************************************************************************************/
MODE_MGR_mode_et MODE_MGR_get_mode( void )
{
	return( mode_mgr_mode_s );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Is a slot of the given interval due on this tick
*
*   \author        MS
*
*   \param[in]     interval_ms - Slot interval, one of the MODE_MGR_SLOT_* values
*
*   \return        TRUE if due
*
***************************************************************************************************/
STATIC false_true_et mode_mgr_slot_due( u16_t interval_ms )
{
	false_true_et status = FALSE;

	if( ( mode_mgr_cycle_ms_s % interval_ms ) == 0u )
	{
		status = TRUE;
	}

	return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Run whichever slots are due on this tick, then advance the cycle
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void mode_mgr_action_schedule_normal( void )
{
	if( mode_mgr_slot_due( MODE_MGR_SLOT_10MS ) == TRUE )
	{
		ESP01_tick();
		//WIFI_tick();
		TB_tick();

		/* Drives the panel's encoder, buttons and OLED refresh in one call. Its tick_rate_ms is
		   configured as 10 to match this slot - change one and you must change the other. */
		HMI_SH1106_tick();
	}

	if( mode_mgr_slot_due( MODE_MGR_SLOT_20MS ) == TRUE )
	{
		BUZZER_tick( &buzzer_instance_s );
		BTN_MGR_tick( &btm_mgr_instance_s );
		NRF24_tick( &nrf24_instance_s );
		RF_MGR_tick();
		MSG_SCHED_tick();
		CPS_tick( &cps_crank_instance_s );
		APS_tick( &aps_pedal_instance_s );
		/* GKT_SHIFTER goes first, deliberately. HAL_CAN has no TX queue - each PDUR_tx() needs one
		   of bxCAN's 3 hardware mailboxes and FAILs if they're all busy - and TCU sends 3 frames
		   back to back every cycle. Ticked after TCU, the shifter's LED (0x3FD) and backlight
		   (0x202) frames found every mailbox taken and were starved. The shifter needs its
		   frames on time; TCU retries a rejected frame next cycle, so it can afford to be the one
		   that waits (its 2nd/3rd frame slips one cycle whenever the shifter is due, every 100 ms). */
		GKT_SHIFTER_tick();
		/* TCU decides the gear (using last cycle's engine RPM feedback) before ENGINE_SIM computes
		   this cycle's RPM from it - see tcu_cfg_s/engine_sim_cfg_s's comments, INTEGRATION_STUBS.c. */
		TCU_tick();
		app_tcu_gear_state_local_relay();
		ENGINE_SIM_tick( &engine_sim_instance_s );
		app_engine_sim_cluster_relay();
		MQB_CLUSTER_tick();
		MRA_CLUSTER_tick();
	}

	if( mode_mgr_slot_due( MODE_MGR_SLOT_50MS ) == TRUE )
	{
		/* Commits any block whose RAM mirror has been marked dirty. A plain append is a short
		   flash write; only a partition switch erases a page, and that costs ~20-40 ms in this
		   slot - rare enough to live here rather than needing its own deferred context. */
		NVM_GEN2_tick();
	}

	if( mode_mgr_slot_due( MODE_MGR_SLOT_100MS ) == TRUE )
	{
		//WS2811_tick( &ws2811_instance_s );
	}

	mode_mgr_cycle_ms_s = (u16_t)( ( mode_mgr_cycle_ms_s + MODE_MGR_TICK_RATE_MSECS ) % MODE_MGR_CYCLE_MS );
}

/****************************** END OF FILE *******************************************************/
