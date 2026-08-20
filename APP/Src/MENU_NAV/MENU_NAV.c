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
#include "WIFI.h"
#include "RF_MGR.h"
#include "TIME_MGR.h"
#include "BUZZER.h"
#include "TB.h"
#include "PRESS_CONV.h"
#include "GFX.h"
#include "printf.h"
#include "SHARED_RAM.h"
#include "MCU_JUMP.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define MENU_NAV_ITEM_COUNT( table )   ( (u8_t)( sizeof( table ) / sizeof( (table)[0] ) ) )

/*!< Working buffer for a line of formatted text. Only ever used inside a draw function, and
 *   HMI_SH1106_draw_text() renders into the framebuffer before returning, so a local of this size
 *   is all any screen needs - no static row buffers held between repaints. */
#define MENU_NAV_LINE_CHARS            ( HMI_SH1106_MAX_ITEM_CHARS + 1u )

/***************************************************************************************************
**                              Data Types                                                        **
***************************************************************************************************/
/*!
 * \brief One tire's reading for the Vehicle screen
 *
 * Placeholder data - there is no tire pressure sensor anywhere in this codebase yet (RF_MGR's
 * sensor DB has no pressure field and no FL/FR/RL/RR mapping). Replace menu_nav_tire_fl_s etc.
 * with real readings once that data source exists.
 */
typedef struct
{
    s8_t temp_c;
    u8_t pressure_psi;
} menu_nav_tire_reading_st;

/*!
 * \brief Config for a screen that edits a single 0-100 value with the knob (Brightness, Fan Speed)
 *
 * Brightness and Fan Speed are the same screen shape - a title, a live "%u%%" readout, min/max/step
 * clamping on CW/CCW, and BACK/BACK_LONG reverting to whatever was captured on entry - so they share
 * one draw/handle/enter implementation parameterised by this, rather than each keeping its own near-
 * identical copy. apply_func_p is NULL_P for a screen with nothing to push to hardware (Fan Speed -
 * there is no PWM driver wired in yet); screen is looked up in menu_nav_screens_s for back_screen
 * rather than duplicating it here, so there is still only one place that says where BACK goes.
 */
typedef struct
{
    const char*        title_p;
    u8_t*               value_p;
    u8_t*               saved_p;
    u8_t                min;
    u8_t                max;
    u8_t                step;
    void              (*apply_func_p)( u8_t pct );
    MENU_NAV_screen_et  screen;
} menu_nav_pct_editor_st;

/*!
 * \brief The part of a gauge that isn't drawing - a live value plus how the knob moves it
 *
 * Shared by every gauge shape (needle, vertical bar, whatever comes next) via
 * menu_nav_value_edit_handle() - CW/CCW clamp-and-step this exactly the same way regardless of how
 * the value ends up on screen, so a new gauge shape reuses this rather than re-writing the same
 * clamp/step/navigate switch. Unlike menu_nav_pct_editor_st there is no saved_p/apply_func_p - a
 * gauge's value is live and immediate, nothing to revert if BACK is pressed.
 */
typedef struct
{
    u16_t*              value_p;    /*!< Live value - drawn and knob-edited in place */
    u16_t                min;
    u16_t                max;
    u16_t                knob_step;  /*!< How far one detent moves the value; clamped, not wrapped */
    MENU_NAV_screen_et   screen;    /*!< Looked up in menu_nav_screens_s for back_screen */
} menu_nav_value_edit_st;

/*!
 * \brief Config for a needle-and-bar gauge screen (Rev Counter, and any future one)
 *
 * Same idea as menu_nav_pct_editor_st - one draw implementation (menu_nav_gauge_draw()) parameterised
 * by this, rather than a copy per gauge. The shapes themselves (needle, ticks, bar) are
 * GFX_draw_gauge()'s job, not this module's - gfx_cfg is handed straight to it. What lives here
 * instead is everything GFX can't own: the title and digital readout (GFX has no text/font
 * capability - see HMI_SH1106_draw_text()), and the "what does the RPM needle table actually mean"
 * label text per tick (label_table_p, parallel to gfx_cfg.tick_table_p).
 */
typedef struct
{
    u8_t        col;    /*!< See HMI_SH1106_draw_text() - pixel column */
    u8_t        row;    /*!< See HMI_SH1106_draw_text() - 8px page, not a pixel row */
    const char* text_p;
} menu_nav_gauge_label_st;

typedef struct
{
    const char*                    title_p;
    menu_nav_value_edit_st         edit;          /*!< Value + knob feel - see menu_nav_value_edit_st.
                                                         edit.knob_step must be a whole multiple of
                                                         gfx_cfg.value_step */
    GFX_gauge_cfg_st                gfx_cfg;       /*!< Shapes - see GFX_draw_gauge() */
    const menu_nav_gauge_label_st* label_table_p;  /*!< gfx_cfg.num_ticks entries, parallel to
                                                         gfx_cfg.tick_table_p */
} menu_nav_gauge_cfg_st;

/*!
 * \brief Config for a vertical-bar gauge screen (Rev Counter (Bar), and any future one)
 *
 * The bar-shaped sibling of menu_nav_gauge_cfg_st - GFX_draw_bar() (the real vertical fill-from-
 * bottom primitive, not the outline+fill rect GFX_draw_gauge() builds for a horizontal bar) is
 * exactly what a vertical gauge wants, so this is a much thinner config than the needle one: no
 * needle/tick/label tables, just where the bar sits.
 */
typedef struct
{
    const char*             title_p;
    menu_nav_value_edit_st  edit;      /*!< Value + knob feel - see menu_nav_value_edit_st */
    GFX_bar_cfg_st          bar_cfg;   /*!< Shape - see GFX_draw_bar() */
} menu_nav_bar_gauge_cfg_st;

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
STATIC void menu_nav_draw_wifi( void );
STATIC void menu_nav_draw_tb( void );
STATIC void menu_nav_draw_sensors( void );
STATIC void menu_nav_handle_sensors( HMI_SH1106_input_et input );
STATIC void menu_nav_enter_sensors( void );
STATIC void menu_nav_draw_vehicle( void );
STATIC void menu_nav_draw_tire( u8_t tire_x, u8_t tire_y, u8_t label_col, u8_t label_page, const menu_nav_tire_reading_st* reading_p );
STATIC void menu_nav_draw_about( void );
STATIC void menu_nav_draw_bootloader( void );
STATIC void menu_nav_handle_bootloader( HMI_SH1106_input_et input );
STATIC void menu_nav_draw_not_implemented( void );
STATIC void menu_nav_pct_editor_enter( const menu_nav_pct_editor_st* cfg_p );
STATIC void menu_nav_pct_editor_draw( const menu_nav_pct_editor_st* cfg_p );
STATIC void menu_nav_pct_editor_handle( const menu_nav_pct_editor_st* cfg_p, HMI_SH1106_input_et input );
STATIC void menu_nav_draw_brightness( void );
STATIC void menu_nav_handle_brightness( HMI_SH1106_input_et input );
STATIC void menu_nav_enter_brightness( void );
STATIC void menu_nav_draw_fan_speed( void );
STATIC void menu_nav_handle_fan_speed( HMI_SH1106_input_et input );
STATIC void menu_nav_enter_fan_speed( void );
STATIC void menu_nav_draw_buzzer( void );
STATIC void menu_nav_handle_buzzer( HMI_SH1106_input_et input );
STATIC void menu_nav_enter_buzzer( void );
STATIC void menu_nav_value_edit_handle( const menu_nav_value_edit_st* cfg_p, HMI_SH1106_input_et input );
STATIC void menu_nav_gauge_draw( const menu_nav_gauge_cfg_st* cfg_p );
STATIC void menu_nav_gauge_handle( const menu_nav_gauge_cfg_st* cfg_p, HMI_SH1106_input_et input );
STATIC void menu_nav_draw_rev_counter( void );
STATIC void menu_nav_handle_rev_counter( HMI_SH1106_input_et input );
STATIC void menu_nav_bar_gauge_draw( const menu_nav_bar_gauge_cfg_st* cfg_p );
STATIC void menu_nav_bar_gauge_handle( const menu_nav_bar_gauge_cfg_st* cfg_p, HMI_SH1106_input_et input );
STATIC void menu_nav_draw_rev_counter_bar( void );
STATIC void menu_nav_handle_rev_counter_bar( HMI_SH1106_input_et input );

/***************************************************************************************************
**                              Screen contents                                                   **
***************************************************************************************************/
/* ===== Main menu - the only list screen so far. Ten rows against seven item rows, so it scrolls
   and shows the up/down indicators. ===== */
STATIC const MENU_NAV_item_st menu_nav_main_items_s[] =
{
    { "Status",     MENU_NAV_SCREEN_STATUS          },
    { "Brightness", MENU_NAV_SCREEN_BRIGHTNESS      },
    { "Fan Speed",  MENU_NAV_SCREEN_FAN_SPEED       },
    { "WiFi",       MENU_NAV_SCREEN_WIFI            },
    { "TB",         MENU_NAV_SCREEN_TB              },
    { "Vehicle",    MENU_NAV_SCREEN_VEHICLE         },
    { "Rev Counter", MENU_NAV_SCREEN_REV_COUNTER    },
    { "Rev Ctr Bar", MENU_NAV_SCREEN_REV_COUNTER_BAR },
    { "CAN Bus",    MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "LEDs",       MENU_NAV_SCREEN_NOT_IMPLEMENTED },
    { "Buzzer",     MENU_NAV_SCREEN_BUZZER          },
    { "Sensors",    MENU_NAV_SCREEN_SENSORS         },
    { "About",      MENU_NAV_SCREEN_ABOUT           },
    { "Bootloader", MENU_NAV_SCREEN_BOOTLOADER      },
};

STATIC const MENU_NAV_list_st menu_nav_main_list_s =
{
    .title_p    = "MAIN MENU",
    .items_p    = menu_nav_main_items_s,
    .item_count = MENU_NAV_ITEM_COUNT( menu_nav_main_items_s ),
};

/* ===== Vehicle - placeholder tire readings, see menu_nav_tire_reading_st ===== */
STATIC const menu_nav_tire_reading_st menu_nav_tire_fl_s = { 22, 32 };
STATIC const menu_nav_tire_reading_st menu_nav_tire_fr_s = { 23, 33 };
STATIC const menu_nav_tire_reading_st menu_nav_tire_rl_s = { 21, 31 };
STATIC const menu_nav_tire_reading_st menu_nav_tire_rr_s = { 21, 30 };

#define MENU_NAV_TIRE_WIDTH           ( 12u )
#define MENU_NAV_TIRE_HEIGHT          ( 18u )
#define MENU_NAV_TIRE_CORNER_RADIUS   ( 4u )

/* Top-left corner of the left/right tires - see menu_nav_draw_vehicle() */
#define MENU_NAV_TIRE_LEFT_X          ( 42u )
#define MENU_NAV_TIRE_RIGHT_X         ( 70u )
#define MENU_NAV_FRONT_TIRE_Y         ( 12u )
#define MENU_NAV_REAR_TIRE_Y          ( 40u )

/* Axles run between the tires' inner edges, at each pair's vertical centre; the driveshaft runs
   between the two axles' midpoints - see menu_nav_draw_vehicle() */
#define MENU_NAV_AXLE_LEFT_X    ( MENU_NAV_TIRE_LEFT_X + MENU_NAV_TIRE_WIDTH )
#define MENU_NAV_AXLE_RIGHT_X   ( MENU_NAV_TIRE_RIGHT_X )
#define MENU_NAV_FRONT_AXLE_Y   ( MENU_NAV_FRONT_TIRE_Y + ( MENU_NAV_TIRE_HEIGHT / 2u ) )
#define MENU_NAV_REAR_AXLE_Y    ( MENU_NAV_REAR_TIRE_Y  + ( MENU_NAV_TIRE_HEIGHT / 2u ) )
#define MENU_NAV_DRIVESHAFT_X   ( ( MENU_NAV_AXLE_LEFT_X + MENU_NAV_AXLE_RIGHT_X ) / 2u )

/* ===== Rev Counter - placeholder, knob-driven. cps_instance_s/cps_instance_2_s (INTEGRATION_STUBS.c)
   are wheel-speed ABS tone rings wired through the CPS tooth-counter, not an engine crank/cam
   sensor - there is no real engine RPM source anywhere in this codebase, so like Fan Speed this
   only edits a stored value. The knob steps it in MENU_NAV_TACHO_KNOB_STEP_RPM increments rather
   than moving a cursor, same shape as menu_nav_handle_sensors() - but drawing/input both go through
   the generic menu_nav_gauge_draw()/menu_nav_gauge_handle(), configured by menu_nav_rev_counter_gauge_s
   below, rather than Rev Counter having its own copy - see menu_nav_gauge_cfg_st.

   Needle tip and tick mark positions are precomputed pixel offsets from the pivot, one entry per
   MENU_NAV_TACHO_RPM_STEP RPM, rather than computed with sinf/cosf at draw time - the STM32F103
   has no FPU, and libm trig measured ~7KB of the 44K flash budget on this part; this table is
   under 200 bytes. ===== */
#define MENU_NAV_TACHO_MIN_RPM      ( 0u )
#define MENU_NAV_TACHO_MAX_RPM      ( 9000u )
#define MENU_NAV_TACHO_RPM_STEP     ( 100u )    /* Needle table resolution - see menu_nav_tacho_needle_s */
#define MENU_NAV_TACHO_NUM_POINTS   ( (u8_t)( ( ( MENU_NAV_TACHO_MAX_RPM - MENU_NAV_TACHO_MIN_RPM ) / MENU_NAV_TACHO_RPM_STEP ) + 1u ) )

/* How far one detent moves the value - deliberately coarser than the table step above, so a
   couple of clicks sweeps the whole band for a visible needle swing instead of nudging it one
   table entry at a time. Must stay a whole multiple of MENU_NAV_TACHO_RPM_STEP - the index into
   menu_nav_tacho_needle_s (see menu_nav_gauge_draw()) assumes every reachable RPM value lands
   exactly on a table entry. */
#define MENU_NAV_TACHO_KNOB_STEP_RPM ( 500u )

#define MENU_NAV_TACHO_PIVOT_X       ( 64u )
#define MENU_NAV_TACHO_PIVOT_Y       ( 54u )
#define MENU_NAV_TACHO_PIVOT_RADIUS  ( 2u )

/* Horizontal bar, bottom row - sits beside the "NNNN" digital readout rather than under it, so
   the needle gauge above doesn't have to give up a row. x/y are raw pixel coordinates (unlike the
   needle/tick tables, this isn't relative to the pivot), y=56 lands it flush with row 7's top
   edge. Width includes the 1px border GFX_draw_rect_square()/GFX_fill_rect_square() draw at. */
#define MENU_NAV_TACHO_BAR_X         ( 28u )
#define MENU_NAV_TACHO_BAR_Y         ( 56u )
#define MENU_NAV_TACHO_BAR_WIDTH     ( 98u )
#define MENU_NAV_TACHO_BAR_HEIGHT    ( 8u )

/* Needle tip offset from the pivot - index 0 is MENU_NAV_TACHO_MIN_RPM, each entry after that
   MENU_NAV_TACHO_RPM_STEP RPM further, last entry MENU_NAV_TACHO_MAX_RPM. -60 to +60 degrees off
   vertical, radius 30px - generated for this sweep/radius, not hand-tuned, so regenerate the whole
   table if either changes. */
STATIC const GFX_gauge_point_st menu_nav_tacho_needle_s[MENU_NAV_TACHO_NUM_POINTS] =
{
    {-26,-15 },{-26,-16 },{-25,-16 },{-25,-17 },{-24,-17 },{-24,-18 },
    {-24,-18 },{-23,-19 },{-23,-20 },{-22,-20 },{-22,-21 },{-21,-21 },
    {-21,-22 },{-20,-22 },{-20,-23 },{-19,-23 },{-19,-23 },{-18,-24 },
    {-18,-24 },{-17,-25 },{-16,-25 },{-16,-25 },{-15,-26 },{-15,-26 },
    {-14,-26 },{-13,-27 },{-13,-27 },{-12,-27 },{-12,-28 },{-11,-28 },
    {-10,-28 },{-10,-28 },{ -9,-29 },{ -8,-29 },{ -8,-29 },{ -7,-29 },
    { -6,-29 },{ -6,-29 },{ -5,-30 },{ -4,-30 },{ -3,-30 },{ -3,-30 },
    { -2,-30 },{ -1,-30 },{ -1,-30 },{  0,-30 },{  1,-30 },{  1,-30 },
    {  2,-30 },{  3,-30 },{  3,-30 },{  4,-30 },{  5,-30 },{  6,-29 },
    {  6,-29 },{  7,-29 },{  8,-29 },{  8,-29 },{  9,-29 },{ 10,-28 },
    { 10,-28 },{ 11,-28 },{ 12,-28 },{ 12,-27 },{ 13,-27 },{ 13,-27 },
    { 14,-26 },{ 15,-26 },{ 15,-26 },{ 16,-25 },{ 16,-25 },{ 17,-25 },
    { 18,-24 },{ 18,-24 },{ 19,-23 },{ 19,-23 },{ 20,-23 },{ 20,-22 },
    { 21,-22 },{ 21,-21 },{ 22,-21 },{ 22,-20 },{ 23,-20 },{ 23,-19 },
    { 24,-18 },{ 24,-18 },{ 24,-17 },{ 25,-17 },{ 25,-16 },{ 26,-16 },
    { 26,-15 },
};

/* Tick marks every 1000 RPM (outer point, inner point) - same sweep as the needle table above,
   radius 34px outer / 27px inner. */
STATIC const GFX_gauge_tick_st menu_nav_tacho_ticks_s[] =
{
    { -29, -17, -23, -14 },  /* 0 RPM */
    { -25, -23, -20, -19 },  /* 1000 RPM */
    { -19, -28, -15, -23 },  /* 2000 RPM */
    { -12, -32,  -9, -25 },  /* 3000 RPM */
    {  -4, -34,  -3, -27 },  /* 4000 RPM */
    {   4, -34,   3, -27 },  /* 5000 RPM */
    {  12, -32,   9, -25 },  /* 6000 RPM */
    {  19, -28,  15, -23 },  /* 7000 RPM */
    {  25, -23,  20, -19 },  /* 8000 RPM */
    {  29, -17,  23, -14 },  /* 9000 RPM */
};

/* "x1000" digit positions for the Rev Counter dial - col/row rather than a pixel offset from the
   pivot like the tables above, since HMI_SH1106_draw_text() places text on 8px page boundaries,
   not arbitrary pixels, so a label's position is only ever accurate to the nearest row. Radius
   43px, just outside the tick marks' 34px outer radius, same sweep - computed from the same
   sin/cos as the tick/needle tables, except 3/4/5/6 are nudged onto the same row (the raw sweep
   puts 3 and 6 one row lower than 4 and 5, which reads as uneven) - re-check against the real
   display if the sweep/radius changes again. */
STATIC const menu_nav_gauge_label_st menu_nav_tacho_labels_s[MENU_NAV_ITEM_COUNT( menu_nav_tacho_ticks_s )] =
{
    { 25u, 4u, "0" },  /* 0 RPM */
    { 31u, 3u, "1" },  /* 1000 RPM */
    { 38u, 2u, "2" },  /* 2000 RPM */
    { 47u, 1u, "3" },  /* 3000 RPM */
    { 57u, 1u, "4" },  /* 4000 RPM */
    { 67u, 1u, "5" },  /* 5000 RPM */
    { 77u, 1u, "6" },  /* 6000 RPM */
    { 86u, 2u, "7" },  /* 7000 RPM */
    { 93u, 3u, "8" },  /* 8000 RPM */
    { 99u, 4u, "9" },  /* 9000 RPM */
};

STATIC u16_t menu_nav_tacho_rpm_s = MENU_NAV_TACHO_MIN_RPM;

/* The Rev Counter's own instance of the generic gauge mechanism - see menu_nav_gauge_cfg_st. Any
   future needle gauge (boost, coolant temp, ...) is another one of these plus its own tables,
   pivot and bar rect - not a copy of menu_nav_draw_rev_counter()/menu_nav_handle_rev_counter(). */
STATIC const menu_nav_gauge_cfg_st menu_nav_rev_counter_gauge_s =
{
    .title_p = "REV COUNTER",
    .edit    =
    {
        .value_p   = &menu_nav_tacho_rpm_s,
        .min       = MENU_NAV_TACHO_MIN_RPM,
        .max       = MENU_NAV_TACHO_MAX_RPM,
        .knob_step = MENU_NAV_TACHO_KNOB_STEP_RPM,
        .screen    = MENU_NAV_SCREEN_REV_COUNTER,
    },
    .gfx_cfg =
    {
        .needle_table_p = menu_nav_tacho_needle_s,
        .tick_table_p   = menu_nav_tacho_ticks_s,
        .num_ticks      = MENU_NAV_ITEM_COUNT( menu_nav_tacho_ticks_s ),
        .value_step     = MENU_NAV_TACHO_RPM_STEP,
        .pivot_x        = MENU_NAV_TACHO_PIVOT_X,
        .pivot_y        = MENU_NAV_TACHO_PIVOT_Y,
        .pivot_radius   = MENU_NAV_TACHO_PIVOT_RADIUS,
        .bar_x          = MENU_NAV_TACHO_BAR_X,
        .bar_y          = MENU_NAV_TACHO_BAR_Y,
        .bar_width      = MENU_NAV_TACHO_BAR_WIDTH,
        .bar_height     = MENU_NAV_TACHO_BAR_HEIGHT,
    },
    .label_table_p = menu_nav_tacho_labels_s,
};

/* ===== Rev Counter (Bar) - the same placeholder RPM value as the needle Rev Counter
   (menu_nav_tacho_rpm_s), just drawn as a vertical fill bar via GFX_draw_bar() instead of a needle
   dial - turning the knob on either screen moves the same number, so they're two views of one
   reading rather than two independent placeholders. GFX_draw_bar() scales against (max - min), not
   raw max, the same way the needle Rev Counter's horizontal bar does - value == min reads as
   empty, which with MENU_NAV_TACHO_MIN_RPM at 0 is also value == 0. ===== */
#define MENU_NAV_TACHO_BAR_V_X         ( 49u )
#define MENU_NAV_TACHO_BAR_V_WIDTH     ( 30u )
#define MENU_NAV_TACHO_BAR_V_TOP_PAGE  ( 1u )
#define MENU_NAV_TACHO_BAR_V_BOT_PAGE  ( 6u )

STATIC const menu_nav_bar_gauge_cfg_st menu_nav_rev_counter_bar_gauge_s =
{
    .title_p = "REV COUNTER (BAR)",
    .edit    =
    {
        .value_p   = &menu_nav_tacho_rpm_s,
        .min       = MENU_NAV_TACHO_MIN_RPM,
        .max       = MENU_NAV_TACHO_MAX_RPM,
        .knob_step = MENU_NAV_TACHO_KNOB_STEP_RPM,
        .screen    = MENU_NAV_SCREEN_REV_COUNTER_BAR,
    },
    .bar_cfg =
    {
        .x_col    = MENU_NAV_TACHO_BAR_V_X,
        .width    = MENU_NAV_TACHO_BAR_V_WIDTH,
        .top_page = MENU_NAV_TACHO_BAR_V_TOP_PAGE,
        .bot_page = MENU_NAV_TACHO_BAR_V_BOT_PAGE,
    },
};

/* ===== Brightness - the value this screen edits, and what it reverts to if BACK cancels ===== */
STATIC u8_t menu_nav_brightness_pct_s   = 75u;
STATIC u8_t menu_nav_brightness_saved_s = 75u;

#define MENU_NAV_BRIGHTNESS_MIN         ( 5u )
#define MENU_NAV_BRIGHTNESS_MAX         ( 100u )
#define MENU_NAV_BRIGHTNESS_STEP        ( 5u )

/* ===== Fan Speed - placeholder, same shape as Brightness. There is no fan/PWM driver wired into
   this product yet (see PWM.c, unused anywhere in APP), so unlike Brightness this only edits a
   stored value - nothing downstream to apply it to until real hardware exists. ===== */
STATIC u8_t menu_nav_fan_speed_pct_s   = 0u;
STATIC u8_t menu_nav_fan_speed_saved_s = 0u;

#define MENU_NAV_FAN_SPEED_MIN          ( 0u )
#define MENU_NAV_FAN_SPEED_MAX          ( 100u )
#define MENU_NAV_FAN_SPEED_STEP         ( 5u )

STATIC const menu_nav_pct_editor_st menu_nav_brightness_editor_s =
{
    .title_p       = "BRIGHTNESS",
    .value_p       = &menu_nav_brightness_pct_s,
    .saved_p       = &menu_nav_brightness_saved_s,
    .min           = MENU_NAV_BRIGHTNESS_MIN,
    .max           = MENU_NAV_BRIGHTNESS_MAX,
    .step          = MENU_NAV_BRIGHTNESS_STEP,
    .apply_func_p  = HMI_SH1106_set_brightness_pct,
    .screen        = MENU_NAV_SCREEN_BRIGHTNESS,
};

STATIC const menu_nav_pct_editor_st menu_nav_fan_speed_editor_s =
{
    .title_p       = "FAN SPEED",
    .value_p       = &menu_nav_fan_speed_pct_s,
    .saved_p       = &menu_nav_fan_speed_saved_s,
    .min           = MENU_NAV_FAN_SPEED_MIN,
    .max           = MENU_NAV_FAN_SPEED_MAX,
    .step          = MENU_NAV_FAN_SPEED_STEP,
    .apply_func_p  = NULL_P,       /* no fan/PWM driver wired in yet */
    .screen        = MENU_NAV_SCREEN_FAN_SPEED,
};

/* Fan icon, right side of the Fan Speed screen, clear of the editor's text (col 0-72 at most, for
   "BACK cancels") and the cursor-margin-free text this screen already draws - see
   menu_nav_draw_fan_speed(). Static, not spun by pct - GFX has no cheap way to draw at an
   arbitrary angle (see GFX_fan_cfg_st on why 4 blades avoids trig only at fixed N/E/S/W). */
STATIC const GFX_fan_cfg_st menu_nav_fan_speed_icon_s =
{
    .hub_x        = 100u,
    .hub_y        = 36u,
    .hub_radius   = 4u,
    .blade_length = 14u,
};

/* ===== Buzzer - one screen, two fields: whether a press beeps, and for how long. Both revert
   together if BACK cancels; the knob edits whichever field is currently selected. ===== */
STATIC false_true_et menu_nav_buzzer_enabled_s = TRUE;
STATIC false_true_et menu_nav_buzzer_saved_s   = TRUE;

STATIC u16_t menu_nav_beep_time_ms_s    = BUZZER_SHORT_BEEP_MS;
STATIC u16_t menu_nav_beep_time_saved_s = BUZZER_SHORT_BEEP_MS;

#define MENU_NAV_BEEP_TIME_MIN_MS       ( 10u )
#define MENU_NAV_BEEP_TIME_MAX_MS       ( 500u )
#define MENU_NAV_BEEP_TIME_STEP_MS      ( 10u )

typedef enum
{
    MENU_NAV_BUZZER_FIELD_STATE = 0u,
    MENU_NAV_BUZZER_FIELD_TIME,
    MENU_NAV_BUZZER_NUM_FIELDS
} menu_nav_buzzer_field_et;

STATIC menu_nav_buzzer_field_et menu_nav_buzzer_field_s = MENU_NAV_BUZZER_FIELD_STATE;

/* ===== Sensors - which sensor slot the knob is currently showing ===== */
STATIC u8_t menu_nav_sensor_index_s = 0u;

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

    [MENU_NAV_SCREEN_FAN_SPEED] =
    {
        .draw_func_p     = menu_nav_draw_fan_speed,
        .handle_func_p   = menu_nav_handle_fan_speed,   /* The knob edits rather than navigates */
        .on_enter_func_p = menu_nav_enter_fan_speed,    /* Remember what to revert to */
        .back_screen     = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_BUZZER] =
    {
        .draw_func_p     = menu_nav_draw_buzzer,
        .handle_func_p   = menu_nav_handle_buzzer,   /* SELECT swaps field, the knob edits it */
        .on_enter_func_p = menu_nav_enter_buzzer,    /* Remember what to revert to */
        .back_screen     = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_WIFI] =
    {
        .draw_func_p  = menu_nav_draw_wifi,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_TB] =
    {
        .draw_func_p  = menu_nav_draw_tb,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_SENSORS] =
    {
        .draw_func_p     = menu_nav_draw_sensors,
        .handle_func_p   = menu_nav_handle_sensors,   /* The knob steps through sensor slots */
        .on_enter_func_p = menu_nav_enter_sensors,    /* Always start back at slot 0 */
        .back_screen     = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_VEHICLE] =
    {
        .draw_func_p  = menu_nav_draw_vehicle,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_REV_COUNTER] =
    {
        .draw_func_p   = menu_nav_draw_rev_counter,
        .handle_func_p = menu_nav_handle_rev_counter,   /* The knob steps the value, not a cursor */
        .back_screen   = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_REV_COUNTER_BAR] =
    {
        .draw_func_p   = menu_nav_draw_rev_counter_bar,
        .handle_func_p = menu_nav_handle_rev_counter_bar,   /* The knob steps the value, not a cursor */
        .back_screen   = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_ABOUT] =
    {
        .draw_func_p  = menu_nav_draw_about,
        .back_screen  = MENU_NAV_SCREEN_MAIN_MENU,
    },

    [MENU_NAV_SCREEN_BOOTLOADER] =
    {
        .draw_func_p   = menu_nav_draw_bootloader,
        .handle_func_p = menu_nav_handle_bootloader,   /* CONFIRM resets into FBL, not the default */
        .back_screen   = MENU_NAV_SCREEN_MAIN_MENU,
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
*   \note          A long BACK press always ends at MENU_NAV_SCREEN_HOME - no screen can redirect
*                  it elsewhere or suppress the jump. A screen with handle_func_p does still see it
*                  first, purely so it can react (a value editor reverting to what it captured on
*                  entry, for instance) before it is navigated away from - see the note on
*                  MENU_NAV_screen_st.handle_func_p.
*
***************************************************************************************************/
void MENU_NAV_on_input( HMI_SH1106_input_et input )
{
    const MENU_NAV_screen_st* screen_p = &menu_nav_screens_s[menu_nav_screen_s];

    if( input == HMI_SH1106_INPUT_BACK_LONG )
    {
        if( screen_p->handle_func_p != NULL_P )
        {
            screen_p->handle_func_p( input );
        }

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
    HMI_SH1106_draw_text( 1u, 0u, "HUB", TRUE );
    HMI_SH1106_draw_text( 3u, 0u, "Press CONFIRM", TRUE );
    HMI_SH1106_draw_text( 4u, 0u, "for the menu", TRUE );
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

    HMI_SH1106_draw_text( 0u, 0u, "STATUS", FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "CW %lu  CCW %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CW],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CCW] );
    HMI_SH1106_draw_text( 2u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Sel %lu  SelL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_SELECT],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_SELECT_LONG] );
    HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Cfm %lu  CfmL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CONFIRM],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_CONFIRM_LONG] );
    HMI_SH1106_draw_text( 4u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Bck %lu  BckL %lu",
                         (unsigned long)stats.count[HMI_SH1106_INPUT_BACK],
                         (unsigned long)stats.count[HMI_SH1106_INPUT_BACK_LONG] );
    HMI_SH1106_draw_text( 5u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         WiFi - live status straight off the WIFI module
*
*   \author        MS
*
*   \return        none
*
*   \note          Repainted by the panel's own refresh timer, same as Status - RSSI and connection
*                  state keep updating while the screen is open.
*
***************************************************************************************************/
STATIC void menu_nav_draw_wifi( void )
{
    char        line[MENU_NAV_LINE_CHARS];
    const char* status_str;

    switch( WIFI_get_sta_connection_status() )
    {
        case WIFI_STA_CONNECTED_TO_AP:
            status_str = "Connected";
        break;

        case WIFI_STA_CONNECTING_TO_AP:
            status_str = "Connecting";
        break;

        default:
            status_str = "Disconnected";
        break;
    }

    HMI_SH1106_draw_text( 0u, 0u, "WIFI", FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Status %s", status_str );
    HMI_SH1106_draw_text( 2u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "IP %s", (const char*)WIFI_get_ip_address() );
    HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "RSSI %d dBm", (int)WIFI_get_rssi() );
    HMI_SH1106_draw_text( 4u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "MAC %s", (const char*)WIFI_get_mac_address() );
    HMI_SH1106_draw_text( 5u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Disconnects %lu", (unsigned long)WIFI_get_disconnect_count() );
    HMI_SH1106_draw_text( 6u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         TB - live message counts straight off the TB module
*
*   \author        MS
*
*   \return        none
*
*   \note          Repainted by the panel's own refresh timer, same as Status/WiFi - the counts
*                  keep climbing while the screen is open.
*
***************************************************************************************************/
STATIC void menu_nav_draw_tb( void )
{
    char        line[MENU_NAV_LINE_CHARS];
    const char* status_str;

    switch( TB_get_state() )
    {
        case TB_STATE_CONNECTED:
            status_str = "Connected";
        break;

        case TB_STATE_DISCONNECTED:
            status_str = "Disconnected";
        break;

        case TB_STATE_ERROR:
            status_str = "Error";
        break;

        default:
            status_str = "Connecting";
        break;
    }

    HMI_SH1106_draw_text( 0u, 0u, "TB", FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Status %s", status_str );
    HMI_SH1106_draw_text( 2u, 0u, line, FALSE );

//    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Sent %lu", (unsigned long)TB_get_messages_sent_count() );
    HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

   // (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Recv %lu", (unsigned long)TB_get_messages_received_count() );
    HMI_SH1106_draw_text( 4u, 0u, line, FALSE );

    //(void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Dropped %u", (unsigned int)TB_get_dropped_count() );
    HMI_SH1106_draw_text( 5u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Sensors - reset to the first sensor slot on the way in
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_enter_sensors( void )
{
    menu_nav_sensor_index_s = 0u;
}

/*!
****************************************************************************************************
*
*   \brief         Sensors - the sensor DB slot the knob is currently showing
*
*   \author        MS
*
*   \return        none
*
*   \note          Repainted by the panel's own refresh timer, so a node's readings and RX count
*                  keep updating while it stays on screen. Steps through every slot in
*                  RF_MGR_get_sensor_db(), not just ones that have reported in - an empty slot is
*                  shown as such rather than being skipped, so the count in the title always matches
*                  RF_MGR_MAX_SENSORS and turning the knob never appears to do nothing.
*
***************************************************************************************************/
STATIC void menu_nav_draw_sensors( void )
{
    const RF_MGR_sensor_data_st* node_p = &RF_MGR_get_sensor_db()[menu_nav_sensor_index_s];
    char                          line[MENU_NAV_LINE_CHARS];

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "SENSOR %u/%u",
                         (unsigned int)( menu_nav_sensor_index_s + 1u ), (unsigned int)RF_MGR_MAX_SENSORS );
    HMI_SH1106_draw_text( 0u, 0u, line, FALSE );

    if( node_p->valid == TRUE )
    {
        s16_t       temp_whole = (s16_t)( node_p->temperature_centidegC / 100 );
        s16_t       temp_frac  = (s16_t)( node_p->temperature_centidegC % 100 );
        s16_t       hum_whole  = (s16_t)( node_p->humidity_tenths_pct / 10 );
        s16_t       hum_frac   = (s16_t)( node_p->humidity_tenths_pct % 10 );
        /* Cast to u32_t before dividing, not after - dividing while still u64_t pulls the ~700-byte
           __aeabi_uldivmod helper into the link for this one call site. Safe to truncate: comms_lost
           already trips after RF_MGR_COMMS_LOST_TIMEOUT_SECS (30 minutes), so this age is never
           anywhere near the ~49.7 days a 32-bit millisecond difference can hold before wrapping. */
        u32_t       age_secs   = (u32_t)( TIME_get_cumulative_run_time_ms() - node_p->last_rx_time_ms ) / MSECS_PER_SEC;
        const char* batt_str;

        temp_frac = ( temp_frac < 0 ) ? (s16_t)-temp_frac : temp_frac;
        hum_frac  = ( hum_frac  < 0 ) ? (s16_t)-hum_frac  : hum_frac;

        if( ( node_p->battery_flags & (u8_t)( 1u << RF_MGR_BAT_FLAG_CRITICAL_BIT ) ) != 0u )
        {
            batt_str = "CRIT";
        }
        else if( ( node_p->battery_flags & (u8_t)( 1u << RF_MGR_BAT_FLAG_LOW_BIT ) ) != 0u )
        {
            batt_str = "LOW";
        }
        else
        {
            batt_str = "OK";
        }

        (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "ID 0x%08lX", (unsigned long)node_p->sensor_id );
        HMI_SH1106_draw_text( 2u, 0u, line, FALSE );

        (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "T %d.%02dC  H %d.%01d%%",
                             (int)temp_whole, (int)temp_frac, (int)hum_whole, (int)hum_frac );
        HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

        (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Batt %umV %s",
                             (unsigned int)node_p->battery_voltage_mv, batt_str );
        HMI_SH1106_draw_text( 4u, 0u, line, FALSE );

        (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "RX %lu  %s",
                             (unsigned long)node_p->rx_frame_count, ( node_p->comms_lost == TRUE ) ? "LOST" : "OK" );
        HMI_SH1106_draw_text( 5u, 0u, line, FALSE );

        (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Age %lus", (unsigned long)age_secs );
        HMI_SH1106_draw_text( 6u, 0u, line, FALSE );
    }
    else
    {
        HMI_SH1106_draw_text( 3u, 0u, "Empty slot", FALSE );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Sensors - the knob steps through sensor slots instead of moving a cursor
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_handle_sensors( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
            menu_nav_sensor_index_s = ( menu_nav_sensor_index_s >= (u8_t)( RF_MGR_MAX_SENSORS - 1u ) )
                                     ? 0u : (u8_t)( menu_nav_sensor_index_s + 1u );
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_CCW:
            menu_nav_sensor_index_s = ( menu_nav_sensor_index_s == 0u )
                                     ? (u8_t)( RF_MGR_MAX_SENSORS - 1u ) : (u8_t)( menu_nav_sensor_index_s - 1u );
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
        case HMI_SH1106_INPUT_BACK:
            MENU_NAV_goto( menu_nav_screens_s[MENU_NAV_SCREEN_SENSORS].back_screen );
        break;

        default:
            /* Long select/confirm mean nothing here */
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Vehicle - one tire's outline plus its temp/pressure readout
*
*   \author        MS
*
*   \param         tire_x, tire_y - Top-left corner of the tire outline, raw pixel coordinates
*   \param         label_col      - Pixel column for both text lines - drawn margin-free (see
*                                    HMI_SH1106_draw_text), so this is the exact left edge of the
*                                    first glyph
*   \param         label_page     - Page (row) the temperature line lands on; the pressure line
*                                    uses the next page down
*   \param         reading_p      - What to print beside the tire
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_tire( u8_t tire_x, u8_t tire_y, u8_t label_col, u8_t label_page,
                                const menu_nav_tire_reading_st* reading_p )
{
    char          line[MENU_NAV_LINE_CHARS];
    GFX_target_st target           = HMI_SH1106_get_target();
    u16_t         press_mbar       = PRESS_CONV_psi_to_mbar( reading_p->pressure_psi );
    u16_t         press_bar_tenths = (u16_t)( ( press_mbar + 50u ) / 100u );  /* Rounded to nearest 0.1 bar */

    GFX_draw_rect_rounded( &target, tire_x, tire_y, MENU_NAV_TIRE_WIDTH, MENU_NAV_TIRE_HEIGHT, MENU_NAV_TIRE_CORNER_RADIUS );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%dC", (int)reading_p->temp_c );
    HMI_SH1106_draw_text( label_page, label_col, line, FALSE );

    /* Tire pressure is conventionally quoted to one decimal place in bar - PRESS_CONV itself only
       deals in whole units (see PRESS_CONV_mbar_to_bar), so the one decimal digit is split out
       here rather than in PRESS_CONV, the same way this function already owns its own formatting
       for temperature. */
    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%u.%ubar",
                         (unsigned int)( press_bar_tenths / 10u ), (unsigned int)( press_bar_tenths % 10u ) );
    HMI_SH1106_draw_text( (u8_t)( label_page + 1u ), label_col, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Vehicle - four tires laid out like they sit on a real car, seen from above
*
*   \author        MS
*
*   \return        none
*
*   \note          Front axle top, rear axle bottom, tires hugging the left/right edges the way
*                  they actually sit either side of the centreline. Readings run along the outer
*                  edges, outside the tire they belong to - see menu_nav_draw_tire(). Placeholder
*                  data (menu_nav_tire_fl_s etc.) until a real tire pressure sensor exists to read
*                  from. Front/rear axles and the driveshaft between them are drawn straight -
*                  simple lines, no attempt at a differential or anything closer to a real
*                  drivetrain layout.
*
***************************************************************************************************/
STATIC void menu_nav_draw_vehicle( void )
{
    GFX_target_st target = HMI_SH1106_get_target();

    HMI_SH1106_draw_text( 0u, 0u, "VEHICLE", FALSE );

    menu_nav_draw_tire( MENU_NAV_TIRE_LEFT_X,  MENU_NAV_FRONT_TIRE_Y, 0u,  2u, &menu_nav_tire_fl_s );
    menu_nav_draw_tire( MENU_NAV_TIRE_RIGHT_X, MENU_NAV_FRONT_TIRE_Y, 82u, 2u, &menu_nav_tire_fr_s );
    menu_nav_draw_tire( MENU_NAV_TIRE_LEFT_X,  MENU_NAV_REAR_TIRE_Y,  0u,  5u, &menu_nav_tire_rl_s );
    menu_nav_draw_tire( MENU_NAV_TIRE_RIGHT_X, MENU_NAV_REAR_TIRE_Y,  82u, 5u, &menu_nav_tire_rr_s );

    GFX_draw_line( &target, MENU_NAV_AXLE_LEFT_X, MENU_NAV_FRONT_AXLE_Y, MENU_NAV_AXLE_RIGHT_X, MENU_NAV_FRONT_AXLE_Y );
    GFX_draw_line( &target, MENU_NAV_AXLE_LEFT_X, MENU_NAV_REAR_AXLE_Y,  MENU_NAV_AXLE_RIGHT_X, MENU_NAV_REAR_AXLE_Y );
    GFX_draw_line( &target, MENU_NAV_DRIVESHAFT_X, MENU_NAV_FRONT_AXLE_Y, MENU_NAV_DRIVESHAFT_X, MENU_NAV_REAR_AXLE_Y );
}

/*!
****************************************************************************************************
*
*   \brief         Value edit - the knob steps a live value instead of moving a cursor
*
*   \author        MS
*
*   \param         cfg_p - Value, range and knob feel - see menu_nav_value_edit_st
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          Shared by every gauge shape (needle, vertical bar, ...) - CW/CCW/clamp/navigate
*                  don't care how the value ends up on screen, only menu_nav_gauge_draw()/
*                  menu_nav_bar_gauge_draw() differ on that. Clamped, not wrapped - CW past max or
*                  CCW past min just holds there rather than snapping to the other end, the way a
*                  real gauge would.
*
***************************************************************************************************/
STATIC void menu_nav_value_edit_handle( const menu_nav_value_edit_st* cfg_p, HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
        {
            u16_t next = (u16_t)( *cfg_p->value_p + cfg_p->knob_step );
            *cfg_p->value_p = ( next > cfg_p->max ) ? cfg_p->max : next;
            HMI_SH1106_request_redraw();
        }
        break;

        case HMI_SH1106_INPUT_CCW:
            *cfg_p->value_p = ( *cfg_p->value_p <= (u16_t)( cfg_p->min + cfg_p->knob_step ) )
                             ? cfg_p->min
                             : (u16_t)( *cfg_p->value_p - cfg_p->knob_step );
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
        case HMI_SH1106_INPUT_BACK:
            MENU_NAV_goto( menu_nav_screens_s[cfg_p->screen].back_screen );
        break;

        default:
            /* Long select/confirm mean nothing here */
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Gauge - needle, ticks, dial labels and a linear bar, knob-driven
*
*   \author        MS
*
*   \param         cfg_p - Which gauge (Rev Counter, ...) is being drawn - see menu_nav_gauge_cfg_st
*
*   \return        none
*
*   \note          The shapes (ticks, needle, bar) are entirely GFX_draw_gauge()'s job - this only
*                   adds what GFX can't draw itself: the title, the tick labels and the digital
*                   readout, all text (see menu_nav_gauge_cfg_st for why that split exists).
*
***************************************************************************************************/
STATIC void menu_nav_gauge_draw( const menu_nav_gauge_cfg_st* cfg_p )
{
    GFX_target_st target = HMI_SH1106_get_target();
    char          line[MENU_NAV_LINE_CHARS];
    u8_t          i;

    HMI_SH1106_draw_text( 0u, 0u, cfg_p->title_p, FALSE );

    GFX_draw_gauge( &target, &cfg_p->gfx_cfg, *cfg_p->edit.value_p, cfg_p->edit.min, cfg_p->edit.max );

    for( i = 0u; i < cfg_p->gfx_cfg.num_ticks; i++ )
    {
        HMI_SH1106_draw_text( cfg_p->label_table_p[i].row, cfg_p->label_table_p[i].col,
                               cfg_p->label_table_p[i].text_p, FALSE );
    }

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%u", (unsigned int)*cfg_p->edit.value_p );
    HMI_SH1106_draw_text( 7u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Gauge - the knob steps the value instead of moving a cursor
*
*   \author        MS
*
*   \param         cfg_p - Which gauge (Rev Counter, ...) is being driven
*   \param         input - What the panel reported
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_gauge_handle( const menu_nav_gauge_cfg_st* cfg_p, HMI_SH1106_input_et input )
{
    menu_nav_value_edit_handle( &cfg_p->edit, input );
}

/*!
****************************************************************************************************
*
*   \brief         Rev Counter - needle gauge plus a linear bar, knob-driven placeholder value
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_rev_counter( void )
{
    menu_nav_gauge_draw( &menu_nav_rev_counter_gauge_s );
}

/*!
****************************************************************************************************
*
*   \brief         Rev Counter - the knob steps the value instead of moving a cursor
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_handle_rev_counter( HMI_SH1106_input_et input )
{
    menu_nav_gauge_handle( &menu_nav_rev_counter_gauge_s, input );
}

/*!
****************************************************************************************************
*
*   \brief         Bar gauge - title, GFX_draw_bar() and a digital readout, knob-driven
*
*   \author        MS
*
*   \param         cfg_p - Which bar gauge (Rev Counter (Bar), ...) is being drawn
*
*   \return        none
*
*   \note          value - min is what GFX_draw_bar() scales against, not the raw value - see
*                   menu_nav_bar_gauge_cfg_st's comment on why an empty bar has to mean value == min.
*
***************************************************************************************************/
STATIC void menu_nav_bar_gauge_draw( const menu_nav_bar_gauge_cfg_st* cfg_p )
{
    GFX_target_st target = HMI_SH1106_get_target();
    char          line[MENU_NAV_LINE_CHARS];

    HMI_SH1106_draw_text( 0u, 0u, cfg_p->title_p, FALSE );

    GFX_draw_bar( &target, &cfg_p->bar_cfg,
                  (u16_t)( *cfg_p->edit.value_p - cfg_p->edit.min ),
                  (u16_t)( cfg_p->edit.max - cfg_p->edit.min ) );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%u", (unsigned int)*cfg_p->edit.value_p );
    HMI_SH1106_draw_text( 7u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Bar gauge - the knob steps the value instead of moving a cursor
*
*   \author        MS
*
*   \param         cfg_p - Which bar gauge (Rev Counter (Bar), ...) is being driven
*   \param         input - What the panel reported
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_bar_gauge_handle( const menu_nav_bar_gauge_cfg_st* cfg_p, HMI_SH1106_input_et input )
{
    menu_nav_value_edit_handle( &cfg_p->edit, input );
}

/*!
****************************************************************************************************
*
*   \brief         Rev Counter (Bar) - vertical fill bar, knob-driven placeholder value
*
*   \author        MS
*
*   \return        none
*
*   \note          Shares menu_nav_tacho_rpm_s with the needle Rev Counter - see the data section's
*                   comment for why.
*
***************************************************************************************************/
STATIC void menu_nav_draw_rev_counter_bar( void )
{
    menu_nav_bar_gauge_draw( &menu_nav_rev_counter_bar_gauge_s );
}

/*!
****************************************************************************************************
*
*   \brief         Rev Counter (Bar) - the knob steps the value instead of moving a cursor
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_handle_rev_counter_bar( HMI_SH1106_input_et input )
{
    menu_nav_bar_gauge_handle( &menu_nav_rev_counter_bar_gauge_s, input );
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

    HMI_SH1106_draw_text( 0u, 0u, "ABOUT", FALSE );

    VER_get_sw_version_num( sw_ver );
    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "SW v%u.%u.%u", sw_ver[0], sw_ver[1], sw_ver[2] );
    HMI_SH1106_draw_text( 2u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Build %s", VER_get_sw_release_type() );
    HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

    /* BUILD_DATE/MONTH/YEAR straight from autoversion.h, not VER_get_build_date() - that one
       packs the u16_t year into a u8_t buffer slot and truncates it (VER.c), a pre-existing bug
       in that module, unrelated to this screen. */
    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Date %02u/%02u/%u",
                         (unsigned int)BUILD_DATE, (unsigned int)BUILD_MONTH, (unsigned int)BUILD_YEAR );
    HMI_SH1106_draw_text( 4u, 0u, line, FALSE );

    VER_get_hw_version_num( hw_ver );
    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "HW v%u.%u", hw_ver[0], hw_ver[1] );
    HMI_SH1106_draw_text( 5u, 0u, line, FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Bootloader - confirm before resetting into FBL
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_bootloader( void )
{
    HMI_SH1106_draw_text( 1u, 0u, "ENTER BOOTLOADER?", TRUE );
    HMI_SH1106_draw_text( 3u, 0u, "CONFIRM = Yes", TRUE );
    HMI_SH1106_draw_text( 4u, 0u, "BACK = Cancel", TRUE );
}

/*!
****************************************************************************************************
*
*   \brief         Bootloader - CONFIRM sets the FBL request flag and resets, BACK cancels
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          SHARED_RAM_set_fbl_request(TRUE) followed by a reset is the same explicit-request
*                  path BM_run() checks on every boot (see BM.c's BM_check_for_reprog_flag()) - BM
*                  still validates FBL's presence/CRC/signature before actually jumping to it, the
*                  same as every other path into FBL, so a corrupt/missing FBL image fails safe
*                  rather than this screen being able to strand the device. SHARED_RAM's magic is
*                  already valid by the time APP is running (BM sets it before ever jumping here),
*                  so no SHARED_RAM_init() call is needed first. MCU_JUMP_software_reset() never
*                  returns, so CONFIRM has no case that falls through to anything after it.
*
***************************************************************************************************/
STATIC void menu_nav_handle_bootloader( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
            SHARED_RAM_set_fbl_request( TRUE );
            MCU_JUMP_software_reset();
        break;

        case HMI_SH1106_INPUT_BACK:
            MENU_NAV_goto( MENU_NAV_SCREEN_MAIN_MENU );
        break;

        default:
            /* The knob does nothing on a confirm screen */
        break;
    }
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
    HMI_SH1106_draw_text( 2u, 0u, "Not implemented", TRUE );
    HMI_SH1106_draw_text( 3u, 0u, "yet", TRUE );
}

/*!
****************************************************************************************************
*
*   \brief         Percent editor - capture the starting value so BACK can put it back
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_pct_editor_enter( const menu_nav_pct_editor_st* cfg_p )
{
    *cfg_p->saved_p = *cfg_p->value_p;
}

/*!
****************************************************************************************************
*
*   \brief         Percent editor - show the value being edited
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_pct_editor_draw( const menu_nav_pct_editor_st* cfg_p )
{
    char line[MENU_NAV_LINE_CHARS];

    HMI_SH1106_draw_text( 0u, 0u, cfg_p->title_p, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%u%%", (unsigned int)*cfg_p->value_p );
    HMI_SH1106_draw_text( 3u, 0u, line, FALSE );

    HMI_SH1106_draw_text( 6u, 0u, "BACK cancels", FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Percent editor - the knob edits the value instead of moving a cursor
*
*   \author        MS
*
*   \param         cfg_p - Which screen (Brightness, Fan Speed, ...) is being driven
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          apply_func_p, if set, is called on every step and on both revert paths, so a
*                  screen with a live hardware value (Brightness) dims/brightens as it is turned and
*                  reverting feels like an undo rather than just an exit - the same way it always did
*                  before Brightness and Fan Speed shared this function. BACK_LONG reverts the same
*                  way rather than leaving the edit applied - without this case it would fall through
*                  to default and do nothing, and the panic-button escape to home would silently keep
*                  whatever value was last turned to. MENU_NAV_on_input() calls this before it
*                  navigates away, and navigates away regardless of what happens here.
*
***************************************************************************************************/
STATIC void menu_nav_pct_editor_handle( const menu_nav_pct_editor_st* cfg_p, HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
            if( *cfg_p->value_p <= (u8_t)( cfg_p->max - cfg_p->step ) )
            {
                *cfg_p->value_p += cfg_p->step;
                if( cfg_p->apply_func_p != NULL_P )
                {
                    cfg_p->apply_func_p( *cfg_p->value_p );
                }
                HMI_SH1106_request_redraw();
            }
        break;

        case HMI_SH1106_INPUT_CCW:
            if( *cfg_p->value_p >= (u8_t)( cfg_p->min + cfg_p->step ) )
            {
                *cfg_p->value_p -= cfg_p->step;
                if( cfg_p->apply_func_p != NULL_P )
                {
                    cfg_p->apply_func_p( *cfg_p->value_p );
                }
                HMI_SH1106_request_redraw();
            }
        break;

        case HMI_SH1106_INPUT_SELECT:
        case HMI_SH1106_INPUT_CONFIRM:
            /* Keep it - the value is already live, there is nothing to commit */
            MENU_NAV_goto( menu_nav_screens_s[cfg_p->screen].back_screen );
        break;

        case HMI_SH1106_INPUT_BACK:
            *cfg_p->value_p = *cfg_p->saved_p;
            if( cfg_p->apply_func_p != NULL_P )
            {
                cfg_p->apply_func_p( *cfg_p->value_p );
            }
            MENU_NAV_goto( menu_nav_screens_s[cfg_p->screen].back_screen );
        break;

        case HMI_SH1106_INPUT_BACK_LONG:
            /* MENU_NAV_on_input() navigates home right after this returns - just revert */
            *cfg_p->value_p = *cfg_p->saved_p;
            if( cfg_p->apply_func_p != NULL_P )
            {
                cfg_p->apply_func_p( *cfg_p->value_p );
            }
        break;

        default:
            /* Long select/confirm mean nothing while editing */
        break;
    }
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
    menu_nav_pct_editor_enter( &menu_nav_brightness_editor_s );
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
    menu_nav_pct_editor_draw( &menu_nav_brightness_editor_s );
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
***************************************************************************************************/
STATIC void menu_nav_handle_brightness( HMI_SH1106_input_et input )
{
    menu_nav_pct_editor_handle( &menu_nav_brightness_editor_s, input );
}

/*!
****************************************************************************************************
*
*   \brief         Fan Speed - capture the starting value so BACK can put it back
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_enter_fan_speed( void )
{
    menu_nav_pct_editor_enter( &menu_nav_fan_speed_editor_s );
}

/*!
****************************************************************************************************
*
*   \brief         Fan Speed - show the value being edited, plus a fan icon
*
*   \author        MS
*
*   \return        none
*
*   \note          The icon is drawn on top of the shared percent-editor layout rather than that
*                   function taking an optional icon - Brightness has no equivalent, so this stays
*                   a one-line addition on the Fan Speed side instead of a new field every other
*                   percent-editor screen has to leave NULL_P.
*
***************************************************************************************************/
STATIC void menu_nav_draw_fan_speed( void )
{
    GFX_target_st target = HMI_SH1106_get_target();

    menu_nav_pct_editor_draw( &menu_nav_fan_speed_editor_s );
    GFX_draw_fan( &target, &menu_nav_fan_speed_icon_s );
}

/*!
****************************************************************************************************
*
*   \brief         Fan Speed - the knob edits the value instead of moving a cursor
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          Same shape as menu_nav_handle_brightness() bar apply_func_p being NULL_P - there is
*                  no fan/PWM driver wired into this product yet, so this only edits the stored value.
*
***************************************************************************************************/
STATIC void menu_nav_handle_fan_speed( HMI_SH1106_input_et input )
{
    menu_nav_pct_editor_handle( &menu_nav_fan_speed_editor_s, input );
}

/*!
****************************************************************************************************
*
*   \brief         Buzzer - capture the starting value so BACK can put it back
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_enter_buzzer( void )
{
    menu_nav_buzzer_saved_s    = menu_nav_buzzer_enabled_s;
    menu_nav_beep_time_saved_s = menu_nav_beep_time_ms_s;
    menu_nav_buzzer_field_s    = MENU_NAV_BUZZER_FIELD_STATE;
}

/*!
****************************************************************************************************
*
*   \brief         Buzzer - show both fields, with a marker on the one the knob edits
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
STATIC void menu_nav_draw_buzzer( void )
{
    char line[MENU_NAV_LINE_CHARS];

    HMI_SH1106_draw_text( 0u, 0u, "BUZZER", TRUE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%c State %s",
                         ( menu_nav_buzzer_field_s == MENU_NAV_BUZZER_FIELD_STATE ) ? '>' : ' ',
                         ( menu_nav_buzzer_enabled_s == TRUE ) ? "ON" : "OFF" );
    HMI_SH1106_draw_text( 2u, 0u, line, TRUE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "%c Time  %u ms",
                         ( menu_nav_buzzer_field_s == MENU_NAV_BUZZER_FIELD_TIME ) ? '>' : ' ',
                         (unsigned int)menu_nav_beep_time_ms_s );
    HMI_SH1106_draw_text( 3u, 0u, line, TRUE );

    HMI_SH1106_draw_text( 6u, 0u, "SEL field, CFM keep", TRUE );
    HMI_SH1106_draw_text( 7u, 0u, "BACK cancels", TRUE );
}

/*!
****************************************************************************************************
*
*   \brief         Buzzer - the knob edits whichever field SELECT last landed on
*
*   \author        MS
*
*   \param         input - What the panel reported
*
*   \return        none
*
*   \note          SELECT swaps which field the knob edits rather than navigating, so CONFIRM is the
*                  only way to leave and keep - a shape this screen needs precisely because it has
*                  two fields, unlike every single-value editor elsewhere that overloads SELECT and
*                  CONFIRM the same way. CW always means ON for the state field rather than toggling
*                  back and forth - a fast spin lands where it visibly stopped either way.
*
***************************************************************************************************/
STATIC void menu_nav_handle_buzzer( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:
            if( menu_nav_buzzer_field_s == MENU_NAV_BUZZER_FIELD_STATE )
            {
                menu_nav_buzzer_enabled_s = TRUE;
            }
            else if( menu_nav_beep_time_ms_s <= (u16_t)( MENU_NAV_BEEP_TIME_MAX_MS - MENU_NAV_BEEP_TIME_STEP_MS ) )
            {
                menu_nav_beep_time_ms_s += MENU_NAV_BEEP_TIME_STEP_MS;
            }
            else
            {
                /* Already at MENU_NAV_BEEP_TIME_MAX_MS - nothing to step */
            }
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_CCW:
            if( menu_nav_buzzer_field_s == MENU_NAV_BUZZER_FIELD_STATE )
            {
                menu_nav_buzzer_enabled_s = FALSE;
            }
            else if( menu_nav_beep_time_ms_s >= (u16_t)( MENU_NAV_BEEP_TIME_MIN_MS + MENU_NAV_BEEP_TIME_STEP_MS ) )
            {
                menu_nav_beep_time_ms_s -= MENU_NAV_BEEP_TIME_STEP_MS;
            }
            else
            {
                /* Already at MENU_NAV_BEEP_TIME_MIN_MS - nothing to step */
            }
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_SELECT:
            menu_nav_buzzer_field_s = ( menu_nav_buzzer_field_s == MENU_NAV_BUZZER_FIELD_STATE )
                                     ? MENU_NAV_BUZZER_FIELD_TIME : MENU_NAV_BUZZER_FIELD_STATE;
            HMI_SH1106_request_redraw();
        break;

        case HMI_SH1106_INPUT_CONFIRM:
            /* Keep both - they are already live, there is nothing to commit */
            MENU_NAV_goto( menu_nav_screens_s[MENU_NAV_SCREEN_BUZZER].back_screen );
        break;

        case HMI_SH1106_INPUT_BACK:
            menu_nav_buzzer_enabled_s = menu_nav_buzzer_saved_s;
            menu_nav_beep_time_ms_s   = menu_nav_beep_time_saved_s;
            MENU_NAV_goto( menu_nav_screens_s[MENU_NAV_SCREEN_BUZZER].back_screen );
        break;

        case HMI_SH1106_INPUT_BACK_LONG:
            /* MENU_NAV_on_input() navigates home right after this returns - just revert */
            menu_nav_buzzer_enabled_s = menu_nav_buzzer_saved_s;
            menu_nav_beep_time_ms_s   = menu_nav_beep_time_saved_s;
        break;

        default:
            /* Long select/confirm mean nothing while editing */
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Whether the panel should beep on a press
*
*   \author        MS
*
*   \return        Current Buzzer screen value
*
***************************************************************************************************/
false_true_et MENU_NAV_buzzer_enabled( void )
{
    return( menu_nav_buzzer_enabled_s );
}

/*!
****************************************************************************************************
*
*   \brief         How long that beep should last
*
*   \author        MS
*
*   \return        Current Beep Time screen value, in milliseconds
*
***************************************************************************************************/
u16_t MENU_NAV_get_beep_duration_ms( void )
{
    return( menu_nav_beep_time_ms_s );
}

/*!
****************************************************************************************************
*
*   \brief         The Fan Speed screen's current value
*
*   \author        MS
*
*   \return        Current Fan Speed screen value, 0-100
*
***************************************************************************************************/
u8_t MENU_NAV_get_fan_speed_pct( void )
{
    return( menu_nav_fan_speed_pct_s );
}

/*!
****************************************************************************************************
*
*   \brief         Set the Fan Speed screen's value from outside the knob path
*
*   \author        MS
*
*   \param         pct - Requested value, clamped to [MENU_NAV_FAN_SPEED_MIN, MENU_NAV_FAN_SPEED_MAX]
*
*   \return        none
*
*   \note          Written straight to menu_nav_fan_speed_pct_s, not through the pct editor's saved/
*                  revert value - a remote set is meant to stick, not be something BACK undoes. If
*                  the Fan Speed screen is open, the redraw request means the new value appears on
*                  screen immediately rather than waiting for the next knob turn.
*
***************************************************************************************************/
void MENU_NAV_set_fan_speed_pct( u8_t pct )
{
    if( pct > (u8_t)MENU_NAV_FAN_SPEED_MAX )
    {
        pct = (u8_t)MENU_NAV_FAN_SPEED_MAX;
    }
    else if( pct < (u8_t)MENU_NAV_FAN_SPEED_MIN )
    {
        pct = (u8_t)MENU_NAV_FAN_SPEED_MIN;
    }
    else
    {
        /* Already in range */
    }

    menu_nav_fan_speed_pct_s = pct;
    HMI_SH1106_request_redraw();
}

/****************************** END OF FILE *******************************************************/
