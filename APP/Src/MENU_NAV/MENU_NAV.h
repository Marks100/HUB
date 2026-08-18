/*! \file
*               Author: mstewart
*   \brief      MENU_NAV - this project's screens and how you move between them
*
*   \details    HMI_SH1106 turns hardware into input events and draws what it is told to draw.
*               This module is everything in between: which screen is showing, what is on it, and
*               where each input takes you.
*
*               A FLAT GRAPH, NOT A STACK
*               =========================
*               There is no navigation stack. Every screen names its own destinations - the rows
*               of a list name the screen each one opens, and the screen table names where BACK
*               goes from there. Navigation is one assignment (see MENU_NAV_goto()), which is why
*               there is no depth counter, no push/pop, and no way for a parent and child to
*               disagree about where they are.
*
*               The cost of that trade is that a screen reachable from two places has one BACK
*               destination, not two. The escape hatch is a long BACK press, which always returns
*               to MENU_NAV_SCREEN_HOME from anywhere and cannot be overridden by a screen.
*
*               SCREENS, NOT MENUS
*               =========================
*               A screen is free-form by default - it gets a draw function and paints whatever it
*               likes. A *list* is the special case: fill in list_p instead and the cursor, the
*               scrolling, the marker and the up/down indicators all come for free, with no draw
*               or handle function needed at all.
*
*               ADDING A SCREEN
*               =========================
*               Add an enumerator to MENU_NAV_screen_et, add its row to menu_nav_screens_s in the
*               .c, done. Everything a screen needs is in that one row, so there is no second or
*               third place to update and nothing to keep in step.
*
*   \note       Project specific by design. HMI_SH1106 is shared - any board wired to that panel
*               reuses it unchanged - but what screens exist and what they say is a product
*               decision, so it lives here in APP rather than in xCOMMON_MODULES.
*/
#ifndef MENU_NAV_H
#define MENU_NAV_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "HMI_SH1106.h"

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
/*!
 * \brief Every screen this product has
 *
 * MENU_NAV_SCREEN_HOME is the root - a long BACK press returns here from anywhere.
 */
typedef enum
{
    MENU_NAV_SCREEN_HOME = 0u,
    MENU_NAV_SCREEN_MAIN_MENU,
    MENU_NAV_SCREEN_STATUS,
    MENU_NAV_SCREEN_BRIGHTNESS,
    MENU_NAV_SCREEN_FAN_SPEED,
    MENU_NAV_SCREEN_BUZZER,
    MENU_NAV_SCREEN_WIFI,
    MENU_NAV_SCREEN_TB,
    MENU_NAV_SCREEN_SENSORS,
    MENU_NAV_SCREEN_VEHICLE,
    MENU_NAV_SCREEN_ABOUT,
    MENU_NAV_SCREEN_NOT_IMPLEMENTED,
    MENU_NAV_NUM_SCREENS                /*!< Not a screen - array bound, keep last */
} MENU_NAV_screen_et;

/*!
 * \brief One row of a list screen
 *
 * Rows navigate; they do not run actions. Something that needs to *happen* becomes a screen that
 * does it on entry (see on_enter_func_p), which keeps every row the same shape and means the user
 * always ends up somewhere that can tell them what happened.
 */
typedef struct
{
    const char*        label_p;
    MENU_NAV_screen_et target;      /*!< Screen SELECT/CONFIRM opens from this row */
} MENU_NAV_item_st;

/*!
 * \brief A list screen's contents - caller owned, expected to be a static const table
 */
typedef struct
{
    const char*             title_p;      /*!< Heading, or NULL_P for one more item row */
    const MENU_NAV_item_st* items_p;
    u8_t                    item_count;
} MENU_NAV_list_st;

/*!
 * \brief One screen
 *
 * Fill in either list_p (a list screen - nothing else needed) or draw_func_p (a free-form screen).
 * handle_func_p is only for a screen that wants inputs to mean something other than the default
 * for its kind.
 */
typedef struct
{
    /*!< A list screen's contents. NULL_P for a free-form screen, in which case draw_func_p is
     *   what paints it. */
    const MENU_NAV_list_st* list_p;

    /*!< Paints a free-form screen. Ignored when list_p is set. The framebuffer is already cleared,
     *   so this only has to draw what should be there. */
    void (*draw_func_p)( void );

    /*!< Optional. Takes over input handling for this screen, replacing the default behaviour of
     *   its kind (below).
     *
     *   Default for a list screen: the encoder moves the cursor, SELECT/CONFIRM open the row's
     *   target, BACK goes to back_screen.
     *   Default for a free-form screen: SELECT/CONFIRM/BACK all go to back_screen.
     *
     *   HMI_SH1106_INPUT_BACK_LONG is a special case: this still sees it first (so a screen with
     *   state to unwind - a value mid-edit - gets the chance to revert it), but the jump to
     *   MENU_NAV_SCREEN_HOME that follows happens unconditionally and cannot be redirected or
     *   suppressed from here, unlike every other input. */
    void (*handle_func_p)( HMI_SH1106_input_et input );

    /*!< Optional. Called as this screen becomes the current one - for a screen that has to
     *   capture state on the way in (a value editor remembering what to revert to) or do a job
     *   on arrival. */
    void (*on_enter_func_p)( void );

    /*!< Where BACK goes from here. A screen that is its own back_screen simply absorbs BACK. */
    MENU_NAV_screen_et back_screen;
} MENU_NAV_screen_st;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/*!< Selects the starting screen. Call once, after HMI_SH1106_init(). */
void MENU_NAV_init( void );

/*!< Wire these two straight into HMI_SH1106_cfg_st's on_input_func_p and draw_func_p. */
void MENU_NAV_on_input( HMI_SH1106_input_et input );
void MENU_NAV_draw( void );

/*!< The entire navigation mechanism. Also public so the application can drive the display from
 *   outside the panel - jumping to a fault screen when something goes wrong, for instance. */
void MENU_NAV_goto( MENU_NAV_screen_et screen );

/*!< The Buzzer/Beep Time screens' current values. The board layer reads these before it beeps
 *   (see hmi_on_input() in INTEGRATION_STUBS.c) so button-press feedback stays board policy, not
 *   something this module has to know how to trigger. */
false_true_et MENU_NAV_buzzer_enabled( void );
u16_t         MENU_NAV_get_beep_duration_ms( void );

#endif /* MENU_NAV_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
