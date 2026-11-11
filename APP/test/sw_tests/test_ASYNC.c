/*! \file
*               Author: mstewart
*   \brief      The hardware goes on while the firmware runs - software test
*
*   \details    The firmware takes time to run: every function it enters costs time (SW_TEST_NS_PER_ENTRY in SW_TEST_STUBS.c),
*               and the timers count that time while it runs, so a timer's interrupt comes when its time is up whatever the firmware is
*               doing, and a more urgent one preempts a handler that is running. Every test case starts as a virgin ECU
*               (setUp, SW_TEST_STUBS.c).
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
#define TEST_RUN_MS             ( 200u )
#define TEST_WORK_CALLS         ( 200u )     /* what TIM1's handler does: 200 functions of the firmware, about 170 us */
#define TEST_TIM2_PRESCALER     ( 71u )      /* 1 us a count */
#define TEST_TIM2_RELOAD        ( 99u )      /* 100 us: TIM2's handler stops it, so it interrupts once */

static u32_t test_calls_s;                     /* the handler ran */
static u32_t test_tim1_active_s;               /* TIM1's handler is running */
static u32_t test_tim2_calls_s;
static u32_t test_tim2_inside_tim1_s;          /* ... of them while TIM1's handler was running: one handler inside the other */
static u8_t  test_scratch_s[4];

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU with TIM1 set up as SENT_CFG_init() does it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    test_calls_s            = 0u;
    test_tim1_active_s      = FALSE;
    test_tim2_calls_s       = 0u;
    test_tim2_inside_tim1_s = 0u;

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
*   \brief         The handler: count the call
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_timer_handler( void )
{
    test_calls_s++;
}

/*!
****************************************************************************************************
*
*   \brief         Start the 300 us timer and let 200 ms go by
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_run_the_timer( void )
{
    HAL_TIM1_register_callback( test_timer_handler );
    HAL_TIM1_start_periodic( TEST_PERIOD_COUNTS );

    TMD_TIME_advance_time_ms( TEST_RUN_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the time is the same: the timer interrupts as often as it would if the firmware took no time, to within a
*                  period or two
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_timer_counts_the_same_time_whatever_the_firmware_takes( void )
{
    test_run_the_timer();

    TEST_ASSERT_INT_WITHIN( 2, (int)( ( TEST_RUN_MS * 1000u ) / TEST_PERIOD_US ), (int)test_calls_s );
}

/*!
****************************************************************************************************
*
*   \brief         TIM1's handler: stop the timer and work for 170 us, in functions of the firmware, each of which costs time
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_tim1_busy_handler( void )
{
    u32_t i;

    test_tim1_active_s = TRUE;

    HAL_TIM1_stop();
    HAL_TIM2_start();    /* its event is 100 us from now, in the middle of the work below */

    for( i = 0u; i < TEST_WORK_CALLS; i++ )
    {
        STDC_memset( test_scratch_s, 0u, (u16_t)sizeof( test_scratch_s ) );    /* a function of the firmware: it costs time */
    }

    test_tim1_active_s = FALSE;
}

/*!
****************************************************************************************************
*
*   \brief         TIM2's handler: count the call, and whether TIM1's handler was running when it came
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_tim2_handler( void )
{
    test_tim2_calls_s++;

    if( test_tim1_active_s == TRUE )
    {
        test_tim2_inside_tim1_s++;
    }
}

/*!
****************************************************************************************************
*
*   \brief         TIM1 interrupts once, 300 us in, and its handler starts TIM2 and is busy for 170 us; TIM2 interrupts once, 100 us later
*
*   \author        mstewart
*
*   \param[in]     tim2_priority - The preempt priority of TIM2 (HAL_TIM1_init gives TIM1 priority 1; 0 is the most urgent)
*
*   \return        none
*
*****************************************************************************************************/
static void test_run_tim1_busy_and_tim2_fast( u32_t tim2_priority )
{
    HAL_TIM2_init();
    TIM_PrescalerConfig( TIM2, TEST_TIM2_PRESCALER, TIM_PSCReloadMode_Immediate );
    TIM_SetAutoreload( TIM2, TEST_TIM2_RELOAD );
    TIM_ClearFlag( TIM2, TIM_FLAG_Update );    /* the prescaler's update event set it */
    NVIC_SetPriority( TIM2_IRQn, tim2_priority );

    HAL_TIM1_register_callback( test_tim1_busy_handler );
    HAL_TIM2_register_callback( test_tim2_handler );

    HAL_TIM1_start_periodic( TEST_PERIOD_COUNTS );

    TMD_TIME_advance_time_ms( 3u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an interrupt of a higher priority preempts a handler that is running: TIM2 (0) comes while TIM1's handler (1)
*                  works, and its handler runs inside it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_more_urgent_interrupt_preempts_a_running_handler( void )
{
    test_run_tim1_busy_and_tim2_fast( 0u );

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 1u, test_tim2_inside_tim1_s, "TIM2's handler did not run inside TIM1's" );
    TEST_ASSERT_EQUAL_UINT32( 1u, test_tim2_calls_s );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an interrupt of the same priority waits for the handler that is running: TIM2 (1) is held until TIM1's returns,
*                  and runs after it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_interrupt_of_the_same_priority_waits_for_a_running_handler( void )
{
    test_run_tim1_busy_and_tim2_fast( 1u );

    TEST_ASSERT_EQUAL_UINT32( 0u, test_tim2_inside_tim1_s );
    TEST_ASSERT_EQUAL_UINT32( 1u, test_tim2_calls_s );    /* it ran, after TIM1's handler returned */
}

/****************************** END OF FILE *******************************************************/
