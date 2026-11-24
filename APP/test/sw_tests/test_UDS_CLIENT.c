/*! \file
*               Author: mstewart
*   \brief      UDS client - software test
*
*   \details    The nodes of CAN2 are TMD_ERX instances (xCOMMON, test/support/models/TMD_ERX) placed at an ISO position and
*               powered by the HUB's own supply pins: with TMD_ERX_UDS they answer the ECU's UDS requests through ISO-TP as a real node would,
*               from the table of DIDs built into the model, on 0x700 plus their position. The HUB's client (UDS_CLIENT) is used by ERX_IDENT,
*               which reads the hardware part number of the first ERX of each port (DID 0xF191) by itself, through UDS_CLIENT, PDUR, DIAG_GW, the real
*               CANTP, CANIF and the MCP2515 driver; a test moves the time on and looks at what ERX_IDENT holds, or uses the client itself. A tester on
*               CAN1 reaching a node through the gateway is in test_UDS_TESTER.c. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*
*               The HUB follows the front axle's centre line (port 0) and outer left wheel (port 1), app_erx_mgr_init_s: the ERX the tests place is the first
*               of port 1, the first position the gateway carries (request ID 0x716), and port 0 has none.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "ERX_IDENT.h"
#include "ERX_MGR.h"
#include "UDS_CLIENT.h"

#include "TMD_CAN.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_POSITION               ( APP_ISO_AXLE_1_OUTER_LEFT )                           /* the first ERX of port 1: the one ERX_IDENT reads by itself */
#define TEST_NODE                   ( (u8_t)( TEST_POSITION - APP_ISO_POSITION_FIRST ) )    /* its node, as DIAG_GW numbers them */
#define TEST_NOT_CONFIGURED_NODE    ( 1u )                                              /* APP_ISO_POSITION_FIRST + 1: no ERX of the HUB's table */
#define TEST_IDENT_DONE_MS          ( 3000u )                                           /* the node is up and ERX_IDENT has read its part number: the client is free */
#define TEST_PORT_1_POSITION        ( APP_ISO_AXLE_1_CENTRE )                          /* the first ERX of port 0, which these tests do not place */
#define TEST_AFTER_RETRY_MS         ( ERX_IDENT_RETRY_MS + 1500u )                      /* a failed request is repeated after ERX_IDENT_RETRY_MS */
#define TEST_STEP_MS                ( 10u )
#define TEST_WRITE_MS               ( 500u )                                            /* a 19 byte request across the gateway and back: well inside the gateway's hold of the node */
#define TEST_TESTER_ANSWER_MS       ( 50u )                                             /* the request goes out, the node answers */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with the ERX at the first position of port 1 on CAN2 answering the client's
*                  requests
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    (void)TMD_ERX_place_instance( TEST_POSITION, TMD_ERX_PORT_B );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         The part number the ERX model holds, which is what ERX_IDENT is to have read
*
*   \author        mstewart
*
*   \return        The part number, ERX_IDENT_HW_PART_NUMBER_LEN bytes
*
*****************************************************************************************************/
static const u8_t* test_erx_model_part_number( void )
{
    u8_t        len = 0u;
    const u8_t* part_number_p = TMD_ERX_UDS_get_did_value( TMD_ERX_UDS_DID_HW_PART_NUMBER, &len );

    TEST_ASSERT_NOT_NULL( part_number_p );
    TEST_ASSERT_EQUAL_UINT8( ERX_IDENT_HW_PART_NUMBER_LEN, len );

    return( part_number_p );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the client reads a value that takes several frames: the node's part number is 10 bytes, so the
*                  response is a first frame, the client's flow control, and the consecutive frames, and ERX_IDENT
*                  holds the whole value
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_client_reads_a_multi_frame_part_number_from_its_node( void )
{
    u8_t part_number[ERX_IDENT_HW_PART_NUMBER_LEN];

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( TRUE, ERX_IDENT_get( 1u, part_number ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( test_erx_model_part_number(), part_number, ERX_IDENT_HW_PART_NUMBER_LEN );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( TEST_POSITION ) ) > 0u );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( TEST_POSITION ) ) > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a silent node leaves ERX_IDENT without a part number, and the client asks again after its timeout
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_silent_node_leaves_the_client_without_a_part_number_and_it_asks_again( void )
{
    u8_t part_number[ERX_IDENT_HW_PART_NUMBER_LEN];

    TMD_ERX_UDS_set_responding( TMD_ERX_find_instance( TEST_POSITION ), FALSE );

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS + TEST_AFTER_RETRY_MS );

    TEST_ASSERT_EQUAL( FALSE, ERX_IDENT_get( 1u, part_number ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( TEST_POSITION ) ) >= 2u );
    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( TEST_POSITION ) ) );
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
    u8_t part_number[ERX_IDENT_HW_PART_NUMBER_LEN];

    TMD_ERX_UDS_inject_nrc( TMD_ERX_find_instance( TEST_POSITION ), 0x22u, 0x31u );

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( FALSE, ERX_IDENT_get( 1u, part_number ) );
    TEST_ASSERT_TRUE( TMD_ERX_UDS_get_response_count( TMD_ERX_find_instance( TEST_POSITION ) ) > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify ERX_IDENT stops asking once it has the part number
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_erx_ident_stops_asking_once_it_has_everything( void )
{
    const u8_t slot = TMD_ERX_find_instance( TEST_POSITION );
    u32_t      requests;

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );
    requests = TMD_ERX_UDS_get_request_count( slot );

    TEST_ASSERT_EQUAL_UINT32( 1u, requests );    /* one read of the one item */

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );
    TEST_ASSERT_EQUAL_UINT32( requests, TMD_ERX_UDS_get_request_count( slot ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the client can make a request straight after a reply: the gateway still has the node for the
*                  client when ERX_IDENT's part number arrives, and a write at once is not refused
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_straight_after_a_reply_is_not_refused( void )
{
    const u8_t  chassis[4u] = { 'A', 'B', 'C', 'D' };
    u8_t        part_number[ERX_IDENT_HW_PART_NUMBER_LEN];
    u16_t       waited_ms   = 0u;
    u8_t        written_len = 0u;
    const u8_t* written_p;

    while( ( ERX_IDENT_get( 1u, part_number ) == FALSE ) && ( waited_ms < TEST_IDENT_DONE_MS ) )    /* up to the moment the reply is in */
    {
        TMD_TIME_advance_time_ms( TEST_STEP_MS );
        waited_ms = (u16_t)( waited_ms + TEST_STEP_MS );
    }

    TEST_ASSERT_EQUAL( TRUE, ERX_IDENT_get( 1u, part_number ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    written_p = TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( TEST_POSITION ), &written_len );
    TEST_ASSERT_NOT_NULL( written_p );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( chassis, written_p, sizeof( chassis ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the HUB sends nothing for a port with no ERX on it: its table has an ERX for port 0, which is never heard, so ERX_IDENT
*                  asks nobody for it, however long it waits
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_nothing_is_sent_for_a_port_with_no_erx( void )
{
    u8_t part_number[ERX_IDENT_HW_PART_NUMBER_LEN];

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( FALSE, ERX_MGR_is_erx_available( TEST_PORT_1_POSITION ) );
    TEST_ASSERT_EQUAL( FALSE, ERX_IDENT_get( 0u, part_number ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID + ( TEST_PORT_1_POSITION - APP_ISO_POSITION_FIRST ) ) );
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
    const u8_t  chassis[16u] = { 'C', 'H', 'A', 'S', 'S', 'I', 'S', '-', '0', '1', '2', '3', '4', '5', '6', '7' };
    u8_t        written_len  = 0u;
    const u8_t* written_p;

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_WRITE_MS );

    written_p = TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( TEST_POSITION ), &written_len );

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

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );
    TMD_ERX_UDS_inject_nrc( TMD_ERX_find_instance( TEST_POSITION ), 0x2Eu, 0x33u );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_WRITE_MS );

    TEST_ASSERT_NULL( TMD_ERX_UDS_get_written_value( TMD_ERX_find_instance( TEST_POSITION ), &written_len ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
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
    const u8_t slot = TMD_ERX_find_instance( TEST_POSITION );
    u32_t      requests;
    u32_t      responses;

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );
    requests  = TMD_ERX_UDS_get_request_count( slot );
    responses = TMD_ERX_UDS_get_response_count( slot );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_tester_present( NULL_P ) );
    TMD_TIME_advance_time_ms( TEST_TESTER_ANSWER_MS );

    TEST_ASSERT_EQUAL_UINT32( requests + 1u, TMD_ERX_UDS_get_request_count( slot ) );
    TEST_ASSERT_EQUAL_UINT32( responses, TMD_ERX_UDS_get_response_count( slot ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the HUB sends nothing to a node that is not configured: the position is not one of the
*                  ERXs the HUB follows, so a write to it is refused and nothing is put on the bus for it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_nothing_is_sent_to_a_node_that_is_not_configured( void )
{
    const u8_t chassis[4u] = { 'A', 'B', 'C', 'D' };

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( FALSE, ERX_MGR_is_erx_available( (u8_t)( APP_ISO_POSITION_FIRST + TEST_NOT_CONFIGURED_NODE ) ) );
    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NOT_CONFIGURED_NODE ) );
    TEST_ASSERT_EQUAL( FAIL, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );

    TMD_TIME_advance_time_ms( TEST_WRITE_MS );

    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID + TEST_NOT_CONFIGURED_NODE ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
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
    (void)TMD_ERX_unplug_instance( TMD_ERX_find_instance( TEST_POSITION ) );

    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( FALSE, ERX_MGR_is_erx_available( TEST_POSITION ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID + TEST_NODE ) );
    TEST_ASSERT_EQUAL( 0u, TMD_ERX_UDS_get_request_count( TMD_ERX_find_instance( TEST_POSITION ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a refused reservation also stops a request that is not ERX_IDENT's: a write to a node that
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

    (void)TMD_ERX_unplug_instance( TMD_ERX_find_instance( TEST_POSITION ) );
    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL( FAIL, UDS_CLIENT_write_did( TMD_ERX_UDS_DID_CHASSIS_NUMBER, chassis, (u16_t)sizeof( chassis ), NULL_P ) );
    TEST_ASSERT_EQUAL( FALSE, UDS_CLIENT_is_busy() );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, APP_NODE_REQUEST_BASE_ID + TEST_NODE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the client keeps the node it was given
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_client_keeps_the_node_it_is_given( void )
{
    TMD_TIME_advance_time_ms( TEST_IDENT_DONE_MS );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NODE ) );
    TEST_ASSERT_EQUAL_UINT8( TEST_NODE, UDS_CLIENT_get_node() );

    TEST_ASSERT_EQUAL( PASS, UDS_CLIENT_set_node( TEST_NOT_CONFIGURED_NODE ) );
    TEST_ASSERT_EQUAL_UINT8( TEST_NOT_CONFIGURED_NODE, UDS_CLIENT_get_node() );
}

/****************************** END OF FILE *******************************************************/