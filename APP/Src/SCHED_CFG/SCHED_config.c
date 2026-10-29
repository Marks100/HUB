/*! \file
*               Author: mstewart
*   \brief      Application schedule table - which function runs at what rate
*
*   \details    The whole application schedule in one place. Add a line to run something
*               periodically; there is nothing else to touch. SCHED_tick() is the SysTick callback,
*               so this table is everything that runs from the tick. Entries due on the same tick run
*               in table order, so the order below is a dependency order where it matters.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "SCHED_config.h"
#include "DWT.h"
#include "TIME.h"
#include "WDG.h"
#include "CANIF.h"
#include "UDS.h"
#include "UDS_CLIENT.h"
#include "INTEGRATION_STUBS.h"
#include "BTN_MGR.h"
#include "BUZZER.h"
#include "RF_MGR.h"
#include "SENSOR_NET.h"
#include "MSG_SCHED.h"
#include "HMI_SH1106.h"
#include "CPS.h"
#include "VEHICLE_SIM_config.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void sched_cfg_mcp2515_tick( void );
STATIC void sched_cfg_cantp_tick( void );
STATIC void sched_cfg_buzzer_tick( void );
STATIC void sched_cfg_btn_mgr_tick( void );
STATIC void sched_cfg_cps_tick( void );
STATIC void sched_cfg_aps_tick( void );
STATIC void sched_cfg_engine_sim_tick( void );

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
extern BUZZER_instance_st buzzer_instance_s;
extern CPS_instance_st    cps_crank_instance_s;

/*  Callout                          Period                 Offset
    -------------------------------  ---------------------  ------ */
STATIC const SCHED_entry_st app_sched_table_s[] =
{
	{ TIME_increment_time,            SCHED_PERIOD_10MS,     0u },
	{ sched_cfg_mcp2515_tick,         SCHED_PERIOD_5MS,      0u },
	{ DIAG_GW_tick,                   SCHED_PERIOD_5MS,      0u },
	{ UDS_CLIENT_tick,                SCHED_PERIOD_10MS,     0u },
	{ CANIF_tick,                     SCHED_PERIOD_5MS,      0u },
	{ sched_cfg_cantp_tick,           SCHED_PERIOD_5MS,      0u }, 
	{ UDS_tick,                       SCHED_PERIOD_10MS,     0u },
	{ ESP01_tick,                     SCHED_PERIOD_10MS,     0u },
	{ WIFI_tick,                      SCHED_PERIOD_10MS,     0u },   /* after ESP01_tick, before TB_tick */
	{ TB_tick,                        SCHED_PERIOD_10MS,     0u },
	{ HMI_SH1106_tick,                SCHED_PERIOD_10MS,     SCHED_PERIOD_5MS },   /* blocking OLED writes (~6 ms) kept off the gateway's passes */
	{ sched_cfg_buzzer_tick,          SCHED_PERIOD_20MS,     0u },
	{ sched_cfg_btn_mgr_tick,         SCHED_PERIOD_20MS,     0u },
	{ RF_MGR_tick,                    SCHED_PERIOD_10MS,     0u },
	{ SENSOR_NET_tick,                SCHED_PERIOD_10MS,     0u },   /* after RF_MGR_tick */
	{ MSG_SCHED_tick,                 SCHED_PERIOD_20MS,     0u },
	{ sched_cfg_cps_tick,             SCHED_PERIOD_20MS,     0u },
	{ sched_cfg_aps_tick,             SCHED_PERIOD_20MS,     0u },
	{ GKT_SHIFTER_tick,               SCHED_PERIOD_20MS,     0u },
	{ TCU_tick,                       SCHED_PERIOD_20MS,     0u },
	{ app_tcu_gear_state_local_relay, SCHED_PERIOD_20MS,     0u },
	{ sched_cfg_engine_sim_tick,      SCHED_PERIOD_20MS,     0u },
	{ app_engine_sim_cluster_relay,   SCHED_PERIOD_20MS,     0u },
	{ MQB_CLUSTER_tick,               SCHED_PERIOD_20MS,     0u },
	{ MRA_CLUSTER_tick,               SCHED_PERIOD_20MS,     0u },
	{ NVM_GEN2_tick,                  SCHED_PERIOD_50MS,     0u },

	/* Kicked last, so a kick means a whole pass completed - a callout that hangs part-way
	   through stops the kicks */
	{ WDG_kick,                       SCHED_PERIOD_10MS,     0u }
};

const SCHED_cfg_st app_sched_cfg_s =
{
	.table_p     = app_sched_table_s,
	.num_entries = (u8_t)( sizeof( app_sched_table_s ) / sizeof( app_sched_table_s[0u] ) ),
	.tick_ms     = SCHED_TICK_RATE_MSECS,

	.get_count_func_p   = DWT_get_count,
	.count_to_us_func_p = DWT_cycles_to_us,
	.overrun_func_p     = NULL_P
};

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Schedule wrappers for ticks that take an instance
*
*   \author        MS
*
*   \return        none
*
*   \note          SCHED callouts are void( void ), so these bind each instance-taking tick to the
*                  instance it runs on. Kept as real functions rather than casting a function
*                  pointer, which keeps the table type-safe.
*
***************************************************************************************************/
STATIC void sched_cfg_mcp2515_tick( void )
{
	MCP2515_tick( &mcp2515_can2_instance_s );
}

STATIC void sched_cfg_cantp_tick( void )
{
	CANTP_tick( &app_cantp_instance_s );
	CANTP_tick( &app_cantp2_instance_s );
}

STATIC void sched_cfg_buzzer_tick( void )
{
	BUZZER_tick( &buzzer_instance_s );
}

STATIC void sched_cfg_btn_mgr_tick( void )
{
	BTN_MGR_tick( &btm_mgr_instance_s );
}

STATIC void sched_cfg_cps_tick( void )
{
	CPS_tick( &cps_crank_instance_s );
}

STATIC void sched_cfg_aps_tick( void )
{
	APS_tick( &aps_pedal_instance_s );
}

STATIC void sched_cfg_engine_sim_tick( void )
{
	ENGINE_SIM_tick( &engine_sim_instance_s );
}

/****************************** END OF FILE *******************************************************/
