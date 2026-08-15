/*! \file
*               Author: mstewart
*   \brief      MENU_NAV - this project's screens and how you move between them
*
*   \note       Project specific by design - see MENU_NAV.h. A different product on the same panel
*               forks this file; it does not configure it.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "MENU_NAV.h"
#include "VER.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define MENU_NAV_ITEM_COUNT( table )   ( (u8_t)( sizeof( table ) / sizeof( (table)[0] ) ) )

/*!< Working buffer for a line of formatted text. Only ever used inside a draw function, and
 *   HMI_SH1106_draw_text() renders into the framebuffer before returning, so a local of this size
 *   is all any screen needs - no static row buffers held between repaints. */
#define MENU_NAV_LINE_CHARS            ( HMI_SH1106_MAX_ITEM_CHARS + 1u )

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void        menu_nav_draw_list( const MENU_NAV_list_st* list_p );
STATIC void        menu_nav_handle_list( const MENU_NAV_screen_st* screen_p, HMI_SH1106_input_et input );
STATIC void        menu_nav_handle_static( const MENU_NAV_screen_st* screen_p, HMI_SH1106_input_et input );
STATIC void        menu_nav_move_cursor( const MENU_NAV_list_st* list_p, false_true_et down );
STATIC void        menu_nav_update_scroll( const MENU_NAV_list_st* list_p );
STATIC u8_t        menu_nav_visible_rows( const MENU_NAV_list_st* list_p );
STATIC const char* menu_nav_get_label( u8_t index );

STATIC void menu_nav_draw_home( void );
STATIC void menu_nav_handle_home( HMI_SH1106_input_et input );
STATIC void menu_nav_draw_status( void );
STATIC void menu_nav_draw_about( void );
STATIC void menu_nav_draw_not_implemented( void );
STATIC void menu_nav_draw_brightness( void );
STATIC void menu_nav_handle_brightness( HMI_SH1106_input_et input );
STATIC void menu_nav_enter_brightness( void );

/***************************************************************************************************
**                              Screen contents                                                   **
***************************************************************************************************/
/* ===== Main menu - the only list screen so far. Ten rows against seven item rows, so it scrolls
   and shows the up/down indicators. ===== */
STATIC const MENU_NAV_item_st menu_nav_main_items_s[] =
{
    { "Status",     MENU_NAV_SCREEN_STATUS          },
    { "Brightness", MENU_NAV_SCREEN_BRIGHTNESS      },
    { "WiFi",       MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "RF Link",    MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "Vehicle",    MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "CAN Bus",    MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "LEDs",       MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "Buzzer",     MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "Sensors",    MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "About",      MENU_NAV_SCREEN_ABOUT           },
};

STATIC const MENU_NAV_list_st menu_nav_main_list_s =
{
    .title_p    = "MAIN MENU",
    .items_p    = menu_nav_main_items_s,
    .item_count = MENU_NAV_ITEM_COUNT( menu_nav_main_items_s ),
};

/* ===== Brightness - the value this screen edits, and what it reverts to if BACK cancels ===== */
STATIC u8_t menu_nav_brightness_pct_s   = 75u;
STATIC u8_t menu_nav_brightness_saved_s = 75u;

#define MENU_NAV_BRIGHTNESS_MIN         ( 5u )
#define MENU_NAV_BRIGHTNESS_MAX         ( 100u )
#define MENU_NAV_BRIGHTNESS_STEP        ( 5u )

/***************************************************************************************************
**                              Screen table                                                      **
**  One row per screen - see MENU_NAV_screen_st. Indexed by MENU_NAV_screen_et, so the designated **
**  initialisers below keep the table and the enum in step no matter what order they are written. **
***************************************************************************************************/
STATIC const MENU_NAV_screen_st menu_nav_screens_s[MENU_NAV_NUM_SCREENS] =
{
    [MENU_NAV_SCREEN_HOME] =
    {
        .draw_func_p   = menu_nav_draw_home,
        .handle_func_p = menu_nav_handle_home,          /* SELECT/CONFIRM open the main menu */
        .back_screen   = MENU_NAV_SCREEN_HOME,          /* Already home - BACK has nowhere to go */
    },

    [MENU_NAV_SCREEN_MAIN_MENU] =
    {
        .list_p       = &menu_nav_main_list_s,
        .back_screen  = MENU_NAV_SCREEN_HOME,
    },

    [MENU_NAV_SCREEN_STATUS] =
    {
        .draw_func_p  = menu_nav_draw_status,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_BRIGHTNESS] =
    {
        .draw_func_p     = menu_nav_draw_brightness,
        .handle_func_p   = menu_nav_handle_brightness,   /* The knob edits rather than navigates */
        .on_enter_func_p = menu_nav_enter_brightness,    /* Remember what to revert to */
        .back_screen     = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_ABOUT] =
    {
        .draw_func_p  = menu_nav_draw_about,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_NOT_IMPLEMENTED] =
    {
        .draw_func_p  = menu_nav_draw_not_implemented,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },
};

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC MENU_NAV_screen_et menu_nav_screen_s = MENU_NAV_SCREEN_HOME;

/*!
 * \brief Where the cursor sits on each list screen
 *
 * One entry per screen rather than one shared pair, so leaving a list and coming back lands on the
 * row the user left instead of jumping to the top. Unused for free-form screens - a handful of
 * spare bytes is a fair price for indexing it directly by screen and never having to map.
 */
typedef struct
{
    u8_t cursor;    /*!< Item index the marker is on */
    u8_t scroll;    /*!< Item index drawn on the first item row */
} menu_nav_pos_st;

STATIC menu_nav_pos_st menu_nav_pos_s[MENU_NAV_NUM_SCREENS];

/* The list currently being drawn. Set immediately before HMI_SH1106_draw_list() and read only by
   menu_nav_get_label(), which the panel calls back into for each visible row - the panel is
   handed a getter rather than an array so this module never has to flatten its typed rows into a
   parallel array of strings just to have them drawn. */
STATIC const MENU_NAV_list_st* menu_nav_active_list_p = NULL_P;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Select the starting screen
*
*   \author        MS
*
*   \return        none
*
*   \note          Goes through MENU_NAV_goto() rather than assigning directly, so arriving at the
*                  first screen is not a special case - it runs that screen's on_enter_func_p the
*                  same way every later arrival does.
*
***************************************************************************************************/
void MENU_NAV_init( void )
{
    MENU_NAV_goto( MENU_NAV_SCREEN_HOME );
}

/*!
****************************************************************************************************
*
*   \brief         Make a screen the current one
*
*   \author        MS
*
*   \param         screen - Where to go. Ignored if out of range.
*
*   \return        none
*
*   \note          The whole of navigation. Going to the screen already showing is not a special
*                  case - it re-runs on_enter_func_p and repaints, which is what re-entering a
*                  screen should do.
*
***************************************************************************************************/
void MENU_NAV_goto( MENU_NAV_screen_et screen )
{
    if( screen < MENU_NAV_NUM_SCREENS )
    {
        menu_nav_screen_s = screen;

        if( menu_nav_screens_s[screen].on_enter_func_p != NULL_P )
        {
            menu_nav_screens_s[screen].on_enter_func_p();
        }

        HMI_SH1106_request_redraw();
    }
}

/*!
****************************************************************************************************
*
*   \brief         Paint the current screen
*
*   \author        MS
*
*   \return        none
*
*   \note          Called by HMI_SH1106 when a repaint is due, with the framebuffer already
*                  cleared.
*
***************************************************************************************************/
void MENU_NAV_draw( void )
{
    const MENU_NAV_screen_st* screen_p = &menu_nav_screens_s[menu_nav_screen_s];

    if( screen_p->list_p != NULL_P )
    {
        menu_nav_draw_list( screen_p->list_p );
    }
    else if( screen_p->draw_func_p != NULL_P )
    {
        screen_p->draw_func_p();
    }
    else
    {
        /* Screen declares neither - it is simply blank */
    }
}

/*!
****************************************************************************************************
*
*   \brief         Act on an input
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          A long BACK press is dealt with here rather than passed on, so no screen can
*                  swallow it - it is the one gesture guaranteed to get the user out of anywhere.
*
***************************************************************************************************/
void MENU_NAV_on_input( HMI_SH1106_input_et input )
{
    const MENU_NAV_screen_st* screen_p = &menu_nav_screens_s[menu_nav_screen_s];

    if( input == HMI_SH1106_INPUT_BACK_LONG )
    {
        MENU_NAV_goto( MENU_NAV_SCREEN_HOME );
    }
    else if( screen_p->handle_func_p != NULL_P )
    {
        screen_p->handle_func_p( input );
    }
    else if( screen_p->list_p != NULL_P )
    {
        menu_nav_handle_list( screen_p, input );
    }
    else
    {
        menu_nav_handle_static( screen_p, input );
    }
}

/***************************************************************************************************
**                              Private Functions - list screens                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Item rows a list has, once any title has taken the top row
*
*   \author        MS
*
*   \return        Number of items that fit on screen at once
*
***************************************************************************************************/
STATIC u8_t menu_nav_visible_rows( const MENU_NAV_list_st* list_p )
{
    return( ( list_p->title_p != NULL_P ) ? (u8_t)( HMI_SH1106_MAX_VISIBLE_LINES - 1u )
                                          : (u8_t)( HMI_SH1106_MAX_VISIBLE_LINES ) );
}

/*!
****************************************************************************************************
*
*   \brief         Slide the visible window so the cursor stays on screen
*
*   \author        MS
*
*   \return        none
*
*   \note          Minimal scrolling - the window only moves when the cursor would otherwise fall
*                  outside it, so a list that fits on screen never scrolls at all.
*
***************************************************************************************************/
STATIC void menu_nav_update_scroll( const MENU_NAV_list_st* list_p )
{
    menu_nav_pos_st* pos_p   = &menu_nav_pos_s[menu_nav_screen_s];
    u8_t             visible = menu_nav_visible_rows( list_p );

    if( pos_p->cursor < pos_p->scroll )
    {
        pos_p->scroll = pos_p->cursor;
    }
    else if( pos_p->cursor >= (u8_t)( pos_p->scroll + visible ) )
    {
        pos_p->scroll = (u8_t)( ( pos_p->cursor - visible ) + 1u );
    }
    else
    {
        /* Cursor already inside the window - leave it alone */
    }
}

/*!
****************************************************************************************************
*
*   \brief         Move the cursor one item, wrapping at the ends
*
*   \author        MS
*
*   \param         down - TRUE towards the last item, FALSE towards the first
*
*   \return        none
*
*   \note          Wrapping suits a knob with no end stops of its own - keep turning and the list
*                  comes back round rather than silently doing nothing.
*
***************************************************************************************************/
STATIC void menu_nav_move_cursor( const MENU_NAV_list_st* list_p, false_true_et down )
{
    if( list_p->item_count != 0u )
    {
        menu_nav_pos_st* pos_p     = &menu_nav_pos_s[menu_nav_screen_s];
        u8_t             last_item = (u8_t)( list_p->item_count - 1u );

        if( down == TRUE )
        {
            pos_p->cursor = ( pos_p->cursor >= last_item ) ? 0u : (u8_t)( pos_p->cursor + 1u );
        }
        else
        {
            pos_p->cursor = ( pos_p->cursor == 0u ) ? last_item : (u8_t)( pos_p->cursor - 1u );
        }

        menu_nav_update_scroll( list_p );
        HMI_SH1106_request_redraw();
    }
}

/*!
****************************************************************************************************
*
*   \brief         Label for one item - the getter HMI_SH1106_draw_list() calls back into
*
*   \author        MS
*
*   \return        Row text, or NULL_P if the index is out of range or no list is being drawn
*
***************************************************************************************************/
STATIC const char* menu_nav_get_label( u8_t index )
{
    const char* label_p = NULL_P;

    if( ( menu_nav_active_list_p != NULL_P ) && ( index < menu_nav_active_list_p->item_count ) )
    {
        label_p = menu_nav_active_list_p->items_p[index].label_p;
    }

    return( label_p );
}

/*!
****************************************************************************************************
*
*   \brief         Paint a list screen
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_list( const MENU_NAV_list_st* list_p )
{
    const menu_nav_pos_st* pos_p = &menu_nav_pos_s[menu_nav_screen_s];
    HMI_SH1106_list_st     draw_list;

    menu_nav_active_list_p = list_p;

    draw_list.title_p          = list_p->title_p;
    draw_list.item_count       = list_p->item_count;
    draw_list.first_item       = pos_p->scroll;
    draw_list.cursor           = pos_p->cursor;
    draw_list.get_label_func_p = menu_nav_get_label;

    HMI_SH1106_draw_list( &draw_list );
}

/*!
****************************************************************************************************
*
*   \brief         Default input handling for a list screen
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_handle_list( const MENU_NAV_screen_st* screen_p, HMI_SH1106_input_et input )
{
    const MENU_NAV_list_st* list_p = screen_p->list_p;
    const menu_nav_pos_st*  pos_p  = &menu_nav_pos_s[menu_nav_screen_s];

    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
            menu_nav_move_cursor( list_p, TRUE );
        break;

        case HMI_SH1106_INPUT_CCW:
            menu_nav_move_cursor( list_p, FALSE );
        break;

        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
            if( pos_p->cursor < list_p->item_count )
            {
                MENU_NAV_goto( list_p->items_p[pos_p->cursor].target );
            }
        break;

        case HMI_SH1106_INPUT_BACK:
            MENU_NAV_goto( screen_p->back_screen );
        break;

        default:
            /* Long select/confirm mean nothing on a list */
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Default input handling for a free-form screen
*
*   \author        MS
*
*   \return        none
*
*   \note          Any of the three buttons dismisses it. A read-only page has nothing to select,
*                  so making the user find the right button to leave would only be a puzzle.
*
***************************************************************************************************/
STATIC void menu_nav_handle_static( const MENU_NAV_screen_st* screen_p, HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
        case HMI_SH1106_INPUT_BACK:
            MENU_NAV_goto( screen_p->back_screen );
        break;

        default:
            /* The knob does nothing on a page with no cursor */
        break;
    }
}

/***************************************************************************************************
**                              Private Functions - the screens themselves                        **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Home - the root screen
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_home( void )
{
    HMI_SH1106_draw_text( 1u, 0u, "HUB" );
    HMI_SH1106_draw_text( 3u, 0u, "Press CONFIRM" );
    HMI_SH1106_draw_text( 4u, 0u, "for the menu" );
}

/*!
****************************************************************************************************
*
*   \brief         Home - SELECT/CONFIRM open the main menu
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          Needed because HOME is its own back_screen - without this, the default
*                  free-form-screen behaviour (send SELECT/CONFIRM/BACK to back_screen) would send
*                  every press straight back to HOME itself.
*
***************************************************************************************************/
STATIC void menu_nav_handle_home( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
            MENU_NAV_goto( MENU_NAV_SCREEN_MAIN_MENU );
        break;

        default:
            /* BACK and the knob do nothing at the true root */
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Status - live input counters straight off the panel
*
*   \author        MS
*
*   \return        none
*
*   \note          Repainted by the panel's own refresh timer, so these keep counting up while the
*                  screen is open rather than freezing at whatever they were on the way in.
*
***************************************************************************************************/
STATIC void menu_nav_draw_status( void )
{
    HMI_SH1106_stats_st stats;
    char                line[MENU_NAV_LINE_CHARS];

    HMI_SH1106_get_stats( &stats );

    HMI_SH1106_draw_text( 0u, 0u, "STATUS" );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "CW %lu  CCW %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CW],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CCW] );
    HMI_SH1106_draw_text( 2u, 0u, line );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Sel %lu  SelL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_SELECT],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_SELECT_LONG] );
    HMI_SH1106_draw_text( 3u, 0u, line );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Cfm %lu  CfmL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CONFIRM],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CONFIRM_LONG] );
    HMI_SH1106_draw_text( 4u, 0u, line );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Bck %lu  BckL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_BACK],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_BACK_LONG] );
    HMI_SH1106_draw_text( 5u, 0u, line );
}

/*!
****************************************************************************************************
*
*   \brief         About - real build metadata from the VER module
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_about( void )
{
    char  line[MENU_NAV_LINE_CHARS];
    u8_t  sw_ver[SW_VERSION_NUM_SIZE];
    u8_t  hw_ver[HW_VERSION_NUM_SIZE];

    HMI_SH1106_draw_text( 0u, 0u, "ABOUT" );

    VER_get_sw_version_num( sw_ver );
    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "SW v%u.%u.%u", sw_ver[0], sw_ver[1], sw_ver[2] );
    HMI_SH1106_draw_text( 2u, 0u, line );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Build %s", VER_get_sw_release_type() );
    HMI_SH1106_draw_text( 3u, 0u, line );

    /* BUILD_DATE/MONTH/YEAR straight from autoversion.h, not VER_get_build_date() - that one
       packs the u16_t year into a u8_t buffer slot and truncates it (VER.c), a pre-existing bug
       in that module, unrelated to this screen. */
    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Date %02u/%02u/%u",
                         (unsigned int)BUILD_DATE, (unsigned int)BUILD_MONTH, (unsigned int)BUILD_YEAR );
    HMI_SH1106_draw_text( 4u, 0u, line );

    VER_get_hw_version_num( hw_ver );
    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "HW v%u.%u", hw_ver[0], hw_ver[1] );
    HMI_SH1106_draw_text( 5u, 0u, line );
}

/*!
****************************************************************************************************
*
*   \brief         Placeholder for menu rows whose feature does not exist yet
*
*   \author        MS
*
*   \return        none
*
*   \note          One screen shared by every unimplemented row. When one of them becomes real it
*                  gets its own screen and its row's target changes to point at it.
*
***************************************************************************************************/
STATIC void menu_nav_draw_not_implemented( void )
{
    HMI_SH1106_draw_text( 2u, 0u, "Not implemented" );
    HMI_SH1106_draw_text( 3u, 0u, "yet" );
}

/*!
****************************************************************************************************
*
*   \brief         Brightness - capture the starting value so BACK can put it back
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_enter_brightness( void )
{
    menu_nav_brightness_saved_s = menu_nav_brightness_pct_s;
}

/*!
****************************************************************************************************
*
*   \brief         Brightness - show the value being edited
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_brightness( void )
{
    char line[MENU_NAV_LINE_CHARS];

    HMI_SH1106_draw_text( 0u, 0u, "BRIGHTNESS" );

    (void)STDC_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%u%%", (unsigned int)menu_nav_brightness_pct_s );
    HMI_SH1106_draw_text( 3u, 0u, line );

    HMI_SH1106_draw_text( 6u, 0u, "BACK cancels" );
}

/*!
****************************************************************************************************
*
*   \brief         Brightness - the knob edits the value instead of moving a cursor
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          The value is applied to the hardware on every step, so the screen dims and
*                  brightens as it is turned. Cancelling re-applies the value captured on entry,
*                  which is what makes BACK feel like an undo rather than just an exit.
*
***************************************************************************************************/
STATIC void menu_nav_handle_brightness( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
            if( menu_nav_brightness_pct_s <= (u8_t)( MENU_NAV_BRIGHTNESS_MAX - MENU_NAV_BRIGHTNESS_STEP ) )
            {
                menu_nav_brightness_pct_s += MENU_NAV_BRIGHTNESS_STEP;
                HMI_SH1106_set_brightness_pct( menu_nav_brightness_pct_s );
                HMI_SH1106_request_redraw();
            }
        break;

        case HMI_SH1106_INPUT_CCW:
            if( menu_nav_brightness_pct_s >= (u8_t)( MENU_NAV_BRIGHTNESS_MIN + MENU_NAV_BRIGHTNESS_STEP ) )
            {
                menu_nav_brightness_pct_s -= MENU_NAV_BRIGHTNESS_STEP;
                HMI_SH1106_set_brightness_pct( menu_nav_brightness_pct_s );
                HMI_SH1106_request_redraw();
            }
        break;

        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
            /* Keep it - the value is already live, there is nothing to commit */
            MENU_NAV_goto( menu_nav_screens_s[MENU_NAV_SCREEN_BRIGHTNESS].back_screen );
        break;

        case HMI_SH1106_INPUT_BACK:
            menu_nav_brightness_pct_s = menu_nav_brightness_saved_s;
            HMI_SH1106_set_brightness_pct( menu_nav_brightness_pct_s );
            MENU_NAV_goto( menu_nav_screens_s[MENU_NAV_SCREEN_BRIGHTNESS].back_screen );
        break;

        default:
            /* Long presses mean nothing while editing */
        break;
    }
}

/****************************** END OF FILE *******************************************************/
