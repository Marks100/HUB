/*! \file
*               Author: mstewart
*   \brief      UDS client and the gateway's diagnostics - software test
*
*   \details    The nodes of CAN2 are TMD_ERX instances (xCOMMON, test/support/models/TMD_ERX) placed at an ISO position and
*               powered by the HUB's own supply pins: with TMD_ERX_UDS they answer the ECU's UDS requests through ISO-TP as a real node would,
*               from the table of DIDs built into the model, on 0x700 plus their position. The ECU's client
*               reads the part number of its selected node by itself (UDS_CLIENT auto-read, DID 0xF187) through UDS_CLIENT,
*               PDUR, DIAG_GW, the real CANTP, CANIF and the MCP2515 driver; a test moves the time on and looks at what the
*               client holds. A tester on CAN1 reaches a node through the gateway the same way. Every test case starts as a
*               virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "UDS_CLIENT.h"

#include "TMD_CAN.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_TESTER_REQUEST_ID      ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_2_INNER_RIGHT )
#define TEST_TESTER_RESPONSE_ID     ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_2_INNER_RIGHT )
#define TEST_AUTO_READ_MS           ( 2500u )                    /* longer than the client's period between automatic reads */
#define TEST_TESTER_ANSWER_MS       ( 50u )                      /* the request crosses the gateway, the node answers, the answer crosses back */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with the ERX at the client's position (the front axle's outer left wheel) on CAN2 answering the
*                  client's requests and an ERX at the second axle's inner right wheel answering the tester's
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    sw_test_place_erx( APP_ISO_AXLE_1_OUTER_LEFT );    /* the ERX the HUB's UDS client reads by itself: request ID 0x716, the first of the gateway's range */
    sw_test_place_erx( APP_ISO_AXLE_2_INNER_RIGHT );    /* the ERX for the tester's requests: request ID 0x729, response ID 0x629 */

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         A tester's request as the CAN bus model takes it: a single frame carrying the service and its data
*
*   \author        mstewart
*
*   \param[out]    frame_p - The frame, to the node of the tester's address
*   \param[in]     data_p  - The service ID and its data
*   \param[in]     len     - How many bytes, at most 7
*
*   \return        none
*
*****************************************************************************************************/
static void test_make_request( TMD_CAN_message_st* frame_p, const u8_t* data_p, u8_t len )
{
    STDC_memset( frame_p, 0x00u, (u16_t)sizeof( TMD_CAN_message_st ) );

    frame_p->message_id  = TEST_TESTER_REQUEST_ID;
    frame_p->data_length = 8u;
    frame_p->data[0u]    = len;
    STDC_memcpy( &frame_p->data[1u], data_p, len );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the client reads a value that takes several frames: the node's part number is 10 bytes, so the
*                  response is a first frame, the client's flow control, and the consecutive frames, and the client
*                  holds the whole value
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_client_reads_a_multi_frame_part_number_from_its_node( void )
{
    u8_t        expected_len = 0u;
    u8_t        len          = 0u;
    const u8_t* expected_p   = TMD_ERX_UDS_get_did_value( APP_NODE_PART_NUMBER_DID, &expected_len );
    const u8_t* value_p;

    TMD_TIME_advance_time_ms( TEST_AUTO_READ_MS );

    value_p = UDS_CLIENT_get_did( APP_NODE_PART_NUMBER_DID, &len );

    TEST_ASSERT_NOT_NULL( expected_p );
    TEST_ASSERT_NOT_NULL( value_p );
    TEST_ASSERT_EQUAL_UINT8( expected_len, len );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_p, value_p, expected_len );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a silent node leaves the client without a value, and the client asks again after its timeout
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_silent_node_leaves_the_client_without_a_part_number_and_it_asks_again( void )
{
    u8_t len = 0u;

    TMD_ERX_UDS_set_responding( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ), FALSE );

    TMD_TIME_advance_time_ms( TEST_AUTO_READ_MS );

    TEST_ASSERT_NULL( UDS_CLIENT_get_did( APP_NODE_PART_NUMBER_DID, &len ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ) ) >= 2u );
    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_ERX_UDS_get_response_count( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a negative response is not kept as a value
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_negative_response_leaves_the_client_without_a_part_number( void )
{
    u8_t len = 0u;

    TMD_ERX_UDS_inject_nrc( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ), 0x22u, 0x31u );

    TMD_TIME_advance_time_ms( TEST_AUTO_READ_MS );

    TEST_ASSERT_NULL( UDS_CLIENT_get_did( APP_NODE_PART_NUMBER_DID, &len ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( sw_test_erx_slot( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester on CAN1 reaches a node through the gateway and gets its answer back: a
*                  DiagnosticSessionControl (extended) goes out on CAN2, the node enters the session and answers, and the
*                  answer leaves on CAN1
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_session_request_is_answered_by_the_node_through_the_gateway( void )
{
    const u8_t         request_data[ 2u ]  = { 0x10u, 0x03u };
    const u8_t         response_data[ 8u ] = { 0x06u, 0x50u, 0x03u, 0x00u, 0x32u, 0x01u, 0xF4u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st answer;

    test_make_request( &request, request_data, 2u );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL_UINT8( 0x03u, TMD_ERX_UDS_get_session( sw_test_erx_slot( APP_ISO_AXLE_2_INNER_RIGHT ) ) );
    TEST_ASSERT_EQUAL( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_TESTER_RESPONSE_ID ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_TESTER_RESPONSE_ID, &answer ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( response_data, answer.data, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester reads a DID that fits one frame through the gateway: the node's hardware version is
*                  3 bytes, so the answer is a single frame, 0x62, the DID and the value
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reads_a_short_did_from_the_node_through_the_gateway( void )
{
    const u8_t         request_data[ 3u ] = { 0x22u, 0xF1u, 0x93u };
    u8_t               expected_len       = 0u;
    const u8_t*        expected_p         = TMD_ERX_UDS_get_did_value( TMD_ERX_UDS_DID_HARDWARE_VERSION, &expected_len );
    TMD_CAN_message_st request;
    TMD_CAN_message_st answer;

    test_make_request( &request, request_data, 3u );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_TESTER_RESPONSE_ID, &answer ) );
    TEST_ASSERT_EQUAL_UINT8( 3u + expected_len, answer.data[0u] );
    TEST_ASSERT_EQUAL_HEX8( 0x62u, answer.data[1u] );
    TEST_ASSERT_EQUAL_HEX8( 0xF1u, answer.data[2u] );
    TEST_ASSERT_EQUAL_HEX8( 0x93u, answer.data[3u] );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_p, &answer.data[4u], expected_len );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an ERX that is unplugged does not answer: the tester's request reaches CAN2 but nothing comes back
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_unplugged_erx_does_not_answer_the_tester( void )
{
    const u8_t         request_data[ 2u ] = { 0x10u, 0x03u };
    TMD_CAN_message_st request;

    sw_test_unplug_erx( APP_ISO_AXLE_2_INNER_RIGHT );

    test_make_request( &request, request_data, 2u );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_ERX_UDS_get_request_count( sw_test_erx_slot( APP_ISO_AXLE_2_INNER_RIGHT ) ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_TESTER_RESPONSE_ID ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester that reads a DID the node does not have gets 'request out of range' back through the
*                  gateway
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reading_an_unknown_did_gets_a_negative_response_through_the_gateway( void )
{
    const u8_t         request_data[ 3u ]  = { 0x22u, 0xF1u, 0xFFu };
    const u8_t         response_data[ 8u ] = { 0x03u, 0x7Fu, 0x22u, 0x31u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st answer;

    test_make_request( &request, request_data, 3u );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_TESTER_RESPONSE_ID, &answer ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( response_data, answer.data, 8u );
}

/****************************** END OF FILE *******************************************************/
