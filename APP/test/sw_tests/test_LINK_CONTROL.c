/*! \file
*               Author: mstewart
*   \brief      LinkControl (UDS 0x87) - software test
*
*   \details    A tester on CAN1 asks the ECU to go to the bootloader at another CAN baud rate: 0x87 0x01 and a baud rate parameter, 0x11 (125 kbit/s),
*               0x12 (250) or 0x13 (500). The ECU takes it in the extended session at security level 1. It answers a response pending (0x78) at the
*               baud rate it has, and hands over: the baud rate, the response the FBL owes (the positive response, which the FBL sends at the new
*               baud rate) and the FBL request go into the handoff RAM, and it resets once the 0x78 is out. The FBL model (the project's test stubs)
*               takes the baud rate and the response owed at the start that follows, as the FBL does, and clears them. The tests look at what
*               the ECU answers and at the handoff. What the real FBL sends at the new baud rate is not here to see, the model does not run
*               its comms. Every test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "BOOT_HANDOFF.h"
#include "HAL_CAN.h"

#include "vhw.h"

#include "TMD_CANTP.h"
#include "TMD_FBL.h"
#include "TMD_INIT.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_LINK_CONTROL_SID           ( 0x87u )
#define TEST_VERIFY_SUBFUNC             ( 0x01u )    /* verifyModeTransitionWithFixedParameter */
#define TEST_TRANSITION_SUBFUNC         ( 0x03u )    /* transitionMode */
#define TEST_RESPONSE_PENDING_NRC       ( 0x78u )
#define TEST_RESET_SETTLE_MS            ( 50u )      /* from the 0x78 to the reset: the confirmation of the response and the grace after it */

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

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Send a LinkControl request and say what the ECU refused it with
*
*   \author        mstewart
*
*   \param[in]     request_p   The request, SID first
*   \param[in]     length      Its length
*
*   \return        The negative response code, 0 if the ECU did not answer with a negative response (a response pending is the answer here)
*
*****************************************************************************************************/
static u8_t send_link_control( const u8_t* request_p, u16_t length )
{
    u8_t  response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t response_length = 0u;
    u8_t  nrc             = 0u;

    TMD_CANTP_set_wait_for_pending( FALSE );    /* the ECU says 0x78 and resets: the FBL's own response, after the reset, is not modelled, so the 0x78 is the answer */

    (void)TMD_UDS_send_request( request_p, length, response, (u16_t)sizeof( response ), &response_length, &nrc );

    return( nrc );
}

/*!
****************************************************************************************************
*
*   \brief         A LinkControl request for a baud rate, taken: the ECU says response pending, hands over in its one reset, and the FBL
*                  takes the baud rate and the response it owes at the start that follows
*
*   \author        mstewart
*
*   \param[in]     baud_param   The baud rate parameter
*
*   \return        none
*
*****************************************************************************************************/
static void assert_a_baud_rate_hands_over( u8_t baud_param )
{
    const u8_t request[3] = { TEST_LINK_CONTROL_SID, TEST_VERIFY_SUBFUNC, baud_param };
    u32_t      resets_before;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    resets_before = vhw_core_get_system_resets();

    TEST_ASSERT_EQUAL_HEX8( TEST_RESPONSE_PENDING_NRC, send_link_control( request, (u16_t)sizeof( request ) ) );
    TMD_TIME_advance_time_ms( TEST_RESET_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT32( resets_before + 1u, vhw_core_get_system_resets() );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL_HEX8( baud_param, BOOT_HANDOFF_get_baud_param() );
    TEST_ASSERT_EQUAL_UINT8( BOOT_HANDOFF_RESPONSE_LINK_CONTROL, BOOT_HANDOFF_get_response_owed() );

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_HEX8( baud_param, TMD_FBL_get_baud_param() );    /* what the FBL found at its start */
    TEST_ASSERT_EQUAL_UINT8( BOOT_HANDOFF_RESPONSE_LINK_CONTROL, TMD_FBL_get_response_owed() );
    TEST_ASSERT_EQUAL_UINT8( 0u, BOOT_HANDOFF_get_baud_param() );    /* and took: nothing is left for the start after it */
    TEST_ASSERT_EQUAL_UINT8( BOOT_HANDOFF_RESPONSE_NONE, BOOT_HANDOFF_get_response_owed() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify 0x87 0x01 0x11 hands over to the FBL at 125 kbit/s
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_link_control_for_125_kbit_hands_over_at_that_baud_rate( void )
{
    assert_a_baud_rate_hands_over( HAL_CAN_BAUD_PARAM_125K );
}

/*!
****************************************************************************************************
*
*   \brief         Verify 0x87 0x01 0x12 hands over to the FBL at 250 kbit/s
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_link_control_for_250_kbit_hands_over_at_that_baud_rate( void )
{
    assert_a_baud_rate_hands_over( HAL_CAN_BAUD_PARAM_250K );
}

/*!
****************************************************************************************************
*
*   \brief         Verify 0x87 0x01 0x13 hands over to the FBL at 500 kbit/s
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_link_control_for_500_kbit_hands_over_at_that_baud_rate( void )
{
    assert_a_baud_rate_hands_over( HAL_CAN_BAUD_PARAM_500K );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a start that is not a LinkControl hand-over has no baud rate and nothing owed
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_start_that_is_not_a_link_control_hand_over_owes_nothing( void )
{
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL_UINT8( 0u, BOOT_HANDOFF_get_baud_param() );
    TEST_ASSERT_EQUAL_UINT8( BOOT_HANDOFF_RESPONSE_NONE, BOOT_HANDOFF_get_response_owed() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a baud rate that is none of the three is refused, and the ECU neither resets nor hands anything over
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_baud_rate_the_ecu_cannot_run_at_is_refused_and_nothing_is_handed_over( void )
{
    const u8_t request[3] = { TEST_LINK_CONTROL_SID, TEST_VERIFY_SUBFUNC, 0x14u };
    u32_t      resets_before;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    resets_before = vhw_core_get_system_resets();

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, send_link_control( request, (u16_t)sizeof( request ) ) );
    TMD_TIME_advance_time_ms( TEST_RESET_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT32( resets_before, vhw_core_get_system_resets() );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL_UINT8( 0u, BOOT_HANDOFF_get_baud_param() );
    TEST_ASSERT_EQUAL_UINT8( BOOT_HANDOFF_RESPONSE_NONE, BOOT_HANDOFF_get_response_owed() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a request with no baud rate, and one with more than it, are refused for their length
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_of_the_wrong_length_is_refused( void )
{
    const u8_t too_short[2] = { TEST_LINK_CONTROL_SID, TEST_VERIFY_SUBFUNC };
    const u8_t too_long[4]  = { TEST_LINK_CONTROL_SID, TEST_VERIFY_SUBFUNC, HAL_CAN_BAUD_PARAM_500K, 0x00u };

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_INCORRECT_LENGTH, send_link_control( too_short, (u16_t)sizeof( too_short ) ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_INCORRECT_LENGTH, send_link_control( too_long, (u16_t)sizeof( too_long ) ) );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify transitionMode (0x03) is a request sequence error: the transition is the FBL's, and there is no verify before it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_transition_mode_is_a_request_sequence_error( void )
{
    const u8_t request[2] = { TEST_LINK_CONTROL_SID, TEST_TRANSITION_SUBFUNC };

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_REQUEST_SEQUENCE_ERROR, send_link_control( request, (u16_t)sizeof( request ) ) );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a sub-function that is neither 0x01 nor 0x03 is not supported
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_unknown_link_control_subfunction_is_not_supported( void )
{
    const u8_t request[3] = { TEST_LINK_CONTROL_SID, 0x02u, HAL_CAN_BAUD_PARAM_500K };

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED, send_link_control( request, (u16_t)sizeof( request ) ) );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the default session is refused, and the extended session without the security access too: the FBL starts unlocked
*                  on the strength of what the application asked for before it handed over
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_link_control_without_the_session_and_the_security_access_is_refused( void )
{
    const u8_t request[3] = { TEST_LINK_CONTROL_SID, TEST_VERIFY_SUBFUNC, HAL_CAN_BAUD_PARAM_500K };

    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION, send_link_control( request, (u16_t)sizeof( request ) ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    TEST_ASSERT_EQUAL_HEX8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, send_link_control( request, (u16_t)sizeof( request ) ) );

    TMD_TIME_advance_time_ms( TEST_RESET_SETTLE_MS );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL_UINT8( 0u, BOOT_HANDOFF_get_baud_param() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify HAL_CAN names exactly the three baud rates LinkControl takes
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_can_driver_runs_at_125_250_and_500_kbit_and_no_other_parameter( void )
{
    u16_t parameter;

    TEST_ASSERT_EQUAL( TRUE, HAL_CAN_baud_is_supported( HAL_CAN_BAUD_PARAM_125K ) );
    TEST_ASSERT_EQUAL( TRUE, HAL_CAN_baud_is_supported( HAL_CAN_BAUD_PARAM_250K ) );
    TEST_ASSERT_EQUAL( TRUE, HAL_CAN_baud_is_supported( HAL_CAN_BAUD_PARAM_500K ) );

    for( parameter = 0u; parameter < 0x100u; parameter++ )
    {
        if( ( parameter < HAL_CAN_BAUD_PARAM_125K ) || ( parameter > HAL_CAN_BAUD_PARAM_500K ) )
        {
            TEST_ASSERT_EQUAL( FALSE, HAL_CAN_baud_is_supported( (u8_t)parameter ) );
        }
    }
}

/****************************** END OF FILE *******************************************************/
