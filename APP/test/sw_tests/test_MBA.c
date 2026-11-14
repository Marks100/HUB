/*! \file
*               Author: mstewart
*   \brief      WriteMemoryByAddress (UDS 0x3D) - software test
*
*   \details    A tester on CAN1 asks the ECU to write flash at an address (TMD_UDS_write_memory_by_address(), over the UDS tester
*               model). The ECU takes the request in the extended session at security level 1, for the variant parameter page of
*               the flash only, and it does not write the flash while it runs: it keeps the write, and at its next start hands over to
*               the VPM server (in the bootloader), which rewrites the page. The tests look at what the ECU answers, at the flash, and at the hand-over
*               (the FBL request in the handoff RAM). The VPM server is the MBA model, standing in for the bootloader it is in, which runs before the application at every
*               start that follows a hand-over, as BM and the FBL do (the project's test stubs, TMD_INIT): so a write is made by the
*               second restart after it, the first being the application's hand-over. Every test case starts as a virgin ECU (setUp,
*               SW_TEST_STUBS.c).
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "BOOT_HANDOFF.h"
#include "vhw.h"
#include "FLS_STM32F1.h"

#include "TMD_INIT.h"
#include "TMD_MBA.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_VPM_ADDRESS                ( 0x0801E400ul )    /* the variant parameter page: one page, 1 KB */
#define TEST_PARAMETER_ADDRESS          ( TEST_VPM_ADDRESS + 0x20u )    /* a parameter in it */
#define TEST_DATASET0_ADDRESS           ( 0x0801E800ul )    /* DATASET0: the flash after the page, which a download writes and 0x3D does not */
#define TEST_NVM_ADDRESS                ( 0x0801F800ul )    /* the NVM: the flash after DATASET0 */
#define TEST_APP_ADDRESS                ( 0x08008000ul )    /* the first byte of APP's image */
#define TEST_OLD_BYTE                   ( 0xA5u )           /* what the page of a test held before the write */

static const u8_t test_data_c[4] = { 0x11u, 0x22u, 0x33u, 0x44u };

/*!
****************************************************************************************************
*
*   \brief         Check a range of the flash holds the data, with a message that shows which byte is wrong
*
*   \author        mstewart
*
*   \param[in]     address      The first byte of the range
*   \param[in]     expected_p   What it should hold
*   \param[in]     length       How many bytes, a page at most
*
*   \return        none
*
*****************************************************************************************************/
static void assert_flash_holds( u32_t address, const u8_t* expected_p, u16_t length )
{
    u8_t actual[FLS_STM32F1_PAGE_SIZE];

    TMD_MBA_flash_read( address, actual, length );
    TEST_ASSERT_EQUAL_UINT8_ARRAY( expected_p, actual, length );
}

/*!
****************************************************************************************************
*
*   \brief         The ECU starts and hands over, and starts again: the VPM server applies the writes the ECU kept
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The first restart is the application's: it finds the write kept (the VPM client) and asks BM for the FBL, in the handoff RAM. The
*                  second is BM's: the VPM server runs before the application, applies the write, notes it and clears the request, and
*                  the application starts without handing over.
*
*****************************************************************************************************/
static void apply_the_kept_writes( void )
{
    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );

    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         A write the ECU refused was not kept: the ECU does not hand over at its next start, and the page is as it was after
*                  the start that would have made the write
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void assert_the_refused_write_was_not_kept( void )
{
    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );

    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Once the writes have been made the ECU has nothing more to hand over: the next start goes to the application
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void assert_the_ecu_does_not_hand_over_again( void )
{
    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU whose variant parameter page holds something
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );
    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );    /* the variant parameter page holds something, in every test */

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write is kept and not made: the ECU says it has it, with a positive response that echoes the request,
*                  one write is waiting for the VPM server and the flash has not changed
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_is_kept_for_the_fbl_and_the_flash_is_not_written_yet( void )
{
    u8_t nrc = 0xFFu;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, nrc );

    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_PARAMETER_ADDRESS, TEST_OLD_BYTE, sizeof( test_data_c ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the next start hands over for the VPM server: the ECU asks BM for the FBL, with the write still waiting
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_next_start_hands_over_to_the_fbl( void )
{
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an ECU with no write waiting does not hand over: the next start asks for the application as usual
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_start_with_no_write_waiting_does_not_hand_over( void )
{
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the VPM server's write: the data is in the flash, and the rest of the page is what it was
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_fbl_writes_the_data_and_leaves_the_rest_of_the_page( void )
{
    u8_t expected[FLS_STM32F1_PAGE_SIZE];

    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );    /* the page held something */
    memset( expected, TEST_OLD_BYTE, sizeof( expected ) );
    memcpy( &expected[TEST_PARAMETER_ADDRESS - TEST_VPM_ADDRESS], test_data_c, sizeof( test_data_c ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    apply_the_kept_writes();

    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );    /* the data, and the rest of the page as it was */
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write is refused in the extended session without the security access, and nothing is kept
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_without_the_security_access_is_refused( void )
{
    u8_t nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );

    assert_the_refused_write_was_not_kept();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write is refused in the default session, and nothing is kept
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_in_the_default_session_is_refused( void )
{
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );

    assert_the_refused_write_was_not_kept();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write that cannot be made is refused as out of range, and nothing is kept: APP's own image, DATASET0, the
*                  NVM, a write that starts in the page and runs past its end, and one of more than
*                  VPM_COMMON_REQUEST_MAX bytes, which no number of slots holds
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_that_cannot_be_made_is_refused( void )
{
    const u8_t too_much[VPM_COMMON_REQUEST_MAX + 2u] = { 0u };
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_APP_ADDRESS, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_NVM_ADDRESS, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_DATASET0_ADDRESS, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_DATASET0_ADDRESS - 2u, test_data_c, sizeof( test_data_c ), &nrc ) );    /* two bytes in the page, two in DATASET0 */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, too_much, sizeof( too_much ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    assert_the_refused_write_was_not_kept();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write at an odd address and of an odd length is made: the VPM server rewrites the whole page from RAM, so
*                  nothing has to be on half-words, and the bytes on either side of it are what they were
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_at_an_odd_address_and_of_an_odd_length_is_made( void )
{
    const u8_t  odd_data[3] = { 0x5Au, 0x6Bu, 0x7Cu };
    const u32_t offset      = TEST_PARAMETER_ADDRESS + 1u - TEST_VPM_ADDRESS;
    u8_t        expected[FLS_STM32F1_PAGE_SIZE];

    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );
    memset( expected, TEST_OLD_BYTE, sizeof( expected ) );
    memcpy( &expected[offset], odd_data, sizeof( odd_data ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 1u, odd_data, sizeof( odd_data ), NULL_P ) );

    apply_the_kept_writes();

    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );
}

/*!
****************************************************************************************************
*
*   \brief         Verify several writes can be kept before the restart and the VPM server makes them all: three in the page, each with
*                  what the page held around it left alone
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_several_writes_kept_before_the_restart_are_all_made_by_the_fbl( void )
{
    const u8_t  second_data[2] = { 0x55u, 0x66u };
    const u8_t  third_data[2]  = { 0x77u, 0x88u };
    u8_t        expected[FLS_STM32F1_PAGE_SIZE];

    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );
    memset( expected, TEST_OLD_BYTE, sizeof( expected ) );
    memcpy( &expected[TEST_PARAMETER_ADDRESS - TEST_VPM_ADDRESS], test_data_c, sizeof( test_data_c ) );
    memcpy( &expected[TEST_PARAMETER_ADDRESS - TEST_VPM_ADDRESS + 0x100u], second_data, sizeof( second_data ) );
    memcpy( &expected[FLS_STM32F1_PAGE_SIZE - 2u], third_data, sizeof( third_data ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, second_data, sizeof( second_data ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS + FLS_STM32F1_PAGE_SIZE - 2u, third_data, sizeof( third_data ), NULL_P ) );    /* the last two bytes of the page */

    apply_the_kept_writes();

    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write of more than one slot holds is split over several and made whole: 120 bytes need several
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_of_many_bytes_is_split_over_slots_and_made_whole( void )
{
    u8_t        data[120];
    u8_t        i;

    for( i = 0u; i < sizeof( data ); i++ )
    {
        data[i] = (u8_t)( i + 1u );
    }

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), NULL_P ) );

    apply_the_kept_writes();

    assert_flash_holds( TEST_PARAMETER_ADDRESS, data, sizeof( data ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the later of two writes to the same bytes is what the flash ends up with
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_later_of_two_overlapping_writes_wins( void )
{
    const u8_t  other_data[4] = { 0xAAu, 0xBBu, 0xCCu, 0xDDu };

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, other_data, sizeof( other_data ), NULL_P ) );

    apply_the_kept_writes();

    assert_flash_holds( TEST_PARAMETER_ADDRESS, other_data, sizeof( other_data ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write is refused as conditions not correct when the free slots are too few for it, and what is kept is
*                  not touched: as many writes as there are slots use them all, so one more is refused, and one that needs every slot is refused
*                  with one already used
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_is_refused_when_the_free_slots_are_too_few( void )
{
    const u8_t  big_data[VPM_COMMON_REQUEST_MAX] = { 0x11u };
    u8_t        nrc     = 0u;
    u32_t       i;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, big_data, sizeof( big_data ), &nrc ) );    /* every slot, with one used */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_CONDITIONS_NOT_CORRECT, nrc );

    for( i = 1u; i < VPM_COMMON_PATCH_COUNT; i++ )
    {
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + ( 0x10u * i ), test_data_c, 2u, NULL_P ) );    /* the other slots */
    }

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, test_data_c, 2u, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_CONDITIONS_NOT_CORRECT, nrc );

    apply_the_kept_writes();
    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );    /* the first, and the refused ones left no trace */
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_PARAMETER_ADDRESS + 0x100u, TEST_OLD_BYTE, 2u ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify a request whose memorySize does not match the data it carries is refused as an incorrect length
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_size_that_does_not_match_the_data_is_refused( void )
{
    /* 3D, the format (a 2-byte size, a 4-byte address), the address, a size of 4 and only 2 bytes of data */
    const u8_t request[10] = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, TMD_UDS_ADDRESS_AND_LENGTH_FORMAT,
                               0x08u, 0x01u, 0xE4u, 0x20u, 0x00u, 0x04u, 0x11u, 0x22u };
    u8_t       response[8];
    u16_t      response_length = 0u;
    u8_t       nrc             = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( request, sizeof( request ), response, sizeof( response ), &response_length, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_INCORRECT_LENGTH, nrc );

    assert_the_refused_write_was_not_kept();
}

/*!
****************************************************************************************************
*
*   \brief         The power goes after the ECU has handed over, before the VPM server has touched the page: the handoff RAM is lost,
*                  so the ECU starts the application, which finds the write still kept and hands over again
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void power_goes_and_the_ecu_hands_over_again( void )
{
    TMD_INIT_power_cut_ecu( TMD_INIT_KEEP_ALL );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut before the VPM server gets to the page changes nothing: the page is as it was, the write is still
*                  kept, and the VPM server makes it when the ECU hands over again
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_before_the_vpm_server_makes_the_page_leaves_it_and_the_write_is_made_later( void )
{
    u8_t expected[FLS_STM32F1_PAGE_SIZE];

    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );
    memset( expected, TEST_OLD_BYTE, sizeof( expected ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TMD_INIT_restart_ecu();    /* the ECU hands over */

    power_goes_and_the_ecu_hands_over_again();
    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );

    TMD_INIT_restart_ecu();    /* the VPM server makes it */
    memcpy( &expected[TEST_PARAMETER_ADDRESS - TEST_VPM_ADDRESS], test_data_c, sizeof( test_data_c ) );
    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );
    assert_the_ecu_does_not_hand_over_again();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut between the page's erase and its program, which leaves the page blank - the one thing a power cut
*                  costs, as there is no second page to build it in - loses nothing that was kept: the next hand-over makes the data
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_that_leaves_the_page_blank_is_made_good_at_the_next_hand_over( void )
{
    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TMD_INIT_restart_ecu();

    TMD_INIT_power_cut_ecu( TMD_INIT_WIPE_MBA );    /* the page is blank */
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, 0xFFu, FLS_STM32F1_PAGE_SIZE ) );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );

    TMD_INIT_restart_ecu();    /* the VPM server makes it */
    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );
    assert_the_ecu_does_not_hand_over_again();
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut after the page is made but before the VPM server notes the batch leaves it pending: the next
*                  hand-over makes the page again, and notes it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_before_the_note_leaves_the_batch_pending_and_it_is_made_again( void )
{
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TMD_INIT_restart_ecu();

    TMD_MBA_flash_write( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );    /* the page is made, the note is not */
    power_goes_and_the_ecu_hands_over_again();

    TMD_INIT_restart_ecu();    /* the VPM server makes it again, and notes it */
    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );
    assert_the_ecu_does_not_hand_over_again();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a record of the NVM that a power cut tore is never half made: the ECU keeps the write it had before, here none,
*                  so nothing is pending and the flash is not touched
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_kept_write_whose_nvm_record_was_torn_by_a_power_cut_is_lost_not_half_made( void )
{
    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );

    TMD_INIT_power_cut_ecu( TMD_INIT_CORRUPT_NVM );    /* the program that kept it is torn */
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify the VPM server's note that it applied the batch is what stops the ECU handing over again: nothing is pending
*                  once it has, and the next start of the ECU goes to the application as usual
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_once_the_batch_is_applied_nothing_is_pending_and_the_next_start_does_not_hand_over( void )
{
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );

    apply_the_kept_writes();

    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write after the batch was applied starts a new one: it is kept again and made at the next hand-over, and
*                  what the first batch made is still there
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_after_the_batch_was_applied_is_a_new_batch_and_made_in_its_turn( void )
{
    const u8_t second_data[2] = { 0x55u, 0x66u };

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    apply_the_kept_writes();

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, second_data, sizeof( second_data ), NULL_P ) );

    apply_the_kept_writes();

    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );
    assert_flash_holds( TEST_PARAMETER_ADDRESS + 0x100u, second_data, sizeof( second_data ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the slots are free again once the batch is applied: a write that needs every slot is refused with one used,
*                  and kept after the restarts that hand over and make the batch
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_slots_are_free_again_once_the_batch_is_applied( void )
{
    const u8_t big_data[VPM_COMMON_REQUEST_MAX] = { 0x11u };
    u8_t       nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, big_data, sizeof( big_data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_CONDITIONS_NOT_CORRECT, nrc );

    apply_the_kept_writes();

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS + 0x100u, big_data, sizeof( big_data ), NULL_P ) );
}
/*!
****************************************************************************************************
*
*   \brief         Verify a request whose format the ECU cannot take is refused as an incorrect length and nothing is kept: no format
*                  byte, a format that gives no address or no size, an address or a size of more than 4 bytes, and a request that
*                  has the header and no data
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_in_a_format_the_ecu_cannot_take_is_refused_and_nothing_is_kept( void )
{
    const u8_t no_format[1]    = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS };
    const u8_t no_lengths[4]   = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x00u, 0x11u, 0x22u };           /* no address and no size */
    const u8_t no_address[5]   = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x20u, 0x00u, 0x02u, 0x11u };     /* a 2-byte size and no address */
    const u8_t long_address[9] = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x15u, 0x00u, 0x08u, 0x01u, 0xE4u, 0x20u, 0x01u, 0x11u };    /* a 5-byte address */
    const u8_t long_size[9]    = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x54u, 0x08u, 0x01u, 0xE4u, 0x20u, 0x00u, 0x00u, 0x00u };      /* a 5-byte size */
    const u8_t no_data[8]      = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, TMD_UDS_ADDRESS_AND_LENGTH_FORMAT, 0x08u, 0x01u, 0xE4u, 0x20u, 0x00u, 0x04u };    /* the header only */
    const u8_t* requests[6]    = { no_format, no_lengths, no_address, long_address, long_size, no_data };
    const u16_t lengths[6]     = { sizeof( no_format ), sizeof( no_lengths ), sizeof( no_address ), sizeof( long_address ), sizeof( long_size ), sizeof( no_data ) };
    u8_t        response[8];
    u16_t       response_length = 0u;
    u8_t        nrc;
    u8_t        i;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    for( i = 0u; i < 6u; i++ )
    {
        nrc = 0u;
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( requests[i], lengths[i], response, sizeof( response ), &response_length, &nrc ) );
        TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_INCORRECT_LENGTH, nrc );
    }

    assert_the_refused_write_was_not_kept();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the other formats the service allows are taken: a 1-byte size and a 4-byte size, with a 4-byte address, answered
*                  with the format, the address and the size as they came, and the data is made
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_in_a_one_byte_size_and_a_four_byte_size_format_is_made_and_echoed( void )
{
    const u8_t one_byte_size[11]  = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x14u, 0x08u, 0x01u, 0xE4u, 0x20u, 0x04u, 0x11u, 0x22u, 0x33u, 0x44u };
    const u8_t four_byte_size[12] = { TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS, 0x44u, 0x08u, 0x01u, 0xE4u, 0x40u, 0x00u, 0x00u, 0x00u, 0x02u, 0x55u, 0x66u };
    const u8_t second_data[2]     = { 0x55u, 0x66u };
    u8_t       response[12];
    u16_t      response_length = 0u;
    u8_t       nrc             = 0xFFu;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( one_byte_size, sizeof( one_byte_size ), response, sizeof( response ), &response_length, &nrc ) );
    TEST_ASSERT_EQUAL_UINT16( 7u, response_length );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SID_WRITE_MEMORY_BY_ADDRESS + 0x40u, response[0] );
    TEST_ASSERT_EQUAL_UINT8_ARRAY( &one_byte_size[1], &response[1], 6u );    /* the format, the address and the size, as they came */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( four_byte_size, sizeof( four_byte_size ), response, sizeof( response ), &response_length, &nrc ) );
    TEST_ASSERT_EQUAL_UINT16( 10u, response_length );
    TEST_ASSERT_EQUAL_UINT8_ARRAY( &four_byte_size[1], &response[1], 9u );

    apply_the_kept_writes();
    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );
    assert_flash_holds( TEST_PARAMETER_ADDRESS + 0x20u, second_data, sizeof( second_data ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write of one byte at the first byte of the page and one at its last byte are both made, and nothing else in
*                  the page changes
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_at_the_first_and_the_last_byte_of_the_page_is_made( void )
{
    const u8_t first = 0xAAu;
    const u8_t last  = 0xBBu;
    u8_t       expected[FLS_STM32F1_PAGE_SIZE];

    memset( expected, TEST_OLD_BYTE, sizeof( expected ) );
    expected[0]                           = first;
    expected[FLS_STM32F1_PAGE_SIZE - 1u] = last;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS, &first, 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS + FLS_STM32F1_PAGE_SIZE - 1u, &last, 1u, NULL_P ) );

    apply_the_kept_writes();
    assert_flash_holds( TEST_VPM_ADDRESS, expected, FLS_STM32F1_PAGE_SIZE );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write with a byte before the page, one that runs in from before it, one that runs out past its end, and one
*                  that wraps round the address space are all refused as out of range, and nothing is kept
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_that_starts_before_the_page_or_wraps_the_address_space_is_refused( void )
{
    u8_t nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS - 1u, test_data_c, 1u, &nrc ) );    /* the last byte of the APP image */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS - 2u, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( TEST_VPM_ADDRESS + FLS_STM32F1_PAGE_SIZE - 1u, test_data_c, 2u, &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_write_memory_by_address( 0xFFFFFFFEul, test_data_c, sizeof( test_data_c ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    assert_the_refused_write_was_not_kept();
}
/*!
****************************************************************************************************
*
*   \brief         A reset after a kept write hands over in the one reset it makes: the FBL is asked for before the ECU restarts, and
*                  that restart is the one that makes the write
*
*   \author        mstewart
*
*   \param[in]     reset_type   The ECU Reset sub-function the tester sends
*
*   \return        none
*
*****************************************************************************************************/
static void assert_a_reset_hands_over_in_its_one_reset( u8_t reset_type )
{
    u32_t resets_before;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );

    resets_before = vhw_core_get_system_resets();
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_ecu_reset( reset_type, NULL_P ) );
    TMD_TIME_advance_time_ms( 50u );

    TEST_ASSERT_EQUAL_UINT32( resets_before + 1u, vhw_core_get_system_resets() );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );    /* asked for before the restart: the application does not have to start to do it */

    TMD_INIT_restart_ecu();
    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    assert_flash_holds( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a key off/on reset (0x11 0x02) after a kept write hands over in its one reset
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_key_off_on_reset_after_a_write_hands_over_in_its_one_reset( void )
{
    assert_a_reset_hands_over_in_its_one_reset( TMD_UDS_RESET_KEY_OFF_ON );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a hard reset (0x11 0x01) after a kept write hands over in its one reset
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_hard_reset_after_a_write_hands_over_in_its_one_reset( void )
{
    assert_a_reset_hands_over_in_its_one_reset( TMD_UDS_RESET_HARD );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a soft reset (0x11 0x03) after a kept write hands over in its one reset
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_soft_reset_after_a_write_hands_over_in_its_one_reset( void )
{
    assert_a_reset_hands_over_in_its_one_reset( TMD_UDS_RESET_SOFT );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a reset with no write waiting is a plain reset, whichever type it is: the FBL is not asked for
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_reset_with_no_write_waiting_does_not_ask_for_the_fbl( void )
{
    const u8_t types[3u] = { TMD_UDS_RESET_HARD, TMD_UDS_RESET_KEY_OFF_ON, TMD_UDS_RESET_SOFT };
    u8_t       i;
    u32_t      resets_before;

    for( i = 0u; i < sizeof( types ); i++ )
    {
        resets_before = vhw_core_get_system_resets();

        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_ecu_reset( types[i], NULL_P ) );
        TMD_TIME_advance_time_ms( 50u );

        TEST_ASSERT_EQUAL_UINT32( resets_before + 1u, vhw_core_get_system_resets() );
        TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );
    }
}

/***************************************************************************************************
**                                  ReadMemoryByAddress (0x23)                                    **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify a read returns what the variant parameter page holds, the data and nothing else in the response
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_read_returns_what_the_variant_parameter_page_holds( void )
{
    u8_t data[4];
    u8_t nrc = 0xFFu;

    memset( data, 0u, sizeof( data ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, nrc );
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_PARAMETER_ADDRESS, TEST_OLD_BYTE, sizeof( data ) ) );
    TEST_ASSERT_EQUAL_UINT8( TEST_OLD_BYTE, data[0] );
    TEST_ASSERT_EQUAL_UINT8( TEST_OLD_BYTE, data[3] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a write that is kept is not in a read yet, as the flash is not written until the FBL makes it, and is in it
*                  after: a tester reads back what a write did
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_is_in_a_read_once_the_fbl_has_made_it_and_not_before( void )
{
    u8_t data[sizeof( test_data_c )];

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TEST_OLD_BYTE, data[0] );                       /* kept, not made */

    apply_the_kept_writes();

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );    /* the restart ended the session */
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8_ARRAY( test_data_c, data, sizeof( data ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the whole variant parameter page can be read in one request, and its first and its last byte on their own
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_whole_page_and_its_ends_can_be_read( void )
{
    u8_t page[FLS_STM32F1_PAGE_SIZE];
    u8_t one = 0u;
    u8_t i;

    for( i = 0u; i < 16u; i++ )
    {
        TMD_MBA_flash_fill( TEST_VPM_ADDRESS + ( (u32_t)i * 64u ), (u8_t)( 0x10u + i ), 64u );    /* a page that says where each byte is */
    }

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TMD_UDS_set_timeout( 3000u );    /* the page is 147 consecutive frames at the tester's 10 ms apart: more than the 500 ms it waits for an answer by default */
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_VPM_ADDRESS, page, sizeof( page ), NULL_P ) );
    assert_flash_holds( TEST_VPM_ADDRESS, page, sizeof( page ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_VPM_ADDRESS, &one, 1u, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( 0x10u, one );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_read_memory_by_address( TEST_DATASET0_ADDRESS - 1u, &one, 1u, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( 0x1Fu, one );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a read of memory outside the variant parameter page is refused, and so is one that starts in it and reaches out:
*                  the application, DATASET0 and the NVM, which holds the keys, are not for a tester to read
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_read_outside_the_page_is_refused( void )
{
    u8_t data[4];
    u8_t nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_APP_ADDRESS, data, sizeof( data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_NVM_ADDRESS, data, sizeof( data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_DATASET0_ADDRESS, data, sizeof( data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_VPM_ADDRESS - 1u, data, sizeof( data ), &nrc ) );    /* a byte before the page */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_DATASET0_ADDRESS - 2u, data, sizeof( data ), &nrc ) );    /* two bytes in the page, two out */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_REQUEST_OUT_OF_RANGE, nrc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a read request the ECU cannot take is refused: in a format it does not take, or with bytes after the size, as an
*                  incorrect length, and a size of nothing as out of range
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_read_request_the_ecu_cannot_take_is_refused( void )
{
    const u8_t no_format[1]    = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS };
    const u8_t no_lengths[4]   = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, 0x00u, 0x11u, 0x22u };
    const u8_t no_address[4]   = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, 0x20u, 0x00u, 0x02u };                     /* a 2-byte size and no address */
    const u8_t long_address[8] = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, 0x15u, 0x00u, 0x08u, 0x01u, 0xE4u, 0x20u, 0x01u };    /* a 5-byte address */
    const u8_t short_request[7]= { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, TMD_UDS_ADDRESS_AND_LENGTH_FORMAT, 0x08u, 0x01u, 0xE4u, 0x20u, 0x00u };    /* a byte of the size missing */
    const u8_t extra_byte[9]   = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, TMD_UDS_ADDRESS_AND_LENGTH_FORMAT, 0x08u, 0x01u, 0xE4u, 0x20u, 0x00u, 0x04u, 0x99u };    /* a byte after the size */
    const u8_t zero_size[8]    = { TMD_UDS_SID_READ_MEMORY_BY_ADDRESS, TMD_UDS_ADDRESS_AND_LENGTH_FORMAT, 0x08u, 0x01u, 0xE4u, 0x20u, 0x00u, 0x00u };
    const u8_t* requests[7]    = { no_format, no_lengths, no_address, long_address, short_request, extra_byte, zero_size };
    const u16_t lengths[7]     = { sizeof( no_format ), sizeof( no_lengths ), sizeof( no_address ), sizeof( long_address ), sizeof( short_request ), sizeof( extra_byte ), sizeof( zero_size ) };
    u8_t        response[16];
    u16_t       response_length = 0u;
    u8_t        nrc;
    u8_t        i;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );

    for( i = 0u; i < 7u; i++ )
    {
        nrc = 0u;
        TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_send_request( requests[i], lengths[i], response, sizeof( response ), &response_length, &nrc ) );
        TEST_ASSERT_EQUAL_UINT8( ( i == 6u ) ? TMD_UDS_NRC_REQUEST_OUT_OF_RANGE : TMD_UDS_NRC_INCORRECT_LENGTH, nrc );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Verify a read is refused without the security access, in the extended session, and in the default session: it is behind
*                  the same session and security level as a write
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_read_without_the_session_and_the_security_access_is_refused( void )
{
    u8_t data[4];
    u8_t nrc = 0u;

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), &nrc ) );    /* the default session */
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION, nrc );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, TMD_UDS_read_memory_by_address( TEST_PARAMETER_ADDRESS, data, sizeof( data ), &nrc ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_NRC_SECURITY_ACCESS_DENIED, nrc );
}

/****************************** END OF FILE *******************************************************/
