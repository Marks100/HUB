/*! \file
*               Author: mstewart
*   \brief      The firmware's RAM across a restart - software test
*
*   \details    On the target the startup code gives the firmware its RAM as the program left the factory at every start: the
*               initialised variables get their initial values and the rest is zero. TMD_STARTUP does the same for the host
*               library, so a restart in a test starts the firmware as the target does. What the firmware keeps across a
*               reset on the target (the shared RAM BM and APP hand over through) is kept here too. Every test case starts as a
*               virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "BOOT_HANDOFF.h"
#include "MENU_NAV.h"

#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_PANEL.h"
#include "SW_TEST_STUBS.h"

#define TEST_BOOT_SETTLE_MS     ( 500u )    /* the first screen is drawn */

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
    TMD_TIME_advance_time_ms( TEST_BOOT_SETTLE_MS );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Restart the ECU, keeping its flash, and let the first screen be drawn
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_restart( void )
{
    TMD_INIT_restart_ecu( NON_VIRGIN );
    TMD_TIME_advance_time_ms( TEST_BOOT_SETTLE_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a variable that starts at zero is zero after a restart: the fan speed, which nothing in the start-up sets
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_restart_zeroes_a_variable_the_startup_code_zeroes( void )
{
    MENU_NAV_set_fan_speed_pct( 55u );
    TEST_ASSERT_EQUAL_UINT8( 55u, MENU_NAV_get_fan_speed_pct() );

    test_restart();

    TEST_ASSERT_EQUAL_UINT8( 0u, MENU_NAV_get_fan_speed_pct() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a variable that starts with a value has it again after a restart: the brightness starts at 75 %, the user
*                  turns it up to 85 %, and after the restart the screen shows 75 % again
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_restart_gives_an_initialised_variable_its_initial_value( void )
{
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );    /* home to the main menu */
    SW_TEST_PANEL_move_the_cursor_to( "Brightness" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_assert_on_screen( "75%" );

    SW_TEST_PANEL_turn_knob_clockwise();
    SW_TEST_PANEL_turn_knob_clockwise();
    SW_TEST_PANEL_assert_on_screen( "85%" );

    test_restart();

    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_move_the_cursor_to( "Brightness" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_assert_on_screen( "75%" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify what the firmware keeps across a reset on the target survives a restart here too: the request BM and APP
*                  hand over in the shared RAM, which the startup code leaves alone
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_shared_ram_survives_a_restart( void )
{
    BOOT_HANDOFF_set_fbl_request( TRUE );

    test_restart();

    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );
}

/****************************** END OF FILE *******************************************************/
