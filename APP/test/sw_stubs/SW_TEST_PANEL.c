/*! \file
*               Author: mstewart
*   \brief      Software tests - the HUB's panel as a user works it - see SW_TEST_PANEL.h
*/

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "unity.h"

#include "STDC.h"
#include "HAL_config.h"

#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"

#include "TMD_SH1106.h"
#include "TMD_TIME.h"

#include "SW_TEST_PANEL.h"

/***************************************************************************************************
**                                       Definitions                                              **
***************************************************************************************************/
#define SW_TEST_PANEL_PRESS_MS          ( 100u )    /* a short press, well under a long one */
#define SW_TEST_PANEL_DETENT_COUNTS     ( 4u )      /* TIM4 decodes x4, hmi_sh1106_cfg_s.enc_counts_per_detent */
#define SW_TEST_PANEL_DETENT_SETTLE_MS  ( 100u )
#define SW_TEST_PANEL_MAX_DETENTS       ( 40u )     /* more than any list of the menu is long */

#define SW_TEST_PANEL_SCREEN_ROWS       ( 8u )
#define SW_TEST_PANEL_SCREEN_ROW_LEN    ( 24u )

/***************************************************************************************************
**                                    Private Variables                                           **
***************************************************************************************************/
static char sw_test_panel_message_s[( SW_TEST_PANEL_SCREEN_ROWS * ( SW_TEST_PANEL_SCREEN_ROW_LEN + 1u ) ) + 48u];    /* what the screen showed, for a failed assertion */

/***************************************************************************************************
**                                  Private Function Prototypes                                   **
***************************************************************************************************/
static const char* sw_test_panel_screen_message( const char* what_p );

/***************************************************************************************************
**                                  Public Functions                                              **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Press and release a push button of the panel
*
*   \author        mstewart
*
*   \param[in]     button    Which one
*
*   \return        none
*
*   \note          The buttons pull their pin LOW when pressed
*
*****************************************************************************************************/
void SW_TEST_PANEL_press( SW_TEST_PANEL_button_et button )
{
    GPIO_TypeDef* port_p = PANEL_SELECT_BTN_PORT;
    u16_t         pin    = PANEL_SELECT_BTN_PIN;

    if( button == SW_TEST_PANEL_CONFIRM )
    {
        port_p = PANEL_CONFIRM_BTN_PORT;
        pin    = PANEL_CONFIRM_BTN_PIN;
    }
    else if( button == SW_TEST_PANEL_BACK )
    {
        port_p = PANEL_BACK_BTN_PORT;
        pin    = PANEL_BACK_BTN_PIN;
    }
    else
    {
        /* SELECT, as above */
    }

    GPIO_SIM_set_input( port_p, pin, LOW );
    TMD_TIME_advance_time_ms( SW_TEST_PANEL_PRESS_MS );
    GPIO_SIM_set_input( port_p, pin, HIGH );
    TMD_TIME_advance_time_ms( SW_TEST_PANEL_PRESS_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Turn the knob a detent clockwise
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void SW_TEST_PANEL_turn_knob_clockwise( void )
{
    TIM_SIM_set_counter( TIM4, TIM_GetCounter( TIM4 ) + SW_TEST_PANEL_DETENT_COUNTS );
    TMD_TIME_advance_time_ms( SW_TEST_PANEL_DETENT_SETTLE_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Turn the knob until the cursor of the list is on a row
*
*   \author        mstewart
*
*   \param[in]     text_p    The row
*
*   \return        none
*
*   \note          Where the cursor starts is not the test's business: the list wraps round
*
*****************************************************************************************************/
void SW_TEST_PANEL_move_the_cursor_to( const char* text_p )
{
    u8_t detents = 0u;

    while( ( TMD_SH1106_find_marked_text( text_p, NULL_P ) == FALSE ) && ( detents < SW_TEST_PANEL_MAX_DETENTS ) )
    {
        SW_TEST_PANEL_turn_knob_clockwise();
        detents++;
    }

    SW_TEST_PANEL_assert_cursor_on( text_p );
}

/*!
****************************************************************************************************
*
*   \brief         Assert a string is on the screen, and show the screen if it is not
*
*   \author        mstewart
*
*   \param[in]     text_p    The string
*
*   \return        none
*
*****************************************************************************************************/
void SW_TEST_PANEL_assert_on_screen( const char* text_p )
{
    TEST_ASSERT_TRUE_MESSAGE( TMD_SH1106_find_text( text_p, NULL_P, NULL_P ), sw_test_panel_screen_message( text_p ) );
}

/*!
****************************************************************************************************
*
*   \brief         Assert a string is not on the screen, and show the screen if it is
*
*   \author        mstewart
*
*   \param[in]     text_p    The string
*
*   \return        none
*
*****************************************************************************************************/
void SW_TEST_PANEL_assert_not_on_screen( const char* text_p )
{
    TEST_ASSERT_FALSE_MESSAGE( TMD_SH1106_find_text( text_p, NULL_P, NULL_P ), sw_test_panel_screen_message( text_p ) );
}

/*!
****************************************************************************************************
*
*   \brief         Assert a row is on the screen with the cursor of the list in front of it, and show the screen if it is not
*
*   \author        mstewart
*
*   \param[in]     text_p    The row
*
*   \return        none
*
*****************************************************************************************************/
void SW_TEST_PANEL_assert_cursor_on( const char* text_p )
{
    TEST_ASSERT_TRUE_MESSAGE( TMD_SH1106_find_marked_text( text_p, NULL_P ), sw_test_panel_screen_message( text_p ) );
}

/***************************************************************************************************
**                                  Private Functions                                             **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         The message of an assertion about the screen: what was looked for, and what the screen shows
*
*   \author        mstewart
*
*   \param[in]     what_p    What the test looked for
*
*   \return        The message, kept until the next call
*
*****************************************************************************************************/
static const char* sw_test_panel_screen_message( const char* what_p )
{
    char  row[SW_TEST_PANEL_SCREEN_ROW_LEN + 1u];
    u32_t length = 0u;
    u8_t  page;

    length += (u32_t)snprintf( &sw_test_panel_message_s[length], sizeof( sw_test_panel_message_s ) - length, "%s, screen:", what_p );

    for( page = 0u; page < SW_TEST_PANEL_SCREEN_ROWS; page++ )
    {
        TMD_SH1106_read_page_text( page, row, (u16_t)sizeof( row ) );
        length += (u32_t)snprintf( &sw_test_panel_message_s[length], sizeof( sw_test_panel_message_s ) - length, "|%s", row );
    }

    return( sw_test_panel_message_s );
}

/****************************** END OF FILE *******************************************************/
