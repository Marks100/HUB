/*! \file
*               Author: mstewart
*   \brief      UDS client and the gateway's diagnostics - software test
*
*   \details    The nodes of CAN2 are TMD_ERX instances (xCOMMON, test/support/models/TMD_ERX) placed at an ISO position and
*               powered by the HUB's own supply pins: with TMD_ERX_UDS they answer the ECU's UDS requests through ISO-TP as a real node would,
*               from the table of DIDs built into the model, on 0x700 plus their position. The ECU's client
*               has NODE_INFO read the part number of its selected node (DID 0xF187) through UDS_CLIENT,
*               PDUR, DIAG_GW, the real CANTP, CANIF and the MCP2515 driver; a test moves the time on and looks at what the
*               client holds. A tester on CAN1 reaches a node through the gateway the same way. Every test case starts as a
*               virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "UDS_CLIENT.h"
#include "NODE_INFO.h"

#include "TMD_CAN.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_TESTER_REQUEST_ID      ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_2_INNER_RIGHT )
#define TEST_TESTER_RESPONSE_ID     ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_2_INNER_RIGHT )
#define TEST_NODE_INFO_MS           ( 2500u )                    /* the node is up after about 0.6 s, and NODE_INFO's two reads are a period (1 s) apart: both done */
#define TEST_NODE_UP_AND_READ_MS    ( 800u )                     /* the node is up after about 0.6 s and NODE_INFO has read its part number a moment later */
#define TEST_WRITE_MS               ( 500u )                     /* a 19 byte request across the gateway and back: well inside the gateway's hold of the node */
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

    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );    /* the ERX the HUB's UDS client reads by itself: request ID 0x716, the first of the gateway's range */
    TMD_ERX_place_instance( APP_ISO_AXLE_2_INNER_RIGHT, TMD_ERX_PORT_A );    /* the ERX for the tester's requests: request ID 0x729, response ID 0x629 */

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
    const u8_t* expected_p   = TMD_ERX_UDS_get_did_value( NODE_INFO_PART_NUMBER_DID, &expected_len );
    const u8_t* value_p;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    value_p = NODE_INFO_get( NODE_INFO_PART_NUMBER, &len );

    TEST_ASSERT_NOT_NULL( expected_p );
    TEST_ASSERT_NOT_NULL( value_p );
    TEST_ASSERT_EQUAL_UINT8( expected_len, len );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_p, value_p, expected_len );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
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

    TMD_ERX_UDS_set_responding( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), FALSE );

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    TEST_ASSERT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) >= 2u );
    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) );
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

    TMD_ERX_UDS_inject_nrc( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), 0x22u, 0x31u );

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    TEST_ASSERT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the client writes a value of several frames to its node: the 16 byte chassis number goes
*                  out as a first frame, the node's flow control, and the consecutive frames, and the node keeps the
*                  whole value and answers positively
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_client_writes_a_multi_frame_chassis_number_to_its_node( void )
{
    const u8_t  chassis[16u]  = { 'C', 'H', 'A', 'S', 'S', 'I', 'S', '-', '0', '1', '2', '3', '4', '5', '6', '7' };
    u8_t        written_len   = 0u;
    const u8_t* written_p;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );    /* NODE_INFO has both items, so the client is free */
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_WRITE_MS );

    written_p = TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), &written_len );

    TEST_ASSERT_NOT_NULL( written_p );
    TEST_ASSERT_EQUAL_UINT8( sizeof( chassis ), written_len );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( chassis, written_p, sizeof( chassis ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );    /* the node's 6E answered it, rather than the client timing out */
}

/*!
****************************************************************************************************
*
*   \brief         Verify a node that refuses the write (NRC) ends the request without keeping anything
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_node_that_refuses_the_chassis_number_keeps_nothing( void )
{
    const u8_t chassis[16u] = { 'C', 'H', 'A', 'S', 'S', 'I', 'S', '-', '0', '1', '2', '3', '4', '5', '6', '7' };
    u8_t       written_len  = 0u;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );
    TMD_ERX_UDS_inject_nrc( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), 0x2Eu, 0x33u );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_WRITE_MS );

    TEST_ASSERT_NULL( TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), &written_len ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
}
/*!
****************************************************************************************************
*
*   \brief         Verify NODE_INFO reads the node's software version as well as its part number: the answer is a
*                  single frame, where the part number's takes several
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_node_info_reads_the_software_version_as_well_as_the_part_number( void )
{
    u8_t        expected_len = 0u;
    u8_t        len          = 0u;
    const u8_t* expected_p   = TMD_ERX_UDS_get_did_value( NODE_INFO_VERSION_DID, &expected_len );
    const u8_t* value_p;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    value_p = NODE_INFO_get( NODE_INFO_VERSION, &len );

    TEST_ASSERT_NOT_NULL( expected_p );
    TEST_ASSERT_NOT_NULL( value_p );
    TEST_ASSERT_EQUAL_UINT8( expected_len, len );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_p, value_p, expected_len );
    TEST_ASSERT_NOT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify NODE_INFO stops asking once it has everything
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_node_info_stops_asking_once_it_has_everything( void )
{
    const u8_t slot = TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT );
    u32_t      requests;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );
    requests = TMD_ERX_UDS_get_request_count( slot );

    TEST_ASSERT_EQUAL_UINT32( 2u, requests );    /* one read of each item */

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );
    TEST_ASSERT_EQUAL_UINT32( requests, TMD_ERX_UDS_get_request_count( slot ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a TesterPresent with its response suppressed ends as soon as it is sent: the node gets the
*                  request, answers nothing, and the client is free without waiting out a timeout
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_tester_present_ends_when_sent_without_waiting_for_a_response( void )
{
    const u8_t slot = TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT );
    u32_t      requests;
    u32_t      responses;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );
    requests  = TMD_ERX_UDS_get_request_count( slot );
    responses = TMD_ERX_UDS_get_response_count( slot );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_tester_present( NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL_UINT32( requests + 1u, TMD_ERX_UDS_get_request_count( slot ) );
    TEST_ASSERT_EQUAL_UINT32( responses, TMD_ERX_UDS_get_response_count( slot ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
}
/*!
****************************************************************************************************
*
*   \brief         Verify the client can make a request straight after a reply: the gateway still has the node for the
*                  client when NODE_INFO's part number arrives, and a write at once is not refused
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_straight_after_a_reply_is_not_refused( void )
{
    const u8_t  chassis[4u] = { 'A', 'B', 'C', 'D' };
    u8_t        len         = 0u;
    u8_t        written_len = 0u;
    const u8_t* written_p;

    TMD_TIME_advance_time_ms( TEST_NODE_UP_AND_READ_MS );

    TEST_ASSERT_NOT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    written_p = TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ), &written_len );
    TEST_ASSERT_NOT_NULL( written_p );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( chassis, written_p, sizeof( chassis ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify the HUB sends nothing to a node that is not configured: the position is not one of the
*                  ERXs the HUB follows, so nothing is put on the bus for it, however long NODE_INFO waits
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_nothing_is_sent_to_a_node_that_is_not_configured( void )
{
    const u8_t not_configured = 1u;    /* ISO position APP_ISO_POSITION_FIRST + 1: no row of app_autocfg_init_s */

    TEST_ASSERT_EQUAL( FALSE, app_node_is_available( not_configured ) );
    TEST_ASSERT_EQUAL( PASS, NODE_INFO_select_node( not_configured ) );

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID + not_configured ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
    TEST_ASSERT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the HUB sends nothing to a configured node that is unplugged, and does not even start a
*                  request for it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_nothing_is_sent_to_a_node_that_is_unplugged( void )
{
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) );

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    TEST_ASSERT_EQUAL( FALSE, app_node_is_available( 0u ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID ) );
    TEST_ASSERT_EQUAL( 0u, TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) ) );
    TEST_ASSERT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a refused reservation also stops a request that is not NODE_INFO's: a write to a node that
*                  is not there is refused, not sent
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_to_a_node_that_is_not_there_is_refused( void )
{
    const u8_t chassis[4u] = { 'A', 'B', 'C', 'D' };

    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );

    TEST_ASSERT_EQUAL( FAIL, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify selecting another node clears the part number read from the first, and
*                  selecting the same node keeps it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_selecting_another_node_clears_the_part_number( void )
{
    u8_t len = 0u;

    TMD_TIME_advance_time_ms( TEST_NODE_INFO_MS );
    TEST_ASSERT_NOT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );

    TEST_ASSERT_EQUAL( PASS, NODE_INFO_select_node( 0u ) );
    TEST_ASSERT_NOT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );

    TEST_ASSERT_EQUAL( PASS, NODE_INFO_select_node( 1u ) );
    TEST_ASSERT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &len ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, len );
    TEST_ASSERT_EQUAL_UINT8( 1u, UDS_CLIENT_get_node() );
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

    TEST_ASSERT_EQUAL_UINT8( 0x03u, TMD_ERX_UDS_get_session( TMD_ERX_find_instance( APP_ISO_AXLE_2_INNER_RIGHT ) ) );
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

    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_2_INNER_RIGHT ) );

    test_make_request( &request, request_data, 2u );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( APP_ISO_AXLE_2_INNER_RIGHT ) ) );
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
