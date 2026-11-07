/*! \file
*               Author: mstewart
*   \brief      ERX heartbeat - software test
*
*   \details    The heartbeat consumer of AUTOCFG (xCOMMON): an ERX at an ISO position sends CAN ID 0x400 plus that position
*               every 500 ms (0x418 for the ERX at the front axle's centre line) with a counter of the seconds since it
*               started, and the ECU follows the positions its init configured: the front axle's centre line (port 0) and outer
*               left wheel (port 1), app_autocfg_init_s. The positions are the ones of APP_ISO_POSITIONS.h. The ERXs here are
*               TMD_ERX instances on CAN2, placed at a position and powered by the HUB itself: it drives its port supply pins
*               HIGH at start-up, which is all that powers them (it does not run the AUTOCFG master). The heartbeat goes
*               through the MCP2515 driver, CANIF, PDUR and AUTOCFG_receive_heartbeat(). A test unplugs an ERX and plugs it
*               back in, and moves the time on. Every test case starts as a virgin ECU with no ERX placed (setUp,
*               SW_TEST_STUBS.c): a test places the ones it needs, and no more than two send at once, as the MCP2515 has two
*               receive buffers and the third frame of the same millisecond is lost.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "AUTOCFG.h"

#include "TMD_CAN.h"
#include "TMD_ERX.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_SETTLE_MS                  ( 100u )  /* an ERX that was powered sends its next heartbeat */

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
*   \brief         Verify an ERX that is unplugged sends nothing: it is not alive and the ECU has no counter for it, while
*                  the one beside it is alive. It is not lost while it can still be starting up.
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_unplugged_erx_is_not_alive( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged from the start */

    TMD_TIME_advance_time_ms( 2000u );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, AUTOCFG_get_erx_heartbeat_counter( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );    /* within the timeout */
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_OUTER_LEFT ) );    /* the outer left wheel's ERX is running */
}

/*!
****************************************************************************************************
*
*   \brief         Verify an ERX that is unplugged from the start is lost once the timeout went by, although it never sent
*                  a heartbeat, while the one beside it is not
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_erx_that_is_unplugged_from_the_start_is_lost( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged from the start */

    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT32( 0u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_OUTER_LEFT ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an ERX that is plugged in after it was lost from the start is no longer lost
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_erx_that_is_lost_from_the_start_recovers_when_plugged_in( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged from the start */
    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + TEST_SETTLE_MS );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );

    TMD_ERX_plug_in_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );
    TMD_TIME_advance_time_ms( TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU follows only the heartbeats: the rest of AUTOCFG (discovery, the status message) does not
*                  run, so nothing of it goes out on CAN2
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_does_not_run_the_autocfg_master( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 2000u );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, AUTOCFG_REQ_MESSAGE_ID ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, AUTOCFG_STATUS_MSG_ID ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU reads the first heartbeat of an ERX: the ERX starts, sends at its first period, and is alive
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_reads_the_first_heartbeat_of_an_erx( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( TMD_ERX_HEARTBEAT_PERIOD_MS - TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );

    TMD_TIME_advance_time_ms( 2u * TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 1u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_1_CENTRE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU follows the counter of an ERX: after 2.1 s the ERX has sent four heartbeats (every 500 ms)
*                  and the last, sent at 2 s, carried the 2 seconds
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_follows_the_counter_of_an_erx( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 2100u );

    TEST_ASSERT_EQUAL_UINT32( 4u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 2u, AUTOCFG_get_erx_heartbeat_counter( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU notices an ERX that is unplugged: still alive shortly after its last heartbeat, lost once
*                  the timeout has gone by without one
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_notices_an_erx_that_is_unplugged( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 1000u );
    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged */

    TMD_TIME_advance_time_ms( 500u );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );

    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU sees an ERX come back after it was lost, with a later counter than before
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_ecu_sees_an_erx_come_back_after_it_was_lost( void )
{
    u32_t counter_before_unplugging;

    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 1000u );
    counter_before_unplugging = AUTOCFG_get_erx_heartbeat_counter( APP_ISO_AXLE_1_CENTRE );

    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged */
    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );

    TMD_ERX_plug_in_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* plugged back in: the HUB's supply is still on, so it starts again */
    TMD_TIME_advance_time_ms( TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_TRUE( AUTOCFG_get_erx_heartbeat_counter( APP_ISO_AXLE_1_CENTRE ) > counter_before_unplugging );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ERXs are followed one by one: with two running, losing one loses only that one
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_losing_one_erx_does_not_lose_the_other( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );

    TMD_TIME_advance_time_ms( 1000u );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_OUTER_LEFT ) );

    TMD_ERX_unplug_instance( TMD_ERX_find_instance( APP_ISO_AXLE_1_CENTRE ) );    /* unplugged */
    TMD_TIME_advance_time_ms( AUTOCFG_HEARTBEAT_TIMEOUT_MS + TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_CENTRE ) );
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_heartbeat_lost( APP_ISO_AXLE_1_OUTER_LEFT ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a heartbeat from a supported position the HUB does not follow is not followed: the front axle's
*                  centre line is, the second axle's is not
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_heartbeat_from_a_position_the_hub_does_not_follow_is_ignored( void )
{
    TMD_ERX_place_instance( APP_ISO_AXLE_1_CENTRE, TMD_ERX_PORT_A );
    TMD_ERX_place_instance( APP_ISO_AXLE_2_CENTRE, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 1000u );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_CENTRE ) );    /* control: the bus carries heartbeats, so the silence of the second axle's ERX is the ECU's */
    TEST_ASSERT_EQUAL( FALSE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_2_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, AUTOCFG_get_erx_heartbeat_counter( APP_ISO_AXLE_2_CENTRE ) );
    TEST_ASSERT_EQUAL_UINT32( 0u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_2_CENTRE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the first and the last heartbeat ID are followed: the ERXs at the lowest position (the front axle's
*                  outer left, ID 0x416) and the highest (the rearmost axle's outer right, ID 0x45A), counted frame by frame
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_first_and_the_last_heartbeat_id_are_followed( void )
{
    AUTOCFG_init_st init_cfg = app_autocfg_init_s;

    STDC_memset( init_cfg.erx_locations, 0x00, sizeof( init_cfg.erx_locations ) );
    init_cfg.erx_locations[0u].iso_position = APP_ISO_AXLE_1_OUTER_LEFT;
    init_cfg.erx_locations[1u].iso_position = APP_ISO_AXLE_5_OUTER_RIGHT;

    AUTOCFG_init( &init_cfg );
    TMD_ERX_place_instance( APP_ISO_AXLE_1_OUTER_LEFT, TMD_ERX_PORT_B );
    TMD_ERX_place_instance( APP_ISO_AXLE_5_OUTER_RIGHT, TMD_ERX_PORT_A );

    TMD_TIME_advance_time_ms( 2100u );

    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL( TRUE, AUTOCFG_is_erx_alive( APP_ISO_AXLE_5_OUTER_RIGHT ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_1_OUTER_LEFT ) );
    TEST_ASSERT_EQUAL_UINT32( 4u, AUTOCFG_get_erx_heartbeat_received_count( APP_ISO_AXLE_5_OUTER_RIGHT ) );
}

/****************************** END OF FILE *******************************************************/
