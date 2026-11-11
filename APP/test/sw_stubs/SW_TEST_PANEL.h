/*! \file
*               Author: mstewart
*   \brief      Software tests - the HUB's panel as a user works it
*
*   \details    The three push buttons are pins of the virtual STM32 and the knob is the count of TIM4, which the firmware's
*               encoder reads. What the user sees is the OLED, the TMD_SH1106 model on I2C1. A test presses, turns, and looks at the
*               screen: the assertions put what the screen shows in their failure message.
*/
#ifndef SW_TEST_PANEL_H
#define SW_TEST_PANEL_H

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                                    Data Types and Enums                                        **
***************************************************************************************************/
typedef enum
{
    SW_TEST_PANEL_SELECT = 0u,
    SW_TEST_PANEL_CONFIRM,
    SW_TEST_PANEL_BACK
} SW_TEST_PANEL_button_et;

/***************************************************************************************************
**                                    Function Prototypes                                        **
***************************************************************************************************/
void SW_TEST_PANEL_press( SW_TEST_PANEL_button_et button );                /* press and release: a short press */
void SW_TEST_PANEL_turn_knob_clockwise( void );                            /* one detent */
void SW_TEST_PANEL_move_the_cursor_to( const char* text_p );               /* turn the knob until the cursor of the list is on that row */
void SW_TEST_PANEL_assert_on_screen( const char* text_p );
void SW_TEST_PANEL_assert_not_on_screen( const char* text_p );
void SW_TEST_PANEL_assert_cursor_on( const char* text_p );                 /* the row is on the screen with the cursor in front of it */

#endif /* SW_TEST_PANEL_H */

/****************************** END OF FILE *******************************************************/
