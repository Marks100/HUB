/*! \file
*               Author: mstewart
*   \brief      Gateway - software test
*
*   \details    The tester's bus (CAN1) and the nodes' bus (CAN2) through the real HAL_BRD, HAL_CAN, HAL_SPI, the SPL,
*               the TJA1051 driver, CANIF, PDUR, DIAG_GW and MCP2515 driver, built from the project's own tables in
*               INTEGRATION_STUBS.c. The STM32 is the virtual hardware (vhw.h); the buses are two TMD_CAN instances.
*               CAN1 reaches its bus through the TJA1051 model, which the firmware has to take out of standby (EN pin,
*               TJA1051_init) before a frame passes either way; CAN2 is the MCP2515 on SPI1 and its chip model. The wiring and
*               the start-up list are in sw_stubs/SW_TEST_STUBS.c; setUp() starts every test case as a virgin ECU.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "HAL_config.h"
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "HAL_SPI.h"
#include "SCHED_config.h"
#include "TIME_MGR.h"

#include "vhw.h"
#include "stm32f10x_can.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"

#include "TMD_CAN.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_MCP2515.h"
#include "TMD_SYSTICK.h"
#include "TMD_TIME.h"
#include "TMD_TJA1051.h"

#include "SW_TEST_STUBS.h"

#define TEST_FRAME_SETTLE_MS     ( 10u )    /* two scheduler passes: a frame that arrived crosses the gateway */
#define TEST_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_3_CENTRE )    /* 0x738: a node's request, the third axle's centre line */
#define TEST_RESPONSE_ID         ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_3_CENTRE )    /* 0x638: and its response */

/*!
****************************************************************************************************
*
*   \brief         A frame as the CAN bus model takes it
*
*   \author        mstewart
*
*   \param[out]    frame_p - The frame
*   \param[in]     id      - Arbitration ID, 11 bit
*   \param[in]     data_p  - Eight data bytes
*
*   \return        none
*
*****************************************************************************************************/
static void test_make_frame( TMD_CAN_message_st* frame_p, u32_t id, const u8_t* data_p )
{
    STDC_memset( frame_p, 0x00u, (u16_t)sizeof( TMD_CAN_message_st ) );

    frame_p->message_id  = id;
    frame_p->data_length = 8u;
    STDC_memcpy( frame_p->data, data_p, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU: the shared start-up list of SW_TEST_STUBS.c
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
*   \brief         Verify the firmware takes the TJA1051 out of standby (EN pin driven by TJA1051_init through
*                  HAL_BRD), which is what lets frames through the transceiver at all
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_firmware_switches_the_tja1051_to_normal_mode( void )
{
    TEST_ASSERT_EQUAL( TMD_TJA1051_MODE_NORMAL, TMD_TJA1051_get_mode( TEST_BUS_TESTER ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester request on CAN1 leaves on CAN2 with the same ID and payload, and
*                  nothing goes back out on CAN1
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_request_reaches_the_node_on_can2( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_REQUEST_ID, sent.message_id );
    TEST_ASSERT_EQUAL( 8u, sent.data_length );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( data, sent.data, 8u );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_REQUEST_ID ) );
    TEST_ASSERT_EQUAL( 0u, TMD_MCP2515_get_protocol_error_count() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU can be power cycled: after a restart that keeps its NVM, app_main() has run again on a
*                  freshly powered MCU and the gateway forwards a tester request as before
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_gateway_forwards_again_after_the_ecu_restarts( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st sent;

    TMD_INIT_restart_ecu();

    test_make_frame( &request, TEST_REQUEST_ID, data );
    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( TMD_TJA1051_MODE_NORMAL, TMD_TJA1051_get_mode( TEST_BUS_TESTER ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_REQUEST_ID, sent.message_id );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a node's reply on CAN2 leaves on CAN1 with the same ID and payload
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_node_reply_on_can2_reaches_the_tester_on_can1( void )
{
    const u8_t         request_data[ 8u ]  = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const u8_t         response_data[ 8u ] = { 0x02u, 0x50u, 0x03u, 0x00u, 0x32u, 0x01u, 0xF4u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st response;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, request_data );
    test_make_frame( &response, TEST_RESPONSE_ID, response_data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_NODES, &response );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RESPONSE_ID, sent.message_id );
    TEST_ASSERT_EQUAL( 8u, sent.data_length );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( response_data, sent.data, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify that with the TJA1051 in standby a tester request on CAN1 never reaches the gateway
*                  - the transceiver drops it - so nothing leaves on CAN2
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_does_not_pass_the_tja1051_in_standby( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;

    test_make_frame( &request, TEST_REQUEST_ID, data );
    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_STANDBY );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_TJA1051_get_dropped_rx_count( TEST_BUS_TESTER ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a reply for the tester waits in the bxCAN mailbox while the TJA1051 is in standby, as
*                  the bxCAN retransmits until the bus takes the frame, and leaves once the transceiver is back in
*                  normal mode
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_reply_waits_in_the_mailbox_until_the_tja1051_is_back_in_normal_mode( void )
{
    const u8_t         request_data[ 8u ]  = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const u8_t         response_data[ 8u ] = { 0x02u, 0x50u, 0x03u, 0x00u, 0x32u, 0x01u, 0xF4u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st response;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, request_data );
    test_make_frame( &response, TEST_RESPONSE_ID, response_data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_STANDBY );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_NODES, &response );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID ) );
    TEST_ASSERT_TRUE( CAN_SIM_get_mailboxes_in_use() > 0u );
    TEST_ASSERT_TRUE( TMD_TJA1051_get_blocked_tx_count( TEST_BUS_TESTER ) > 0u );

    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_NORMAL );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );    /* the bxCAN offers the frame again */

    TEST_ASSERT_EQUAL( 0u, CAN_SIM_get_mailboxes_in_use() );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RESPONSE_ID, sent.message_id );
}

/*!
****************************************************************************************************
*
*   \brief         Verify SysTick ticks the scheduler once per tick period the firmware set: advancing the time by a
*                  whole number of periods runs SysTick_Handler that many times
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_systick_ticks_once_per_tick_period_of_the_time_advanced( void )
{
    TMD_TIME_advance_time_ms( 20u * SCHED_TICK_RATE_MSECS );

    TEST_ASSERT_EQUAL_UINT32( 20u, TMD_SYSTICK_get_tick_count() );

    TMD_TIME_advance_time_ms( 5u * SCHED_TICK_RATE_MSECS );

    TEST_ASSERT_EQUAL_UINT32( 25u, TMD_SYSTICK_get_tick_count() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a part of a tick period does not tick: the tick comes on the last millisecond of the
*                  period, and the part left over counts towards the next one
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_part_of_a_tick_period_does_not_tick( void )
{
    TMD_TIME_advance_time_ms( SCHED_TICK_RATE_MSECS - 1u );

    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_SYSTICK_get_tick_count() );

    TMD_TIME_advance_time_ms( 1u );

    TEST_ASSERT_EQUAL_UINT32( 1u, TMD_SYSTICK_get_tick_count() );

    TMD_TIME_advance_time_ms( SCHED_TICK_RATE_MSECS + 1u );

    TEST_ASSERT_EQUAL_UINT32( 2u, TMD_SYSTICK_get_tick_count() );
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
