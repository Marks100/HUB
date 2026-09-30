#ifndef SCHED_CONFIG_H
#define SCHED_CONFIG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "SCHED.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* The base tick SysTick runs at - the one place it comes from is APP_TIMER_TICK_RATE_MS in
   PROJ_config.h. Nothing else in the application states it. */
#define SCHED_TICK_RATE_MSECS   ( APP_TIMER_TICK_RATE_MS )

/* Every rate anything runs at, in ms. A module whose own config needs to know how often it is
   called uses the same constant as its row in SCHED_config.c, so the two cannot disagree. Each
   must be a whole multiple of SCHED_TICK_RATE_MSECS (SCHED_init() rejects it otherwise). */
#define SCHED_PERIOD_10MS       ( 10u )
#define SCHED_PERIOD_20MS       ( 20u )
#define SCHED_PERIOD_50MS       ( 50u )
#define SCHED_PERIOD_100MS      ( 100u )
#define SCHED_PERIOD_200MS      ( 200u )
#define SCHED_PERIOD_500MS      ( 500u )

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern const SCHED_cfg_st app_sched_cfg_s;

#endif /* SCHED_CONFIG_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
