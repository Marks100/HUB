/*! \file
*               Author: mstewart
*   \brief      A tester's UDS diagnostics through the HUB to an ERX - software test
*
*   \details    A tester on CAN1 sends a UDS request to an ERX at the second axle's inner right wheel (ISO position 0x29, request ID 0x729, answer ID 0x629). The
*               HUB's gateway (DIAG_GW, CANTP, CANIF and the MCP2515 driver) carries it onto CAN2, the ERX answers, and the
*               answer comes back to the tester on CAN1. The tester is the UDS tester model (TMD_UDS, pointed at the
*               ERX with TMD_CANTP_set_ids()): it answers a first frame with a flow control, which the HUB has to carry to
*               the ERX the same way, and puts the frames together.
*
*               The tests look only at what the tester gets back: they never ask the ERX what it received. Every test case
*               starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"

#include "TMD_CANTP.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "UDS_CLIENT.h"
#include "NODE_INFO.h"
#include "SW_TEST_STUBS.h"

/* The diagnostic IDs of the ERXs under test: the tester asks on 0x700 plus the position and the ERX answers on 0x600 plus it */
#define TEST_AXLE_2_INNER_RIGHT_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_2_INNER_RIGHT )    /* 0x729 */
#define TEST_AXLE_2_INNER_RIGHT_RESPONSE_ID         ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_2_INNER_RIGHT )    /* 0x629 */
#define TEST_AXLE_2_OUTER_RIGHT_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_2_OUTER_RIGHT )    /* 0x72A */
#define TEST_AXLE_2_OUTER_RIGHT_RESPONSE_ID         ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_2_OUTER_RIGHT )    /* 0x62A */
#define TEST_AXLE_1_OUTER_LEFT_REQUEST_ID               ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_1_OUTER_LEFT )         /* 0x716 */
#define TEST_AXLE_1_OUTER_LEFT_RESPONSE_ID              ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_1_OUTER_LEFT )         /* 0x616 */
#define TEST_AXLE_5_OUTER_RIGHT_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_5_OUTER_RIGHT )    /* 0x75A */
#define TEST_AXLE_5_OUTER_RIGHT_RESPONSE_ID         ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_5_OUTER_RIGHT )    /* 0x65A */
#define TEST_PART_NUMBER_DID        ( 0xF187u )
#define TEST_PART_NUMBER            "PN-1234567"  /* what the ERX reports for DID 0xF187: 10 bytes, so an answer of several frames */
#define TEST_PART_NUMBER_LEN        ( 10u )
#define TEST_GATEWAY_HOLD_MS        ( 2000u )     /* how long a tester waits for P2*: longer than the gateway holds its request for the HUB's own client (1000 ms) */
#define TEST_CLIENT_READ_MS         ( 2500u )     /* longer than the HUB's client waits between its reads */
#define TEST_ERX_UP_MS              ( 700u )      /* the ERX's heartbeat is coming in and the HUB's client has started reading it */
#define TEST_SILENT_TIMEOUT_MS      ( 300u )      /* how long the tester waits for an ERX that does not answer */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU with an ERX at the second axle's inner right wheel on CAN2, running
*                  on the supply the HUB switches on, and the tester on CAN1 pointed at it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    TMD_CANTP_set_ids( TEST_AXLE_2_INNER_RIGHT_REQUEST_ID, TEST_AXLE_2_INNER_RIGHT_RESPONSE_ID );    /* the tester talks to the ERX, not to the ECU */

    TMD_ERX_place_instance( APP_ISO_AXLE_2_INNER_RIGHT, TMD_ERX_PORT_A );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a DiagnosticSessionControl from the tester reaches the ERX and its answer comes back: a positive
*                  response for the extended session
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_session_request_from_a_tester_is_answered_by_the_erx( void )
{
    const u8_t request[2u] = { TMD_UDS_SID_DIAGNOSTIC_SESSION_CONTROL, TMD_UDS_SESSION_EXTENDED };
    u8_t       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t      len = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, 2u, response, sizeof( response ), &len, NULL_P ) );

    TEST_ASSERT_TRUE( len >= 2u );
    TEST_ASSERT_EQUAL_HEX8( 0x50u, response[0u] );    /* positive response to 0x10 */
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, response[1u] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a TesterPresent from the tester is answered by the ERX: 0x7E and its sub-function
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_present_from_a_tester_is_answered_by_the_erx( void )
{
    const u8_t request[2u] = { TMD_UDS_SID_TESTER_PRESENT, TMD_UDS_TESTER_PRESENT_NO_SUPPRESS };
    u8_t       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t      len = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, 2u, response, sizeof( response ), &len, NULL_P ) );

    TEST_ASSERT_EQUAL_UINT16( 2u, len );
    TEST_ASSERT_EQUAL_HEX8( 0x7Eu, response[0u] );
    TEST_ASSERT_EQUAL_HEX8( 0x00u, response[1u] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester reads a DID that takes several frames: the ERX's first frame crosses to CAN1, the tester's
*                  flow control crosses to the ERX, the consecutive frames follow, and the tester holds the whole part number
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reads_a_multi_frame_did_from_the_erx( void )
{
    u8_t  data[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t len = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_data_by_identifier( TEST_PART_NUMBER_DID, data, sizeof( data ), &len, NULL_P ) );

    TEST_ASSERT_EQUAL_UINT16( TEST_PART_NUMBER_LEN, len );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( TEST_PART_NUMBER, data, TEST_PART_NUMBER_LEN );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a negative response of the ERX comes back to the tester as it is: a DID the ERX does not have is
*                  refused with 'request out of range' (0x31)
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_negative_response_of_the_erx_reaches_the_tester( void )
{
    const u8_t request[3u] = { TMD_UDS_SID_READ_DATA_BY_IDENTIFIER, 0xF1u, 0xFFu };
    u8_t       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t      len = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( request, 3u, response, sizeof( response ), &len, NULL_P ) );

    TEST_ASSERT_EQUAL_UINT16( 3u, len );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NEGATIVE_RESPONSE_SID, response[0u] );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SID_READ_DATA_BY_IDENTIFIER, response[1u] );
    TEST_ASSERT_EQUAL_HEX8( 0x31u, response[2u] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester gets nothing back from an ERX that is unplugged, and that the silence is the ERX's: a
*                  DiagnosticSessionControl to the unplugged ERX's own ID (0x729) is not answered, while the same service to
*                  the healthy ERX beside it, on its own ID (0x72A), through the same HUB, is
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_gets_no_answer_from_an_unplugged_erx( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_2_OUTER_RIGHT, TMD_ERX_PORT_A );

    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_2_INNER_RIGHT ) );    /* the ERX at the second axle's inner right wheel is unplugged */
    TMD_UDS_set_timeout( TEST_SILENT_TIMEOUT_MS );

    TEST_ASSERT_NOT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );    /* no answer: the model reports it as a timeout or a transport error */

    TMD_CANTP_set_ids( TEST_AXLE_2_OUTER_RIGHT_REQUEST_ID, TEST_AXLE_2_OUTER_RIGHT_RESPONSE_ID );    /* now the healthy ERX at the second axle's outer right wheel, on its own IDs */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester reaches the ERX at the first ID of the gateway's range, 0x716, which the HUB's own UDS client
*                  is reading from start-up: the gateway holds the tester's request until the client's time is up, then carries it
*                  to CAN2 and the answer back, and the client's own reads go on
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reaches_the_erx_at_the_first_id_of_the_gateway_range( void )
{
    const u8_t request[2u] = { TMD_UDS_SID_DIAGNOSTIC_SESSION_CONTROL, TMD_UDS_SESSION_EXTENDED };
    u8_t       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u8_t       part_number_len = 0u;
    u16_t      len = 0u;

    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );
    TMD_CANTP_set_ids( TEST_AXLE_1_OUTER_LEFT_REQUEST_ID, TEST_AXLE_1_OUTER_LEFT_RESPONSE_ID );
    TMD_UDS_set_timeout( TEST_GATEWAY_HOLD_MS );

    /* The HUB's client waits for the ERX to be up (its heartbeat is coming in) before it reads it. Once it has, the gateway holds the tester's request until the client's time is up */
    TMD_TIME_advance_time_ms( TEST_ERX_UP_MS );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, 2u, response, sizeof( response ), &len, NULL_P ) );

    TEST_ASSERT_TRUE( len >= 2u );
    TEST_ASSERT_EQUAL_HEX8( 0x50u, response[0u] );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, response[1u] );

    TMD_TIME_advance_time_ms( TEST_CLIENT_READ_MS );

    TEST_ASSERT_NOT_NULL( NODE_INFO_get( NODE_INFO_PART_NUMBER, &part_number_len ) );    /* the HUB's own client still reads the same ERX */
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester reaches the ERX at the last position, the rearmost axle's outer right wheel (request ID
*                  0x75A, answer ID 0x65A): the far end of the gateway's range is reachable too
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reaches_the_erx_at_the_last_position( void )
{
    const u8_t request[2u] = { TMD_UDS_SID_DIAGNOSTIC_SESSION_CONTROL, TMD_UDS_SESSION_EXTENDED };
    u8_t       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t      len = 0u;

    TMD_ERX_place_instance( APP_ISO_AXLE_5_OUTER_RIGHT, TMD_ERX_PORT_A );
    TMD_CANTP_set_ids( TEST_AXLE_5_OUTER_RIGHT_REQUEST_ID, TEST_AXLE_5_OUTER_RIGHT_RESPONSE_ID );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, 2u, response, sizeof( response ), &len, NULL_P ) );

    TEST_ASSERT_TRUE( len >= 2u );
    TEST_ASSERT_EQUAL_HEX8( 0x50u, response[0u] );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, response[1u] );
}

/*!
****************************************************************************************************
*
*   \brief         The tester asks the ERX at an ISO position for the extended session, on that ERX's own IDs
*
*   \author        mstewart
*
*   \param[in]     iso_position - The ERX's position: the tester asks on 0x700 plus it and it answers on 0x600 plus it
*
*   \return        none
*
*   \note          Fails the test if the ERX does not answer with a positive response.
*
*****************************************************************************************************/
static void test_open_extended_session_on( u8_t iso_position )
{
    TMD_CANTP_set_ids( TMD_ERX_UDS_REQUEST_BASE_ID + iso_position, TMD_ERX_UDS_RESPONSE_BASE_ID + iso_position );
    TMD_UDS_set_timeout( TEST_GATEWAY_HOLD_MS );    /* the HUB's client reads the ERXs in turn: the gateway may hold the request until its time is up */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         The session an ERX is in, as the ERX model has it
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position the ERX was placed at
*
*   \return        The session: TMD_UDS_SESSION_DEFAULT unless a request for another reached the ERX
*
*****************************************************************************************************/
static u8_t test_session_of( u8_t iso_position )
{
    return( TMD_ERX_UDS_get_session( TMD_ERX_find_instance( iso_position ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester reaches each ERX the HUB is configured for, one at a time, with all of them on the bus: the
*                  centre line (port 0) and then the outer left wheel (port 1). A request to one reaches only that ERX, so it
*                  is the only one that changes session, and the other ERXs, the one on the other port and the one that is
*                  not configured, stay in the default session
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reaches_each_configured_erx_one_at_a_time( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );

    test_open_extended_session_on( APP_ISO_AXLE_1_CENTRE );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, test_session_of( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_2_INNER_RIGHT ) );

    TMD_ERX_UDS_init();    /* every ERX back in the default session */

    test_open_extended_session_on( APP_ISO_AXLE_1_OUTER_LEFT );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, test_session_of( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_2_INNER_RIGHT ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the same in the other order, the port 1 ERX first, so neither depends on being the first the
*                  gateway carried a request to
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reaches_each_configured_erx_one_at_a_time_in_the_other_order( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );

    test_open_extended_session_on( APP_ISO_AXLE_1_OUTER_LEFT );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, test_session_of( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_1_CENTRE ) );

    TMD_ERX_UDS_init();    /* every ERX back in the default session */

    test_open_extended_session_on( APP_ISO_AXLE_1_CENTRE );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_EXTENDED, test_session_of( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SESSION_DEFAULT, test_session_of( APP_ISO_AXLE_1_OUTER_LEFT ) );
}
/****************************** END OF FILE *******************************************************/
