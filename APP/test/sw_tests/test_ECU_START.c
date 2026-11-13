/*! \file
*               Author: mstewart
*   \brief      How the ECU starts - software test
*
*   \details    Every test case starts as a virgin ECU, and a restart in the middle of one says what happens to the ECU's memories.
*
*               A new ECU (TMD_INIT_init(), which every test's setUp calls) is the same whatever the one before it left: its flash
*               blank but for what the firmware writes at its first start, its handoff RAM as it is at a power-on, its models and its
*               firmware's RAM as they are at the first start, and the time the first start took. A restart keeps them all
*               (TMD_INIT_restart_ecu()); a power cut loses the handoff RAM, and wipes or corrupts the memories it is told to
*               (TMD_INIT_power_cut_ecu()).
*
*               The ECU's data that these tests watch is a write it kept for the VPM server (0x3D, TMD_UDS_write_memory_by_address()):
*               it is in the NVM, so whether the NVM is kept shows in whether the write is still pending, and the variant parameter
*               page of the flash is the VPM server's.
*/
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "BOOT_HANDOFF.h"
#include "FLS_STM32F1.h"

#include "TMD_INIT.h"
#include "TMD_MBA.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_FLASH_BASE                 ( 0x08000000ul )    /* the flash of the virtual STM32 at its real address */
#define TEST_FLASH_SIZE                 ( 0x00080000ul )    /* the largest device, 512 KB: all of it is the ECU's flash */
#define TEST_FILL_CHUNK                 ( 0x8000u )
#define TEST_VPM_ADDRESS                ( 0x0801E400ul )    /* the variant parameter page: one page, 1 KB */
#define TEST_PARAMETER_ADDRESS          ( TEST_VPM_ADDRESS + 0x20u )    /* a parameter in it, in the first half of the page */
#define TEST_OLD_BYTE                   ( 0xA5u )           /* what a page held before */

static const u8_t test_data_c[4] = { 0x11u, 0x22u, 0x33u, 0x44u };

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
*   \brief         The ECU keeps a write for the VPM server and the page that write is for holds something
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void keep_a_write_over_a_page_that_holds_something( void )
{
    TMD_MBA_flash_fill( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_unlock( 1u, NULL_P ) );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_write_memory_by_address( TEST_PARAMETER_ADDRESS, test_data_c, sizeof( test_data_c ), NULL_P ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a new ECU is the one a clean start gives, however much the last one left behind: the flash (all 512 KB), the
*                  handoff RAM, the session of the diagnostics, the VPM server's work, and the time
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_new_ecu_is_the_same_whatever_the_last_one_left( void )
{
    u8_t  reference_nvm[NVM_TOTAL_SIZE];
    u8_t  nvm[NVM_TOTAL_SIZE];
    u8_t  session = 0u;
    u32_t address;

    TMD_INIT_init( &sw_test_init_cfg_g );    /* a clean start of its own, for the reference: the one setUp() made may be the first of the run */

    const u8_t   reference_request    = BOOT_HANDOFF_get_fbl_request();
    const u8_t   reference_failures   = BOOT_HANDOFF_get_boot_failure_count();
    const u8_t   reference_reason     = BOOT_HANDOFF_get_last_failure_reason();
    const u8_t   reference_datasets   = BOOT_HANDOFF_get_dataset_valid_mask();
    const u32_t  reference_timeout    = BOOT_HANDOFF_get_fbl_timeout();
    const u32_t  reference_time_ms    = TMD_TIME_get_current_time_ms();

    TMD_MBA_flash_read( NVM_BASE_ADDRESS, reference_nvm, NVM_TOTAL_SIZE );

    /* The last ECU: the diagnostics in the extended session and unlocked, a write kept, time gone by, and then every byte of its flash
       and every field of its handoff RAM something else */
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_diagnostic_session_control( TMD_UDS_SESSION_EXTENDED, NULL_P ) );
    keep_a_write_over_a_page_that_holds_something();
    TMD_TIME_advance_time_ms( 500u );

    for( address = TEST_FLASH_BASE; address < ( TEST_FLASH_BASE + TEST_FLASH_SIZE ); address += TEST_FILL_CHUNK )
    {
        TMD_MBA_flash_fill( address, TEST_OLD_BYTE, TEST_FILL_CHUNK );
    }

    BOOT_HANDOFF_set_fbl_request( TRUE );
    BOOT_HANDOFF_set_boot_failure_count( 7u );
    BOOT_HANDOFF_set_last_failure_reason( 9u );
    BOOT_HANDOFF_set_dataset_valid_mask( 0xFFu );
    BOOT_HANDOFF_set_fbl_timeout( 1234u );

    TMD_INIT_init( &sw_test_init_cfg_g );

    for( address = TEST_FLASH_BASE; address < NVM_BASE_ADDRESS; address += TEST_FILL_CHUNK )
    {
        TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( address, 0xFFu, (u16_t)( ( NVM_BASE_ADDRESS - address ) < TEST_FILL_CHUNK ? ( NVM_BASE_ADDRESS - address ) : TEST_FILL_CHUNK ) ) );
    }

    for( address = NVM_BASE_ADDRESS + NVM_TOTAL_SIZE; address < ( TEST_FLASH_BASE + TEST_FLASH_SIZE ); address += TEST_FILL_CHUNK )
    {
        TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( address, 0xFFu, TEST_FILL_CHUNK ) );    /* nothing above the NVM either */
    }

    TMD_MBA_flash_read( NVM_BASE_ADDRESS, nvm, NVM_TOTAL_SIZE );
    TEST_ASSERT_EQUAL_UINT8_ARRAY( reference_nvm, nvm, NVM_TOTAL_SIZE );    /* what the firmware writes at its first start, and no more */

    TEST_ASSERT_EQUAL_UINT8( reference_request, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL_UINT8( reference_failures, BOOT_HANDOFF_get_boot_failure_count() );
    TEST_ASSERT_EQUAL_UINT8( reference_reason, BOOT_HANDOFF_get_last_failure_reason() );
    TEST_ASSERT_EQUAL_UINT8( reference_datasets, BOOT_HANDOFF_get_dataset_valid_mask() );
    TEST_ASSERT_EQUAL_UINT32( reference_timeout, BOOT_HANDOFF_get_fbl_timeout() );
    TEST_ASSERT_EQUAL_UINT32( reference_time_ms, TMD_TIME_get_current_time_ms() );    /* before the diagnostics are asked anything: a request takes time on the bus */

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_get_session( &session, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT8( TMD_UDS_SESSION_DEFAULT, session );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a restart keeps everything: the write the ECU kept, the page, and the handoff RAM, so the ECU that handed over
*                  goes to the VPM server, which makes the page
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_restart_keeps_the_memories_and_the_handoff_ram( void )
{
    keep_a_write_over_a_page_that_holds_something();

    TMD_INIT_restart_ecu();    /* the application hands over */
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );

    TMD_INIT_restart_ecu();    /* the request is still in the handoff RAM: the VPM server runs first */
    TEST_ASSERT_EQUAL( FALSE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut that corrupts nothing loses the handoff RAM and nothing else: the ECU that had handed over starts
*                  the application, not the VPM server, so the page is as it was and the write is still kept
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_loses_the_handoff_ram_and_keeps_the_memories( void )
{
    keep_a_write_over_a_page_that_holds_something();
    TMD_INIT_restart_ecu();    /* the application hands over */

    TMD_INIT_power_cut_ecu( TMD_INIT_KEEP_ALL );

    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );    /* the write is still kept: the application hands over again */
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut that wipes the NVM starts the ECU with none of its data: the write it kept is gone, and the flash,
*                  which is not wiped, still holds the page
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_that_wipes_the_nvm_keeps_the_flash_and_loses_the_ecus_data( void )
{
    keep_a_write_over_a_page_that_holds_something();

    TMD_INIT_power_cut_ecu( TMD_INIT_WIPE_NVM );

    TEST_ASSERT_EQUAL( FALSE, BOOT_HANDOFF_get_fbl_request() );    /* nothing kept: the application does not hand over */
    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, TEST_OLD_BYTE, FLS_STM32F1_PAGE_SIZE ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut that wipes the page keeps the NVM: the page is blank and the write the ECU kept is still pending
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_that_wipes_the_page_keeps_the_nvm( void )
{
    keep_a_write_over_a_page_that_holds_something();

    TMD_INIT_power_cut_ecu( TMD_INIT_WIPE_MBA );

    TEST_ASSERT_EQUAL( TRUE, TMD_MBA_flash_is_value( TEST_VPM_ADDRESS, 0xFFu, FLS_STM32F1_PAGE_SIZE ) );
    TEST_ASSERT_EQUAL( TRUE, BOOT_HANDOFF_get_fbl_request() );    /* the NVM is kept, so the write is: the application hands over */
}

/****************************** END OF FILE *******************************************************/
