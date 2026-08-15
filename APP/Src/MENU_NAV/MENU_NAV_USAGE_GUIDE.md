# MENU_NAV / HMI_SH1106 Usage Guide
## How the OLED panel and its screens fit together

---

## Overview

The panel is built from two modules with one job each, and neither knows the other exists beyond
a handful of function pointers.

- **`HMI_SH1106`** (`xCOMMON_MODULES/Src/HMI/`) turns hardware into input events and draws
  whatever it is told to draw. It has no idea what a "screen" is.
- **`MENU_NAV`** (`APP/Src/MENU_NAV/`) owns every screen, what's on it, and where each input
  takes you. It has no idea what an I2C bus is.

The board (`INTEGRATION_STUBS.c` in this project) is the only thing that knows both exist - it
supplies `HMI_SH1106` with a config struct pointing back at `MENU_NAV`'s two entry points.

```
buttons/encoder --> HMI_SH1106 --> on_input_func_p --> MENU_NAV_on_input() --> screen table
                                                                                     |
     OLED <-------- HMI_SH1106 <-------- draw_func_p <-------- MENU_NAV_draw() ----+
```

---

## The Two Layers

### HMI_SH1106 - the panel

Everything it does fits in two sentences: it reports one of eight things just happened
(`HMI_SH1106_input_et` - CW, CCW, SELECT, SELECT_LONG, CONFIRM, CONFIRM_LONG, BACK, BACK_LONG),
and when a repaint is due it clears the framebuffer and calls `draw_func_p`. That's the entire
surface a navigation layer has to work with.

Drawing is two stateless primitives, callable only from inside `draw_func_p`:

```c
void HMI_SH1106_draw_text( u8_t row, u8_t col, const char* text_p );
void HMI_SH1106_draw_list( const HMI_SH1106_list_st* list_p );
```

`draw_list` takes the cursor position and scroll offset as arguments - it does not remember them
between calls, because it isn't the thing deciding where they are. Whoever owns navigation is.

A repaint doesn't just happen because you asked - it happens on the next tick after you asked:

```c
void HMI_SH1106_request_redraw( void );   /* test-and-clear, coalesces bursts into one repaint */
```

`HMI_SH1106_cfg_st.refresh_period_ms` also forces a repaint on a timer, independent of any
request - useful for a screen showing a value that changes on its own (a counter, a sensor
reading) without the navigation layer having to notice and ask for a redraw itself.

### MENU_NAV - the screens

A **flat table**, one row per screen, indexed by an enum. No navigation stack:

```c
typedef struct
{
    const MENU_NAV_list_st* list_p;              /* set this OR draw_func_p, not both */
    void (*draw_func_p)( void );
    void (*handle_func_p)( HMI_SH1106_input_et ); /* optional - overrides default input handling */
    void (*on_enter_func_p)( void );              /* optional - runs as this screen becomes current */
    MENU_NAV_screen_et back_screen;               /* where BACK goes */
} MENU_NAV_screen_st;
```

Every screen names its own `back_screen` explicitly. There's no depth counter and nothing to push
or pop, which also means there's no way for two parts of the code to disagree about "how deep" the
user is - there is no depth, only "which row is current."

The trade: a screen reachable from two different parents has one `back_screen`, not two. The
escape hatch is a **long BACK press**, which always jumps to the root screen from anywhere and
cannot be intercepted by any screen (see Gotchas).

---

## Quick Start - Wiring the Panel

The board layer builds one `HMI_SH1106_cfg_st` and points its two callbacks at `MENU_NAV`:

```c
STATIC void my_on_input( HMI_SH1106_input_et input )
{
    MENU_NAV_on_input( input );   /* add board policy here first if you want it - a beep, an LED */
}

const HMI_SH1106_cfg_st panel_cfg_s =
{
    /* ... transport, pins, timing - see HMI_SH1106_cfg_st for the full list ... */
    .on_input_func_p = my_on_input,
    .draw_func_p     = MENU_NAV_draw,
};

void app_init( void )
{
    HMI_SH1106_init( &panel_cfg_s );   /* must run first - the pending initial repaint needs a screen */
    MENU_NAV_init();
}
```

`MENU_NAV_init()` selects the root screen and requests a redraw, so the first tick after startup
paints it without anything else asking.

---

## Adding a Screen

Three shapes, in increasing order of how much control you need.

### 1. A list screen (no code beyond the data)

```c
STATIC const MENU_NAV_item_st my_list_items_s[] =
{
    { "Option A", MY_SCREEN_A },
    { "Option B", MY_SCREEN_B },
};

STATIC const MENU_NAV_list_st my_list_s =
{
    .title_p    = "MY MENU",
    .items_p    = my_list_items_s,
    .item_count = MENU_NAV_ITEM_COUNT( my_list_items_s ),
};
```

Add a table row with just `.list_p` and `.back_screen` set - cursor, scrolling, the marker, and
the up/down indicators all come free. The encoder moves the cursor; SELECT/CONFIRM open the
row's `target`; BACK goes to `back_screen`.

### 2. A free-form screen

```c
STATIC void my_draw_screen( void )
{
    HMI_SH1106_draw_text( 0u, 0u, "Whatever you like" );
}
```

Set `.draw_func_p` and `.back_screen`, leave `.list_p` NULL_P. Default input handling: SELECT,
CONFIRM and BACK all go to `back_screen` - any button dismisses it, which suits a read-only page
with nothing to select.

### 3. A screen with its own input handling

For anything that isn't "navigate on press" - a value editor, a screen that needs SELECT to mean
something other than "go somewhere":

```c
STATIC void my_handle_screen( HMI_SH1106_input_et input )
{
    switch( input )
    {
        case HMI_SH1106_INPUT_CW:   /* do something */          break;
        case HMI_SH1106_INPUT_CCW:  /* do something else */      break;
        case HMI_SH1106_INPUT_BACK: MENU_NAV_goto( back_screen ); break;
        default: break;
    }
}
```

Set `.handle_func_p` and this function owns input entirely for that screen - the defaults above
no longer apply. See `menu_nav_handle_brightness()` in `MENU_NAV.c` for a complete example: the
knob edits a value in place, SELECT/CONFIRM commit it, BACK reverts to what it was on entry.

---

## How a Button Press Reaches the Screen

```
BTN_MGR/ROTARY_MGR detect the press
        |
HMI_SH1106 counts it, calls cfg->on_input_func_p( input )
        |
board wrapper (adds board policy - a beep, ...), calls MENU_NAV_on_input( input )
        |
MENU_NAV_on_input():
    input == BACK_LONG?  -->  MENU_NAV_goto( root screen )         [always, no screen can stop this]
    else screen has handle_func_p?  -->  that function decides
    else screen is a list?  -->  encoder moves cursor, SELECT/CONFIRM open the row, BACK -> back_screen
    else  -->  SELECT/CONFIRM/BACK all go to back_screen
```

`MENU_NAV_goto()` is the entire navigation mechanism (from `MENU_NAV.c`, bounds check omitted
above for brevity - the real thing also checks `screen < MENU_NAV_NUM_SCREENS` before any of this):

```c
menu_nav_screen_s = screen;
if( menu_nav_screens_s[screen].on_enter_func_p != NULL_P )
{
    menu_nav_screens_s[screen].on_enter_func_p();
}
HMI_SH1106_request_redraw();
```

One assignment, one optional callback, one redraw request. Going to the screen already showing is
not a special case - it re-runs `on_enter_func_p` and repaints, which is the right thing for
re-entering a screen deliberately.

---

## Gotchas

### A screen that is its own `back_screen` needs its own `handle_func_p`

The root screen's `back_screen` is itself, by construction (BACK has nowhere else to go). If that
screen is free-form with no `handle_func_p`, the default behaviour - "SELECT/CONFIRM/BACK go to
`back_screen`" - sends every press straight back to the screen already showing. The button press
is genuinely detected (counted, any board-level feedback fires), but nothing visibly happens,
because "navigate to `back_screen`" and "navigate to yourself" are the same call. Any screen where
`back_screen` points at itself needs a `handle_func_p` that does something other than the default.

### Long BACK bypasses every screen, including one mid-edit

`MENU_NAV_on_input()` checks `BACK_LONG` before consulting the current screen at all - no
`handle_func_p` ever sees it. That's deliberate (it's the guaranteed way out of anywhere), but it
means a screen like a value editor that reverts state on a *short* BACK does not get the chance to
revert on a *long* one - the screen changes, whatever was mid-edit is left as it last was, not
rolled back. If a screen's edits need to survive a long BACK too, that has to be handled by making
the change take effect immediately (as the brightness screen does - it pushes the value to
hardware on every step, so there is nothing left to "commit" or "lose") rather than deferred to an
explicit commit step.

### An unfilled screen table row is silently blank, not a compile error

The table is indexed by the enum and built with designated initializers, so adding an enumerator
without adding its row does not fail to compile - it leaves every field zero: `list_p` and
`draw_func_p` both NULL_P (nothing drawn), `back_screen` defaulting to whatever enumerator is 0
(usually the root). `MENU_NAV_goto()` still accepts the index, still redraws - onto a blank
screen. Add the enumerator and its row together.

### Changing what's on screen outside the normal input path still needs a redraw request

`MENU_NAV_goto()` and the cursor-moving path both call `HMI_SH1106_request_redraw()` for you.
Anything else that changes what a screen should show - a `handle_func_p` mutating state without
navigating anywhere, a value pushed in from outside the input path entirely - has to call it too,
or the change sits correct in memory but never reaches the glass until something else happens to
trigger a repaint.

### `refresh_period_ms` is global, not per-screen

One timer in `HMI_SH1106_cfg_st` forces a repaint for every screen alike. A screen with genuinely
live data (a status page reading counters) relies on this to stay current; a screen with nothing
that changes on its own repaints exactly as often as everything else does, which costs nothing
since `draw_func_p` painting the same pixels twice is not observable - but it's worth knowing
there's no way to give one screen a faster or slower refresh than the rest without adding that
mechanism.

---

## Common Patterns

**Value editor** - `on_enter_func_p` captures the starting value, `handle_func_p` applies each
step immediately (so the display and the hardware never disagree), BACK restores the captured
value. See `menu_nav_enter_brightness()` / `menu_nav_handle_brightness()`.

**Live-data screen** - draw function reads live state fresh every call (never a snapshot taken on
entry), relies on `refresh_period_ms` to be called again periodically. See
`menu_nav_draw_status()`.

**Shared placeholder** - several list rows point at the same "not implemented yet" screen. When
one becomes real, only that row's `target` changes.

---

## API Reference

### HMI_SH1106 (`xCOMMON_MODULES/Src/HMI/HMI_SH1106.h`)
- `HMI_SH1106_init( cfg_p )` / `HMI_SH1106_tick()` - lifecycle, call `_tick()` at `cfg->tick_rate_ms`
- `HMI_SH1106_gpio_interrupt()` - software GPIO encoder mode only, call from the pin A ISR
- `HMI_SH1106_request_redraw()` - ask for a repaint on the next tick, test-and-clear
- `HMI_SH1106_get_stats( stats_p )` - lifetime counts, indexed by `HMI_SH1106_input_et`
- `HMI_SH1106_set_brightness_pct( pct )` - hardware contrast, independent of the framebuffer
- `HMI_SH1106_draw_text( row, col, text_p )` / `HMI_SH1106_draw_list( list_p )` - only valid inside `draw_func_p`

### MENU_NAV (`APP/Src/MENU_NAV/MENU_NAV.h`)
- `MENU_NAV_init()` - selects the root screen, call once after `HMI_SH1106_init()`
- `MENU_NAV_on_input( input )` / `MENU_NAV_draw()` - wire straight into `HMI_SH1106_cfg_st`
- `MENU_NAV_goto( screen )` - the entire navigation mechanism, also callable from outside the
  panel (jumping to a fault screen when something else in the system goes wrong, for instance)

---

**For the full reasoning behind the design (why a flat table instead of a stack, why screens
rather than menus), see the file header comments in `MENU_NAV.h` and `HMI_SH1106.h` - this guide
covers how to use it, those cover why it's built this way.**
