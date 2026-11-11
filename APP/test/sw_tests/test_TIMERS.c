/*! \file
*               Author: mstewart
*   \brief      Timers - software test
*
*   \details    TIM1 as the firmware uses it for a waveform (HAL_TIM1_*, the path SENT_TX takes): a periodic update interrupt, and
*               a handler that sets the length of the next phase. The virtual STM32 counts the 72 MHz clock through the
*               prescaler and runs the real TIM1_UP_IRQHandler, so the test counts how often the handler it registered runs in the
*               time that goes by. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "HAL_TIM.h"

#include "vhw.h"
#include "stm32f10x_tim.h"

#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_PRESCALER          ( 215u )     /* 72 MHz / 216: 3 us per count, as SENT_config.c sets it */
#define TEST_PERIOD_COUNTS      ( 100u )     /* 300 us */
#define TEST_PERIOD_US          ( 300u )
#define TEST_RUN_MS             ( 30u )      /* a hundred periods */

static u32_t test_handler_calls_s;
static u16_t test_next_period_counts_s;

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with TIM1 set up as SENT_CFG_init() does it and a handler that counts
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    test_handler_calls_s      = 0u;
    test_next_period_counts_s = 0u;

    HAL_TIM1_init();
    HAL_TIM1_set_prescaler( TEST_PRESCALER );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         The handler: count the call, and set the next period if the test wants a different one
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_timer_handler( void )
{
    test_handler_calls_s++;

    if( test_next_period_counts_s != 0u )
    {
        HAL_TIM1_set_reload( test_next_period_counts_s );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Verify a periodic update interrupt runs once per period: 30 ms of a 300 us period is a hundred calls
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_periodic_timer_interrupt_runs_once_per_period( void )
{
    HAL_TIM1_register_callback( test_timer_handler );
    HAL_TIM1_start_periodic( TEST_PERIOD_COUNTS );

    TMD_TIME_advance_time_ms( TEST_RUN_MS );

    TEST_ASSERT_INT_WITHIN( 1, (int)( ( TEST_RUN_MS * 1000u ) / TEST_PERIOD_US ), (int)test_handler_calls_s );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a handler sets the length of the phase that follows, as SENT_TX does: the reload alternates between a
*                  300 us and a 600 us phase, so the pair takes 900 us and 36 ms holds forty pairs
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_handler_that_sets_the_next_period_makes_a_variable_waveform( void )
{
    HAL_TIM1_register_callback( test_timer_handler );
    test_next_period_counts_s = 2u * TEST_PERIOD_COUNTS;    /* the phase after the first is twice as long */
    HAL_TIM1_start_periodic( TEST_PERIOD_COUNTS );

    TMD_TIME_advance_time_ms( 36u );    /* the handler keeps the reload at 600 us from the first call on: 36 ms is 60 phases of it */

    TEST_ASSERT_INT_WITHIN( 2, 60, (int)test_handler_calls_s );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a stopped timer runs its handler no more
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_stopped_timer_runs_no_more_interrupts( void )
{
    u32_t calls_when_stopped;

    HAL_TIM1_register_callback( test_timer_handler );
    HAL_TIM1_start_periodic( TEST_PERIOD_COUNTS );
    TMD_TIME_advance_time_ms( 10u );
    TEST_ASSERT_TRUE( test_handler_calls_s > 0u );

    HAL_TIM1_stop();
    calls_when_stopped = test_handler_calls_s;
    TMD_TIME_advance_time_ms( TEST_RUN_MS );

    TEST_ASSERT_EQUAL_UINT32( calls_when_stopped, test_handler_calls_s );
}

/****************************** END OF FILE *******************************************************/
