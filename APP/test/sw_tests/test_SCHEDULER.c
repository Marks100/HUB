/*! \file
*               Author: mstewart
*   \brief      Scheduler - software test
*
*   \details    The scheduler runs on the one clock of the test (TMD_TIME): the clock counts up a millisecond at a time, and the
*               scheduler's tick (SysTick) comes once every SCHED_TICK_RATE_MSECS of it. The ECU's own run time keeps up with the
*               clock. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "SCHED_config.h"
#include "TIME_MGR.h"

#include "TMD_INIT.h"
#include "TMD_SYSTICK.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/***************************************************************************************************
**                              The clock                                                          **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify the clock counts up a millisecond at a time: advancing 1 ms a thousand times moves it on by at least 1 ms
*                  each time and by less than 2 (the firmware that runs at the end of a step is part of the time, not on top of it),
*                  it never goes back, and its three readings - ns, us and ms - agree every time; and the scheduler's tick came as often
*                  as its period says in the time that went by
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_clock_counts_up_a_millisecond_at_a_time( void )
{
    const u64_t start_ns    = TMD_TIME_get_current_time_ns();
    const u32_t start_ticks = TMD_SYSTICK_get_tick_count();
    u64_t       previous_ns = start_ns;
    u64_t       now_ns;
    u32_t       expected_ticks;
    u32_t       i;

    for( i = 0u; i < 1000u; i++ )
    {
        TMD_TIME_advance_time_ms( 1u );

        now_ns = TMD_TIME_get_current_time_ns();

        TEST_ASSERT_TRUE_MESSAGE( now_ns >= ( previous_ns + 1000000u ), "a step moved the clock on by less than a millisecond" );
        TEST_ASSERT_TRUE_MESSAGE( now_ns <  ( previous_ns + 2000000u ), "a step moved the clock on by two milliseconds or more" );
        TEST_ASSERT_EQUAL_UINT32( (u32_t)( now_ns / 1000000u ), TMD_TIME_get_current_time_ms() );
        TEST_ASSERT_TRUE( ( now_ns / 1000u ) == TMD_TIME_get_current_time_us() );

        previous_ns = now_ns;
    }

    TEST_ASSERT_TRUE( ( now_ns - start_ns ) >= 1000000000u );    /* a second of steps is at least a second */

    /* And the scheduler's tick, the event the clock exists to schedule, came once every SCHED_TICK_RATE_MSECS of that time: 200 in a second at 5 ms */
    expected_ticks = (u32_t)( ( now_ns - start_ns ) / ( (u64_t)SCHED_TICK_RATE_MSECS * 1000000u ) );

    TEST_ASSERT_UINT32_WITHIN( 1u, expected_ticks, TMD_SYSTICK_get_tick_count() - start_ticks );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the scheduler loses no ticks: TIME_increment_time runs every 10 ms and counts what it is given, so ten seconds
*                  of the clock of the test is ten seconds on the ECU's own run time, to within a period. A pass that overran the tick,
*                  or ticks that merged, would leave it behind
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_run_time_keeps_up_with_the_clock( void )
{
    const u64_t start_ns = TMD_TIME_get_current_time_ns();
    const u32_t start_ms = TIME_get_cumulative_run_time_ms();
    u32_t       clock_ms;
    u32_t       run_time_ms;

    TMD_TIME_advance_time_s( 10u );

    clock_ms    = (u32_t)( ( TMD_TIME_get_current_time_ns() - start_ns ) / 1000000u );
    run_time_ms = TIME_get_cumulative_run_time_ms() - start_ms;

    TEST_ASSERT_UINT32_WITHIN( 2u * SCHED_PERIOD_10MS, clock_ms, run_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU's run time still keeps up with the clock over ten minutes: a tick lost now and then, too few to show
*                  in ten seconds, would add up to more than a period. It is also the long run the speed of the tests is measured on
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_run_time_keeps_up_with_the_clock_for_ten_minutes( void )
{
    const u64_t start_ns = TMD_TIME_get_current_time_ns();
    const u32_t start_ms = TIME_get_cumulative_run_time_ms();
    u32_t       clock_ms;
    u32_t       run_time_ms;

    TMD_TIME_advance_time_s( 600u );

    clock_ms    = (u32_t)( ( TMD_TIME_get_current_time_ns() - start_ns ) / 1000000u );
    run_time_ms = TIME_get_cumulative_run_time_ms() - start_ms;

    TEST_ASSERT_UINT32_WITHIN( 2u * SCHED_PERIOD_10MS, clock_ms, run_time_ms );
}

/****************************** END OF FILE *******************************************************/
