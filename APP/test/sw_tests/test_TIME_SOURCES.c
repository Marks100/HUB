/*! \file
*               Author: mstewart
*   \brief      What counts time - software test
*
*   \details    Everything in the ECU that counts time counts the one clock of the test (TMD_TIME): the DWT cycle counter the
*               firmware times its delays with, and the RTC. The real world time is a date a test sets, which goes on with the clock.
*               Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "DWT.h"

#include "vhw.h"
#include "stm32f10x_rtc.h"

#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_CYCLES_PER_MS         ( 72000u )      /* 72 MHz */
#define TEST_REAL_WORLD_S          ( 1767225600u ) /* 2026-01-01 00:00:00 UTC */

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
**                              DWT cycle counter                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify the cycle counter counts the core clock: a millisecond of the clock of the test is 72000 cycles
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_cycle_counter_counts_the_core_clock( void )
{
    const u32_t start = DWT_get_count();
    u32_t       end;

    TMD_TIME_advance_time_ms( 1u );

    end = DWT_get_count();

    TEST_ASSERT_UINT32_WITHIN( 400u, TEST_CYCLES_PER_MS, end - start );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a delay on the cycle counter takes the time it was asked for on the clock of the test
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_delay_on_the_cycle_counter_takes_its_time( void )
{
    const u64_t start_us = TMD_TIME_get_current_time_us();
    u64_t       taken_us;

    DWT_delay_us( 200u );

    taken_us = TMD_TIME_get_current_time_us() - start_us;

    TEST_ASSERT_TRUE( ( taken_us >= 200u ) && ( taken_us <= 230u ) );
}

/***************************************************************************************************
**                              RTC                                                                **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify the RTC counts the clock of the test: a one second prescaler counts one a second from what was set
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_rtc_counts_the_clock_of_the_test( void )
{
    RTC_SetPrescaler( 32767u );
    RTC_SetCounter( 1000u );

    TMD_TIME_advance_time_s( 5u );

    TEST_ASSERT_EQUAL_UINT32( 1005u, RTC_GetCounter() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the prescaler sets the rate: 10 Hz counts ten in a second
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_rtc_prescaler_sets_its_rate( void )
{
    RTC_SetPrescaler( 3276u );
    RTC_SetCounter( 0u );

    TMD_TIME_advance_time_s( 1u );

    TEST_ASSERT_UINT32_WITHIN( 1u, 10u, RTC_GetCounter() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the alarm flag is set when the counter passes the alarm, and not before
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_rtc_alarm_flag_is_set_when_the_counter_passes_it( void )
{
    RTC_SetPrescaler( 32767u );
    RTC_SetCounter( 1000u );
    RTC_SetAlarm( 1003u );

    TMD_TIME_advance_time_s( 2u );

    TEST_ASSERT_EQUAL( RESET, RTC_GetFlagStatus( RTC_FLAG_ALR ) );

    TMD_TIME_advance_time_s( 1u );

    TEST_ASSERT_EQUAL( SET, RTC_GetFlagStatus( RTC_FLAG_ALR ) );
}

/***************************************************************************************************
**                              Real world time                                                    **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify the real world time is the date a test set, and goes on with the clock of the test and across a restart
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_real_world_time_goes_on_from_the_date_a_test_set( void )
{
    TMD_TIME_set_real_world_time( TEST_REAL_WORLD_S );

    TMD_TIME_advance_time_s( 90u );

    TEST_ASSERT_EQUAL_UINT32( TEST_REAL_WORLD_S + 90u, (u32_t)TMD_TIME_get_real_world_time() );

    TMD_INIT_restart_ecu( NON_VIRGIN );

    TEST_ASSERT_TRUE( (u32_t)TMD_TIME_get_real_world_time() >= ( TEST_REAL_WORLD_S + 90u ) );
}


/****************************** END OF FILE *******************************************************/
