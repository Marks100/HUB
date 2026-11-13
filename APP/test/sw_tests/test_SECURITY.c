/*! \file
*               Author: mstewart
*   \brief      SecurityAccess (UDS 0x27) - software test
*
*   \details    A tester on CAN1 unlocks the ECU's security levels (TMD_SEC) and then asks for what each one gates: the key
*               writes (0x2E, DID_APP_KEY_1 needs level 2 and DID_APP_KEY_2 level 3) and the programming session (level 1). The
*               tests look at what the ECU answers, its negative response code (nrc_p of the model calls) when it refuses. The ECU
*               does not check the key yet (UDS_config.c), so a wrong key is not tested: the gates are. Every test case starts as
*               a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "APP_NVM_BLOCKS.h"
#include "BOOT_HANDOFF.h"
#include "UDS_config.h"

#include "TMD_INIT.h"
#include "TMD_SEC.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_LEVEL_COUNT    ( 3u )     /* the levels APP has */
#define TEST_SEED_COUNT     ( 10u )    /* the seeds requested back to back */

static const u8_t test_key_c[APP_KEY_LEN] = { [0 ... ( APP_KEY_LEN - 1u )] = 0x5Au };    /* a key to write */

static u8_t  test_response_s[8];           /* what the ECU answered a request sent as it is */
static u16_t test_response_length_s;

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
*   \brief         Verify every level the ECU has can be unlocked from the extended session
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_every_level_the_ecu_has_can_be_unlocked( void )
{
    u8_t level;
    u8_t session = 0u;

    for( level = 1u; level <= TEST_LEVEL_COUNT; level++ )
    {
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
        TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_EXTENDED, session );

        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( level, NULL_P ) );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Verify a level the ECU does not have is refused: the seed of level 4 is not given, and a key for it is not taken
*                  even after a seed, so nothing above level 3 can be unlocked
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_level_the_ecu_does_not_have_is_refused( void )
{
    const u8_t level_4_seed_request[2] = { TMD_UDS_SID_SECURITY_ACCESS, 0x07u };
    const u8_t level_1_seed_request[2] = { TMD_UDS_SID_SECURITY_ACCESS, 0x01u };
    const u8_t level_4_key_request[6]  = { TMD_UDS_SID_SECURITY_ACCESS, 0x08u, 0x01u, 0x02u, 0x03u, 0x04u };
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( level_4_seed_request, sizeof( level_4_seed_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( level_1_seed_request, sizeof( level_1_seed_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( level_4_key_request, sizeof( level_4_key_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), &nrc ) );    /* and nothing was granted */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify SecurityAccess is only there in the extended session: in the default session the seed is refused
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_security_access_is_refused_in_the_default_session( void )
{
    u8_t nrc     = 0u;
    u8_t session = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_DEFAULT, session );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_SEC_unlock( 1u, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a key with no seed requested before it is a request sequence error
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_key_without_a_seed_is_a_request_sequence_error( void )
{
    const u8_t key_request[6] = { TMD_UDS_SID_SECURITY_ACCESS, 0x02u, 0x01u, 0x02u, 0x03u, 0x04u };
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( key_request, sizeof( key_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_SEQUENCE_ERROR, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a key of the wrong length is refused as an incorrect length, and the level is not granted
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_key_of_the_wrong_length_is_refused( void )
{
    const u8_t seed_request[2] = { TMD_UDS_SID_SECURITY_ACCESS, 0x03u };
    const u8_t key_request[4]  = { TMD_UDS_SID_SECURITY_ACCESS, 0x04u, 0x01u, 0x02u };    /* 2 bytes, not 4 */
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( seed_request, sizeof( seed_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( key_request, sizeof( key_request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_INCORRECT_LENGTH, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify seeds requested back to back are all different: ten requests in a row each get a seed (a positive response of
*                  the sub-function and 4 bytes), and no two of the ten are the same, so the seed is not a constant and does not repeat
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_seeds_requested_back_to_back_are_all_different( void )
{
    const u8_t request[2] = { TMD_UDS_SID_SECURITY_ACCESS, 0x01u };
    u32_t      seed[TEST_SEED_COUNT];
    u8_t       i;
    u8_t       j;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    for( i = 0u; i < TEST_SEED_COUNT; i++ )
    {
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, sizeof( request ), test_response_s, sizeof( test_response_s ), &test_response_length_s, NULL_P ) );
        TEST_ASSERT_EQUAL_UINT16( 2u + TMD_SEC_KEY_LEN, test_response_length_s );
        TEST_ASSERT_EQUAL_HEX8( TMD_UDS_SID_SECURITY_ACCESS + TMD_UDS_POSITIVE_RESPONSE_OFFSET, test_response_s[0] );
        TEST_ASSERT_EQUAL_HEX8( 0x01u, test_response_s[1] );

        seed[i] = STDC_copy_buffer_msb_first_to_32bit( &test_response_s[2] );

        for( j = 0u; j < i; j++ )
        {
            TEST_ASSERT_NOT_EQUAL_MESSAGE( seed[j], seed[i], "two seeds requested back to back are the same" );
        }
    }
}

/*!
****************************************************************************************************
*
*   \brief         Verify the key writes are refused until the level of their key is unlocked: key 1 needs level 2 and key 2 level 3,
*                  and a level below is not enough
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_key_write_needs_the_level_of_its_key( void )
{
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), &nrc ) );    /* nothing unlocked */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), &nrc ) );    /* level 1 is not 2 */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 2u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_2, test_key_c, sizeof( test_key_c ), &nrc ) );    /* level 2 is not 3 */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 3u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_data_by_identifier( DID_APP_KEY_2, test_key_c, sizeof( test_key_c ), NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a higher level also opens what the levels below it gate: level 3 writes key 1
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_higher_level_opens_what_a_lower_one_gates( void )
{
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 3u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify going back to the default session takes the level away: after it and the extended session again, the
*                  key write is refused as it was
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_default_session_takes_the_level_away( void )
{
    u8_t nrc     = 0u;
    u8_t session = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 2u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_DEFAULT, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_DEFAULT, session );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_EXTENDED, session );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_data_by_identifier( DID_APP_KEY_1, test_key_c, sizeof( test_key_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the programming session needs level 1: refused with the ECU locked, and with level 1 it takes the
*                  request, which asks BM for the FBL at the next start
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_programming_session_needs_level_1( void )
{
    u8_t nrc     = 0u;
    u8_t session = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_PROGRAMMING, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_EXTENDED, session );    /* refused: still in it */
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_SEC_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_PROGRAMMING, NULL_P ) );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );    /* the ECU resets into the FBL after it, so there is no session left to read */
}

/****************************** END OF FILE *******************************************************/
