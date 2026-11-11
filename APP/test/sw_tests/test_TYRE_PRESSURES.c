/*! \file
*               Author: mstewart
*   \brief      Tyre pressures - software test
*
*   \details    DATASET0 as BM leaves it (SW_TEST_STUBS.c: two makes, three models, four year ranges, and the valid bit in
*               the handoff RAM) read by TYRE_DATA, and the menu that lists it on the panel's OLED. A test drives the panel as a
*               user does (SW_TEST_PANEL.h) and looks at the display. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "TYRE_DATA.h"

#include "TMD_INIT.h"
#include "TMD_MBA.h"
#include "TMD_TIME.h"

#include "SW_TEST_PANEL.h"
#include "SW_TEST_STUBS.h"

#define TEST_BOOT_SETTLE_MS     ( 500u )    /* the first screen is drawn */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with the dataset the factory flashed in DATASET0
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
*   \brief         From the home screen to the list of makes: SELECT opens the main menu, the knob goes to Tyre Pressures, SELECT
*                  opens it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_open_the_tyre_pressures_menu( void )
{
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_assert_on_screen( "MAIN MENU" );

    SW_TEST_PANEL_move_the_cursor_to( "Tyre Pressures" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the dataset BM confirmed is the table TYRE_DATA reads after the start: the real DATASET0 symbols, the
*                  handoff RAM and the checks of TYRE_DATA_init()
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_dataset_bm_confirmed_is_read_at_the_start( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 2u, TYRE_DATA_get_make_count() );
    TEST_ASSERT_EQUAL_UINT8( 2u, TYRE_DATA_get_model_count( 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, TYRE_DATA_get_model_count( 1u ) );

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 1u, name, (u8_t)sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "BMW", name );
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_model_name( 0u, 1u, name, (u8_t)sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "A4", name );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a blank DATASET0 reads as empty: BM has no dataset to confirm, the table is not used and the menu says there is no data
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_blank_dataset_leaves_the_tyre_menu_empty( void )
{
    TMD_MBA_init();    /* DATASET0 is blank: BM has nothing to confirm */
    TMD_INIT_restart_ecu( NON_VIRGIN );
    TMD_TIME_advance_time_ms( TEST_BOOT_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_make_count() );

    test_open_the_tyre_pressures_menu();

    SW_TEST_PANEL_assert_on_screen( "No tyre data" );
    SW_TEST_PANEL_assert_not_on_screen( "Audi" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the menu lists the makes of the dataset
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_tyre_menu_lists_the_makes_of_the_dataset( void )
{
    test_open_the_tyre_pressures_menu();

    SW_TEST_PANEL_assert_on_screen( "Audi" );
    SW_TEST_PANEL_assert_on_screen( "BMW" );
    SW_TEST_PANEL_assert_not_on_screen( "No tyre data" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify choosing a make lists the models of that make only
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_choosing_a_make_lists_the_models_of_that_make( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "Audi" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );

    SW_TEST_PANEL_assert_on_screen( "A3" );
    SW_TEST_PANEL_assert_on_screen( "A4" );
    SW_TEST_PANEL_assert_not_on_screen( "M3" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the knob chooses another make, whose models are then the ones listed
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_knob_chooses_the_other_make( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "BMW" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );

    SW_TEST_PANEL_assert_on_screen( "M3" );
    SW_TEST_PANEL_assert_not_on_screen( "A3" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a model lists its year ranges
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_model_lists_its_year_ranges( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "Audi" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_move_the_cursor_to( "A3" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );

    SW_TEST_PANEL_assert_on_screen( "2003 - 2012" );
    SW_TEST_PANEL_assert_on_screen( "2013 - 2020" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the last screen shows the car chosen and the front and rear pressures of its year range, in bar
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_year_range_shows_the_pressures_of_the_car( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "Audi" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_move_the_cursor_to( "A3" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_move_the_cursor_to( "2013 - 2020" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );

    SW_TEST_PANEL_assert_on_screen( "TYRE PRESSURES" );
    SW_TEST_PANEL_assert_on_screen( "Audi" );
    SW_TEST_PANEL_assert_on_screen( "A3" );
    SW_TEST_PANEL_assert_on_screen( "2013 - 2020" );
    SW_TEST_PANEL_assert_on_screen( "Front 2.4 bar 35 psi" );    /* 34.8 psi */
    SW_TEST_PANEL_assert_on_screen( "Rear  2.3 bar 33 psi" );    /* 33.4 psi, the label padded to the width of Front */
}

/*!
****************************************************************************************************
*
*   \brief         Verify BACK goes up one level: from the models to the makes
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_back_returns_from_the_models_to_the_makes( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "Audi" );
    SW_TEST_PANEL_press( SW_TEST_PANEL_SELECT );
    SW_TEST_PANEL_assert_on_screen( "A3" );

    SW_TEST_PANEL_press( SW_TEST_PANEL_BACK );

    SW_TEST_PANEL_assert_on_screen( "BMW" );
    SW_TEST_PANEL_assert_not_on_screen( "A3" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a restart puts the lists back at the top: the cursor was on BMW, and after the restart the make list
*                  opens on Audi
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_restart_puts_the_tyre_lists_back_at_the_top( void )
{
    test_open_the_tyre_pressures_menu();
    SW_TEST_PANEL_move_the_cursor_to( "BMW" );

    TMD_INIT_restart_ecu( NON_VIRGIN );
    TMD_TIME_advance_time_ms( TEST_BOOT_SETTLE_MS );
    test_open_the_tyre_pressures_menu();

    SW_TEST_PANEL_assert_cursor_on( "Audi" );
}

/****************************** END OF FILE *******************************************************/
