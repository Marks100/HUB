/*! \file
*               Author: mstewart
*   \brief      What counts time - software test
*
*   \details    The firmware times its short delays on the DWT cycle counter (DWT_delay_us()). A test calls the delay and looks at
*               the one clock of the test (TMD_TIME) to see how long it took. Every test case starts as a virgin ECU
*               (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "DWT.h"

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

/****************************** END OF FILE *******************************************************/
