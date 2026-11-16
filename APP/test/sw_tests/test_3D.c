/*! \file
*               Author: mstewart
*   \brief      3D screen - software test
*
*   \details    The 3D entry of the main menu opens a wireframe cube that tumbles with the run time clock, and SELECT or CONFIRM swap
*               it for a pyramid and back. A test drives the panel as a user does (SW_TEST_STUBS.h) and looks at the pixels on the
*               OLED. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"

#include "TMD_INIT.h"
#include "TMD_PANEL.h"
#include "TMD_SH1106.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

/* The title takes page 0 from column 0, 60 columns for the longest ("3D PYRAMID"); the shape must stay clear of everything right of it */
#define TEST_TITLE_FREE_FROM_COLUMN     ( 64u )

/* A cube drawn in lines lights far more pixels than this, even edge on */
#define TEST_MIN_CUBE_PIXELS            ( 40u )

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
    TMD_TIME_advance_time_ms( SW_TEST_BOOT_SETTLE_MS );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         From the home screen to the 3D screen: SELECT opens the main menu, the knob goes to 3D, SELECT opens it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_open_the_3d_screen( void )
{
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "MAIN MENU" );

    TMD_PANEL_move_the_cursor_to( "3D" );
    TMD_PANEL_press( TMD_PANEL_SELECT );
}

/*!
****************************************************************************************************
*
*   \brief         Copy the pixels under the title row: pages 1 to 7, as the display RAM holds them
*
*   \author        mstewart
*
*   \param[out]    dest_p - TMD_SH1106_NUM_PAGES - 1 pages of TMD_SH1106_RAM_COLUMNS bytes
*
*   \return        none
*
*****************************************************************************************************/
static void test_read_drawing( u8_t* dest_p )
{
    u8_t page;

    for( page = 1u; page < TMD_SH1106_NUM_PAGES; page++ )
    {
        TMD_SH1106_read_page_bytes( page, &dest_p[(u16_t)( page - 1u ) * TMD_SH1106_RAM_COLUMNS], TMD_SH1106_RAM_COLUMNS );
    }
}

/*!
****************************************************************************************************
*
*   \brief         How many pixels are lit in a copy made by test_read_drawing()
*
*   \author        mstewart
*
*   \param[in]     drawing_p - The copy
*
*   \return        Lit pixels
*
*****************************************************************************************************/
static u16_t test_count_lit_pixels( const u8_t* drawing_p )
{
    u16_t count = 0u;
    u16_t i;
    u8_t  bit;

    for( i = 0u; i < ( ( TMD_SH1106_NUM_PAGES - 1u ) * TMD_SH1106_RAM_COLUMNS ); i++ )
    {
        for( bit = 0u; bit < 8u; bit++ )
        {
            count = (u16_t)( count + ( ( drawing_p[i] >> bit ) & 1u ) );
        }
    }

    return( count );
}

/*!
****************************************************************************************************
*
*   \brief         Let the shape turn for ten seconds, a quarter of a second at a time, and check the title row to the right of the
*                  title is blank every time
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_assert_the_title_row_stays_clear( void )
{
    u8_t page_0[TMD_SH1106_RAM_COLUMNS];
    u8_t step;
    u8_t column;

    for( step = 0u; step < 40u; step++ )
    {
        TMD_TIME_advance_time_ms( 250u );
        TMD_SH1106_read_page_bytes( 0u, page_0, (u16_t)sizeof( page_0 ) );

        for( column = TEST_TITLE_FREE_FROM_COLUMN; column < TMD_SH1106_RAM_COLUMNS; column++ )
        {
            TEST_ASSERT_EQUAL_HEX8( 0x00u, page_0[column] );
        }
    }
}

/*!
****************************************************************************************************
*
*   \brief         Verify the main menu has a 3D row that opens the screen, on the cube, and that the menu is not on the screen any more
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_main_menu_opens_the_3d_screen_on_the_cube( void )
{
    test_open_the_3d_screen();

    TMD_PANEL_assert_on_screen( "3D CUBE" );
    TMD_PANEL_assert_not_on_screen( "PYRAMID" );
    TMD_PANEL_assert_not_on_screen( "MAIN MENU" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify SELECT swaps the cube for the pyramid, and that a pyramid is drawn
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_select_swaps_the_cube_for_the_pyramid( void )
{
    u8_t drawing[( TMD_SH1106_NUM_PAGES - 1u ) * TMD_SH1106_RAM_COLUMNS];

    test_open_the_3d_screen();
    TMD_PANEL_press( TMD_PANEL_SELECT );

    TMD_PANEL_assert_on_screen( "3D PYRAMID" );
    TMD_PANEL_assert_not_on_screen( "3D CUBE" );

    test_read_drawing( drawing );
    TEST_ASSERT_TRUE( test_count_lit_pixels( drawing ) >= TEST_MIN_CUBE_PIXELS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify CONFIRM swaps the shape too, and the shapes go round: cube, pyramid, cube
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_shapes_swap_each_time_with_select_or_confirm( void )
{
    test_open_the_3d_screen();

    TMD_PANEL_press( TMD_PANEL_CONFIRM );
    TMD_PANEL_assert_on_screen( "3D PYRAMID" );

    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "3D CUBE" );

    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "3D PYRAMID" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the shape chosen is still there after leaving the screen and coming back
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_chosen_shape_is_kept_when_the_screen_is_left_and_opened_again( void )
{
    test_open_the_3d_screen();
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "3D PYRAMID" );

    TMD_PANEL_press( TMD_PANEL_BACK );
    TMD_PANEL_assert_on_screen( "MAIN MENU" );

    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "3D PYRAMID" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a restart puts the screen back on the cube
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_restart_puts_the_3d_screen_back_on_the_cube( void )
{
    test_open_the_3d_screen();
    TMD_PANEL_press( TMD_PANEL_SELECT );
    TMD_PANEL_assert_on_screen( "3D PYRAMID" );

    TMD_INIT_restart_ecu();
    TMD_TIME_advance_time_ms( SW_TEST_BOOT_SETTLE_MS );

    test_open_the_3d_screen();
    TMD_PANEL_assert_on_screen( "3D CUBE" );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the screen draws a shape, not just the title
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_3d_screen_draws_a_cube( void )
{
    u8_t drawing[( TMD_SH1106_NUM_PAGES - 1u ) * TMD_SH1106_RAM_COLUMNS];

    test_open_the_3d_screen();
    test_read_drawing( drawing );

    TEST_ASSERT_TRUE( test_count_lit_pixels( drawing ) >= TEST_MIN_CUBE_PIXELS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the cube stays out of the title row, however far round it has turned
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_cube_never_touches_the_title_row( void )
{
    test_open_the_3d_screen();

    test_assert_the_title_row_stays_clear();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the pyramid stays out of the title row, however far round it has turned
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_pyramid_never_touches_the_title_row( void )
{
    test_open_the_3d_screen();
    TMD_PANEL_press( TMD_PANEL_SELECT );

    test_assert_the_title_row_stays_clear();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the cube turns as the clock runs
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_cube_turns_with_the_clock( void )
{
    u8_t before[( TMD_SH1106_NUM_PAGES - 1u ) * TMD_SH1106_RAM_COLUMNS];
    u8_t after[( TMD_SH1106_NUM_PAGES - 1u ) * TMD_SH1106_RAM_COLUMNS];

    test_open_the_3d_screen();
    test_read_drawing( before );

    TMD_TIME_advance_time_ms( 1000u );
    test_read_drawing( after );

    TEST_ASSERT_TRUE( memcmp( before, after, sizeof( before ) ) != 0 );
    TEST_ASSERT_TRUE( test_count_lit_pixels( after ) >= TEST_MIN_CUBE_PIXELS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify BACK returns to the main menu with the cursor still on the 3D row
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_back_returns_from_the_3d_screen_to_the_main_menu( void )
{
    test_open_the_3d_screen();
    TMD_PANEL_press( TMD_PANEL_BACK );

    TMD_PANEL_assert_on_screen( "MAIN MENU" );
    TMD_PANEL_assert_cursor_on( "3D" );
}

/****************************** END OF FILE *******************************************************/
