/*! \file
*               Author: mstewart
*   \brief      Software tests - how every test starts the ECU
*
*   \details    The one TMD_INIT_cfg_st the software tests share: the virtual STM32 powered on, the clock and the CAN
*               models wired to it, a blank flash and then the real app_main(). setUp() of a test is
*               TMD_INIT_init( &sw_test_init_cfg_g ), so every test case begins as a virgin ECU.
*
*               The includes of a software test are in this order, in blocks, so the tests read the same: the test framework
*               (unity.h, test_timing.h), STDC.h, the firmware (INTEGRATION_STUBS.h, HAL_config.h, then its modules A to Z),
*               the virtual STM32 (vhw.h, then stm32f10x_*.h A to Z), the TMD models A to Z, and this header last.
*/
#ifndef SW_TEST_STUBS_H
#define SW_TEST_STUBS_H

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "TMD_INIT.h"
#include "TMD_CAN.h"
#include "TMD_SH1106.h"

/***************************************************************************************************
**                                       Definitions                                              **
***************************************************************************************************/
#define TEST_BUS_TESTER          ( TMD_CAN_INSTANCE_0 )    /* CAN1: bxCAN through the TJA1051 */
#define TEST_BUS_NODES           ( TMD_CAN_INSTANCE_1 )    /* CAN2: the MCP2515 */

/* The tester on CAN1 is the UDS tester model (TMD_UDS, over TMD_CANTP), started with every other model: TMD_UDS_send_request() and the
   services on it. It starts at the ECU's own UDS server; TMD_CANTP_set_ids() points it at another node. */
#define SW_TEST_ECU_UDS_REQUEST_ID      ( 0x7E0u )  /* the ECU's physical request, APP_CAN_RX_ID in INTEGRATION_STUBS.c */
#define SW_TEST_ECU_UDS_RESPONSE_ID     ( 0x7E8u )  /* its answer, APP_CAN_TX_ID */
#define SW_TEST_TESTER_TIMEOUT_MS       ( 500u )    /* how long the tester waits for the whole answer, the first frame to the last */
#define SW_TEST_TESTER_RESPONSE_MAX_LEN ( 128u )    /* what a test's buffer for an answer has to hold */

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
**                                    Exported Globals                                            **
***************************************************************************************************/
extern const TMD_INIT_cfg_st sw_test_init_cfg_g;    /* power on, models, blank flash, app_main(): a virgin ECU */

/***************************************************************************************************
**                                    Function Prototypes                                        **
***************************************************************************************************/
/* A test puts an ERX on the bus with TMD_ERX_place_instance( APP_ISO_..., TMD_ERX_PORT_A or B ), and takes it off and puts it back
   with TMD_ERX_unplug_instance() / TMD_ERX_plug_in_instance() on TMD_ERX_find_instance( APP_ISO_... ). The HUB powers it. */

/* The HUB's panel as a user works it: the three push buttons are pins of the virtual STM32 and the knob is the count of TIM4, which the
   firmware's encoder reads. What the user sees is the OLED, the TMD_SH1106 model on I2C1. A test presses, turns, and looks at the screen. */
void        SW_TEST_PANEL_press( SW_TEST_PANEL_button_et button );    /* press and release: a short press */
void        SW_TEST_PANEL_turn_knob_clockwise( void );                /* one detent */
void        SW_TEST_PANEL_turn_knob_to( const char* text_p );         /* turn the knob until the cursor of the list is on that row, or the list has gone round */
const char* SW_TEST_PANEL_screen_message( const char* what_p );       /* what was looked for and what the screen shows, kept until the next call: the message of a failed assertion */

/* The assertions are macros, so a failure names the line of the test, and the library needs no Unity */
#define SW_TEST_PANEL_assert_on_screen( text_p )        TEST_ASSERT_TRUE_MESSAGE( TMD_SH1106_find_text( ( text_p ), NULL_P, NULL_P ), SW_TEST_PANEL_screen_message( text_p ) )
#define SW_TEST_PANEL_assert_not_on_screen( text_p )    TEST_ASSERT_FALSE_MESSAGE( TMD_SH1106_find_text( ( text_p ), NULL_P, NULL_P ), SW_TEST_PANEL_screen_message( text_p ) )
#define SW_TEST_PANEL_assert_cursor_on( text_p )        TEST_ASSERT_TRUE_MESSAGE( TMD_SH1106_find_marked_text( ( text_p ), NULL_P ), SW_TEST_PANEL_screen_message( text_p ) )    /* the row is on the screen with the cursor in front of it */
#define SW_TEST_PANEL_move_the_cursor_to( text_p )      do { SW_TEST_PANEL_turn_knob_to( text_p ); SW_TEST_PANEL_assert_cursor_on( text_p ); } while( 0 )

#endif /* SW_TEST_STUBS_H */

/****************************** END OF FILE *******************************************************/
