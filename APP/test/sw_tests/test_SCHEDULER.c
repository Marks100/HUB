/*! \file
*               Author: mstewart
*   \brief      Scheduler - software test
*
*   \details    The scheduler runs on the one clock of the test (TMD_TIME). A test moves the clock on and looks at the ECU's own run
*               time (TIME_MGR), which the scheduler adds to every 10 ms and which has to keep up with the clock. Every test case
*               starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "SCHED_config.h"
#include "TIME_MGR.h"

#include "TMD_INIT.h"
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

/*!
****************************************************************************************************
*
*   \brief         Verify the scheduler ran its rows at their period: the firmware's own run time, which a 10 ms row
*                  adds to, advances by the time the test advanced
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_firmware_run_time_advances_with_the_time_advanced( void )
{
    u64_t run_time_at_start_ms = TIME_get_cumulative_run_time_ms_64();

    TMD_TIME_advance_time_ms( 1000u );

    TEST_ASSERT_EQUAL_UINT32( 1000u, (u32_t)( TIME_get_cumulative_run_time_ms_64() - run_time_at_start_ms ) );
}

/****************************** END OF FILE *******************************************************/
