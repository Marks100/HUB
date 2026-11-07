/*! \file
*               Author: mstewart
*   \brief      ERX heartbeat state over UDS - software test
*
*   \details    A tester on CAN1 reads the ECU's heartbeat DID (DID_APP_ERX_HEARTBEATS, 0xF910) from the ECU's own UDS server:
*               the physical request goes to 0x7E0, the answer comes back on 0x7E8 through the real CANTP, which for a
*               response of this size sends a first frame and waits for the tester's flow control. The tester is the UDS tester
*               model (TMD_UDS), which every test starts pointed at the ECU's own server (SW_TEST_ECU_UDS_REQUEST_ID) and which
*               answers that and puts the frames together. The HUB follows the front axle's centre line (slot 0 of port 0) and
*               outer left wheel (slot 0 of port 1), app_autocfg_init_s. The ERXs are TMD_ERX instances on CAN2, powered
*               by the HUB's own supply pins, so what the tester reads is what the ECU made of their heartbeats. Every test
*               case starts as a virgin ECU with no ERX placed (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "UDS_config.h"

#include "TMD_ERX.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_SLOT                   ( AUTOCFG_HEARTBEAT_INFO_SLOT_SIZE )    /* the record of a slot starts at slot * TEST_SLOT */
#define TEST_PORT_1                 ( AUTOCFG_V2_MAX_ERX_PER_PORT * TEST_SLOT )    /* the records of port 1 start here: the centre line is port 0's, the outer left wheel port 1's */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with no ERX placed
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
*   \brief         Verify a tester reads what the ECU made of the heartbeats: an ERX that is sending shows alive with its frames
*                  and alive counter, one that is unplugged shows its position and nothing else, and no other slot has a record
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_reads_the_erx_heartbeat_state( void )
{
    u8_t  data[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u8_t  empty[SW_TEST_TESTER_RESPONSE_MAX_LEN] = { 0u };
    u16_t len = 0u;

    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged from the start */
    TMD_TIME_advance_time_ms( 2100u );    /* four heartbeats, the last carrying the 2 seconds */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_data_by_identifier( DID_APP_ERX_HEARTBEATS, data, sizeof( data ), &len ) );
    TEST_ASSERT_EQUAL_UINT16( AUTOCFG_HEARTBEAT_INFO_SIZE, len );

    /* Port 0, slot 0: the centre line, unplugged. Only the position. */
    TEST_ASSERT_EQUAL_HEX8( APP_ISO_AXLE_1_CENTRE, data[0u] );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( empty, &data[1u], TEST_SLOT - 1u );

    /* Port 0, slots 1 to 4 */
    TEST_ASSERT_EQUAL_HEX8_ARRAY( empty, &data[1u * TEST_SLOT], TEST_PORT_1 - ( 1u * TEST_SLOT ) );

    /* Port 1, slot 0: the outer left wheel, alive, 4 frames, counter 2 */
    TEST_ASSERT_EQUAL_HEX8( APP_ISO_AXLE_1_OUTER_LEFT, data[TEST_PORT_1] );
    TEST_ASSERT_EQUAL_HEX8( AUTOCFG_HEARTBEAT_INFO_ALIVE, data[TEST_PORT_1 + 1u] );
    TEST_ASSERT_EQUAL_UINT32( 4u, STDC_make_32_bit( data[TEST_PORT_1 + 2u], data[TEST_PORT_1 + 3u], data[TEST_PORT_1 + 4u], data[TEST_PORT_1 + 5u] ) );
    TEST_ASSERT_EQUAL_UINT32( 2u, STDC_make_32_bit( data[TEST_PORT_1 + 6u], data[TEST_PORT_1 + 7u], data[TEST_PORT_1 + 8u], data[TEST_PORT_1 + 9u] ) );

    /* Port 1, slots 1 to 4 */
    TEST_ASSERT_EQUAL_HEX8_ARRAY( empty, &data[TEST_PORT_1 + TEST_SLOT], len - ( TEST_PORT_1 + TEST_SLOT ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify a tester sees an ERX that was unplugged as lost, not alive, with the frames it sent before still counted
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_sees_an_erx_that_was_unplugged_as_lost( void )
{
    u8_t  data[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t len = 0u;

    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );

    TMD_TIME_advance_time_ms( 1000u );    /* two heartbeats */
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_OUTER_LEFT ) );    /* unplugged */
    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + 100u );    /* the timeout without a heartbeat */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_data_by_identifier( DID_APP_ERX_HEARTBEATS, data, sizeof( data ), &len ) );

    TEST_ASSERT_EQUAL_UINT16( AUTOCFG_HEARTBEAT_INFO_SIZE, len );
    TEST_ASSERT_EQUAL_HEX8( APP_ISO_AXLE_1_OUTER_LEFT, data[TEST_PORT_1] );
    TEST_ASSERT_EQUAL_HEX8( AUTOCFG_HEARTBEAT_INFO_LOST, data[TEST_PORT_1 + 1u] );
    TEST_ASSERT_EQUAL_UINT32( 2u, STDC_make_32_bit( data[TEST_PORT_1 + 2u], data[TEST_PORT_1 + 3u], data[TEST_PORT_1 + 4u], data[TEST_PORT_1 + 5u] ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester sees an ERX that was unplugged from the start as lost once the timeout went by:
*                  not alive, no frames, no counter
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_sees_an_erx_that_was_never_plugged_in_as_lost( void )
{
    u8_t  data[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u8_t  empty[SW_TEST_TESTER_RESPONSE_MAX_LEN] = { 0u };
    u16_t len = 0u;

    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged from the start */
    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + 100u );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_data_by_identifier( DID_APP_ERX_HEARTBEATS, data, sizeof( data ), &len ) );

    TEST_ASSERT_EQUAL_HEX8( APP_ISO_AXLE_1_CENTRE, data[0u] );
    TEST_ASSERT_EQUAL_HEX8( AUTOCFG_HEARTBEAT_INFO_LOST, data[1u] );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( empty, &data[2u], TEST_SLOT - 2u );
}

/****************************** END OF FILE *******************************************************/
