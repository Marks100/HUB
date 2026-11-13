/*! \file
*               Author: mstewart
*   \brief      Tyre pressures - software test
*
*   \details    DATASET0 as the factory downloads it (two makes, three models, four year ranges) and BM leaves it, with the
*               valid bit in the handoff RAM, read by TYRE_DATA, and the menu that lists it on the panel's OLED. A test drives the panel as a
*               user does (SW_TEST_STUBS.h) and looks at the display. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "TYRE_DATA.h"

#include "TMD_INIT.h"
#include "TMD_PANEL.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

/* DATASET0's payload as the factory flashes it (the layout of TYRE_DATA.h, format 2): two makes, three models, four year ranges */
static const u8_t test_dataset0_c[] =
{
    'T', 'P', 2u, 2u, 3u, 0u, 4u, 0u,                                   /* header: magic, format, 2 makes, 3 models, 4 year ranges */
    2u, 1u,                                                             /* models per make: Audi 2, BMW 1 */
    2u, 1u, 1u,                                                         /* year ranges per model: A3 2, A4 1, M3 1 */
    'A', 'u', 'd', 'i', 0, 'B', 'M', 'W', 0,                            /* make names */
    'A', '3', 0, 'A', '4', 0, 'M', '3', 0,                              /* model names */
    103u, 112u, 23u, 22u,   113u, 120u, 24u, 23u,                       /* A3 2003 - 2012 and 2013 - 2020: front and rear in 0.1 bar */
    108u, 115u, 22u, 21u,                                               /* A4 2008 - 2015 */
    107u, 113u, 25u, 24u,                                               /* M3 2007 - 2013 */
    0xFFu                                                               /* blank, to make it an even number of bytes: flash is written in half-words */
};

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU whose DATASET0 the factory has downloaded: the dataset is put in the flash
*                  as the FBL's download leaves it, and the ECU starts again so that BM finds it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    memcpy( (void*)SW_TEST_DATASET0_PAYLOAD_ADDRESS, test_dataset0_c, sizeof( test_dataset0_c ) );    /* the download: not a 0x3D, which does not write there */

    TMD_INIT_restart_ecu();
    TMD_TIME_advance_time_ms( SW_TEST_BOOT_SETTLE_MS );    /* BM finds the dataset at the start of the ECU */

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
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "MAIN MENU" );

    TMD_PANEL_move_the_cursor_to( "Tyre Pressures" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
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
    TMD_INIT_init( &sw_test_init_cfg_g );    /* a new ECU, its DATASET0 blank: BM has nothing to confirm */
    TMD_TIME_advance_time_ms( SW_TEST_BOOT_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_make_count() );

    test_open_the_tyre_pressures_menu();

    TMD_PANEL_assert_on_screen( "No tyre data" );
    TMD_PANEL_assert_not_on_screen( "Audi" );
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

    TMD_PANEL_assert_on_screen( "Audi" );
    TMD_PANEL_assert_on_screen( "BMW" );
    TMD_PANEL_assert_not_on_screen( "No tyre data" );
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
    TMD_PANEL_move_the_cursor_to( "Audi" );
    TMD_PANEL_press( TMD_PANEL_SELECT );

    TMD_PANEL_assert_on_screen( "A3" );
    TMD_PANEL_assert_on_screen( "A4" );
    TMD_PANEL_assert_not_on_screen( "M3" );
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
    TMD_PANEL_move_the_cursor_to( "BMW" );
    TMD_PANEL_press( TMD_PANEL_SELECT );

    TMD_PANEL_assert_on_screen( "M3" );
    TMD_PANEL_assert_not_on_screen( "A3" );
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
    TMD_PANEL_move_the_cursor_to( "Audi" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_move_the_cursor_to( "A3" );
    TMD_PANEL_press( TMD_PANEL_SELECT );

    TMD_PANEL_assert_on_screen( "2003 - 2012" );
    TMD_PANEL_assert_on_screen( "2013 - 2020" );
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
    TMD_PANEL_move_the_cursor_to( "Audi" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_move_the_cursor_to( "A3" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_move_the_cursor_to( "2013 - 2020" );
    TMD_PANEL_press( TMD_PANEL_SELECT );

    TMD_PANEL_assert_on_screen( "TYRE PRESSURES" );
    TMD_PANEL_assert_on_screen( "Audi" );
    TMD_PANEL_assert_on_screen( "A3" );
    TMD_PANEL_assert_on_screen( "2013 - 2020" );
    TMD_PANEL_assert_on_screen( "Front 2.4 bar 35 psi" );    /* 34.8 psi */
    TMD_PANEL_assert_on_screen( "Rear  2.3 bar 33 psi" );    /* 33.4 psi, the label padded to the width of Front */
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
    TMD_PANEL_move_the_cursor_to( "Audi" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "A3" );

    TMD_PANEL_press( TMD_PANEL_BACK );

    TMD_PANEL_assert_on_screen( "BMW" );
    TMD_PANEL_assert_not_on_screen( "A3" );
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
    TMD_PANEL_move_the_cursor_to( "BMW" );

    TMD_INIT_restart_ecu();
    TMD_TIME_advance_time_ms( SW_TEST_BOOT_SETTLE_MS );
    test_open_the_tyre_pressures_menu();

    TMD_PANEL_assert_cursor_on( "Audi" );
}

/****************************** END OF FILE *******************************************************/
