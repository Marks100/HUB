/*! \file
*               Author: mstewart
*   \brief      An interrupt at every point of the firmware - software test
*
*   \details    A tester request on CAN1 reaches the gateway through the bxCAN receive interrupt (HAL_CAN, CANIF, PDUR, DIAG_GW) and
*               leaves on CAN2 through the scheduler's tick and the MCP2515 driver: an interrupt handing a frame to code that is
*               not in the interrupt. Where the interrupt runs in that code is not decided by the test of test_GATEWAY.c: it runs at
*               the first register access after the frame arrives.
*
*               TMD_PREEMPT holds the interrupt and runs it at a function entry the test chooses. The sweep runs the scenario once for
*               each of the entries the firmware makes in the time the frame needs, so the interrupt runs at the top of each
*               function of the scheduler's pass in turn, and the outcome has to be the same every time: the request leaves on CAN2
*               once, as it was sent. A race on a variable the interrupt and the tick share shows as the first entry at which it is not.
*               Every run starts as a virgin ECU.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"

#include "vhw.h"

#include "TMD_CAN.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_MCP2515.h"
#include "TMD_PREEMPT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_FRAME_SETTLE_MS     ( 10u )     /* two scheduler passes: a frame that arrived crosses the gateway */
#define TEST_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID + APP_ISO_AXLE_3_CENTRE )    /* 0x738: a node's request, the third axle's centre line */
#define TEST_MAX_RUNS            ( 150u )    /* a sweep runs the scenario this many times at most: the entries are sampled evenly if there are more */
#define TEST_NEVER               ( 0xFFFFFFFFu )

static const u8_t test_request_data_s[8u] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
static char       test_message_s[160u];

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
*   \brief         The request of the tester on CAN1, as the bus model takes it
*
*   \author        mstewart
*
*   \param[out]    frame_p    The frame
*
*   \return        none
*
*****************************************************************************************************/
static void test_make_request( TMD_CAN_message_st* frame_p )
{
    STDC_memset( frame_p, 0x00u, (u16_t)sizeof( TMD_CAN_message_st ) );

    frame_p->message_id  = TEST_REQUEST_ID;
    frame_p->data_length = 8u;
    STDC_memcpy( frame_p->data, test_request_data_s, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Send the request with the interrupts armed for a function entry, and let it cross the gateway
*
*   \author        mstewart
*
*   \param[in]     function_entry    Where the interrupts run
*
*   \return        How many function entries the firmware made in the window, up to the one the interrupts ran at
*
*   \note          The interrupts have the window to run in. A frame that arrives at the end of it still needs its two scheduler
*                  passes to cross the gateway, so the same time goes by after the window
*
*****************************************************************************************************/
static u32_t test_send_request_with_the_interrupt_at( u32_t function_entry )
{
    TMD_CAN_message_st request;
    u32_t              entries;

    test_make_request( &request );

    TMD_PREEMPT_arm( function_entry );
    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );
    entries = TMD_PREEMPT_disarm();

    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    return( entries );
}

/*!
****************************************************************************************************
*
*   \brief         Assert the request left on CAN2 once, as it was sent, and nothing came back on CAN1
*
*   \author        mstewart
*
*   \param[in]     function_entry    Where the interrupts ran, for the message
*
*   \return        none
*
*****************************************************************************************************/
static void test_assert_the_request_crossed_the_gateway( u32_t function_entry )
{
    TMD_CAN_message_st sent;

    (void)snprintf( test_message_s, sizeof( test_message_s ), "interrupt at function entry %lu: function %p, called from %p (TMD_PREEMPT_arm is %p)",
                    (unsigned long)function_entry, TMD_PREEMPT_get_function(), TMD_PREEMPT_get_caller(), (void*)TMD_PREEMPT_arm );

    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ), test_message_s );
    TEST_ASSERT_TRUE_MESSAGE( TMD_CAN_consume_message_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID, &sent ), test_message_s );
    TEST_ASSERT_EQUAL_UINT8_MESSAGE( 8u, sent.data_length, test_message_s );
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE( test_request_data_s, sent.data, 8u, test_message_s );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_REQUEST_ID ), test_message_s );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 0u, TMD_MCP2515_get_protocol_error_count(), test_message_s );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE( 0u, vhw_irq_get_stuck_count(), test_message_s );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the model holds the interrupt: with it armed for an entry the firmware never makes, the request is not
*                  taken in, and it crosses the gateway when the interrupt is released
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_interrupt_waits_for_the_entry_it_was_armed_for( void )
{
    TMD_CAN_message_st request;

    test_make_request( &request );

    TMD_PREEMPT_arm( TEST_NEVER );
    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ) );
    TEST_ASSERT_FALSE( TMD_PREEMPT_has_run() );

    TMD_PREEMPT_disarm();
    TMD_TIME_advance_time_ms( 2u * TEST_FRAME_SETTLE_MS );

    test_assert_the_request_crossed_the_gateway( TEST_NEVER );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the interrupt runs at the entry it was armed for: at the very first the request is taken in and
*                  crosses the gateway
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_interrupt_runs_at_the_entry_it_was_armed_for( void )
{
    (void)test_send_request_with_the_interrupt_at( 1u );

    TEST_ASSERT_TRUE( TMD_PREEMPT_has_run() );
    test_assert_the_request_crossed_the_gateway( 1u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the request crosses the gateway however the interrupt falls in the scheduler's passes: one run for each
*                  function entry the firmware makes while the frame crosses, or an even sample of them if there are many
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_request_crosses_the_gateway_with_the_interrupt_at_every_function_entry( void )
{
    u32_t window;
    u32_t stride;
    u32_t entry;
    u32_t runs = 0u;

    window = test_send_request_with_the_interrupt_at( TEST_NEVER );    /* how many entries the firmware makes in the time: the size of the window */
    TEST_ASSERT_TRUE_MESSAGE( window > 0u, "the firmware made no instrumented function entry: is the library built with -finstrument-functions?" );

    stride = ( window / TEST_MAX_RUNS ) + 1u;

    for( entry = 1u; entry <= window; entry += stride )
    {
        TMD_INIT_init( &sw_test_init_cfg_g );

        (void)test_send_request_with_the_interrupt_at( entry );

        TEST_ASSERT_TRUE_MESSAGE( TMD_PREEMPT_has_run(), "the interrupt did not run at the entry it was armed for" );
        test_assert_the_request_crossed_the_gateway( entry );
        runs++;
    }

    (void)snprintf( test_message_s, sizeof( test_message_s ), "%lu runs over a window of %lu entries", (unsigned long)runs, (unsigned long)window );
    TEST_ASSERT_TRUE_MESSAGE( runs > 1u, test_message_s );
    printf( "[SWEEP] %s\n", test_message_s );
}

/****************************** END OF FILE *******************************************************/
