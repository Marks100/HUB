/*! \file
*               Author: mstewart
*   \brief      NVM - software test
*
*   \details    The real stack from a block to the flash: NVM_GEN2, the project's hardware table in INTEGRATION_STUBS.c,
*               FLS_STM32F1 and the SPL FLASH_* functions on the virtual flash (2 KB at NVM_BASE_ADDRESS). The flash keeps
*               its contents across a power cycle, so a restart that keeps the NVM finds what the firmware saved, and a
*               virgin one finds it blank. A test makes a fault the way the silicon would: by changing the flash. Every
*               test case starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*
*               The block's RAM copy survives a restart in the test process (the firmware's RAM is not reset), so a test
*               overwrites it before the restart: what it reads afterwards is then what registration took from the flash.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "APP_NVM_BLOCKS.h"
#include "NVM_GEN2.h"

#include "stm32f10x_flash.h"

#include "TMD_INIT.h"
#include "TMD_NVM.h"

#include "SW_TEST_STUBS.h"

#define TEST_SAVED_WELD_TIME_MS     ( 1234u )    /* neither the default (40) nor the value a test overwrites it with */
#define TEST_OVERWRITTEN_WELD_TIME  ( 0u )

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU: blank flash, so every block holds its defaults
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
*   \brief         Change the generic block and make NVM_GEN2 write it to the flash
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_save_generic_block( void )
{
    app_generic_data_blk_g.weld_time_ms = TEST_SAVED_WELD_TIME_MS;

    NVM_GEN2_flush_block( APP_GENERIC_BLOCK_ID );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the flash window of the virtual STM32 exists, which every test here relies on
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_virtual_flash_is_mapped( void )
{
    TEST_ASSERT_EQUAL( TRUE, FLASH_SIM_is_mapped() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a virgin ECU has no record of the generic block on flash and holds its defaults
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_virgin_ecu_holds_the_default_block( void )
{
    const NVM_GEN2_block_info_st* info_p = NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID );

    TEST_ASSERT_NOT_NULL( info_p );
    TEST_ASSERT_EQUAL_HEX32( NVM_GEN2_OFFSET_NONE, info_p->latest_offset );
    TEST_ASSERT_EQUAL_UINT8( APP_GENERIC_DEFAULT_DATA_BLK_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a saved block has a record on flash
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_saved_block_has_a_record_on_flash( void )
{
    test_save_generic_block();

    TEST_ASSERT_TRUE( NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset != NVM_GEN2_OFFSET_NONE );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a saved block survives a power cycle that keeps the NVM: the RAM copy is overwritten first, so
*                  the value afterwards is the one registration read back from the flash
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_saved_block_survives_a_restart_that_keeps_the_nvm( void )
{
    test_save_generic_block();
    app_generic_data_blk_g.weld_time_ms = TEST_OVERWRITTEN_WELD_TIME;

    TMD_INIT_restart_ecu( NON_VIRGIN );

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a virgin restart loses a saved block: the flash is blank again, so the defaults come back
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_virgin_restart_loses_a_saved_block( void )
{
    test_save_generic_block();
    app_generic_data_blk_g.weld_time_ms = TEST_OVERWRITTEN_WELD_TIME;

    TMD_INIT_restart_ecu( VIRGIN );

    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a record with a damaged payload is not used: a bit flips in the flash, the CRC no longer
*                  matches, and registration applies the defaults instead
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_corrupt_record_is_replaced_by_the_defaults( void )
{
    u32_t payload_address;

    test_save_generic_block();

    payload_address = NVM_GEN2_get_active_base_address() +
                      NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset +
                      (u32_t)sizeof( NVM_GEN2_rec_hdr_st );

    *(volatile u8_t*)payload_address ^= 0x01u;
    app_generic_data_blk_g.weld_time_ms = TEST_OVERWRITTEN_WELD_TIME;

    TMD_INIT_restart_ecu( NON_VIRGIN );

    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT8( APP_GENERIC_DEFAULT_DATA_BLK_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the NVM model sees the programs of a save, so a test can count what the firmware wrote
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_nvm_model_counts_the_programs_of_a_save( void )
{
    TMD_NVM_reset_statistics();

    test_save_generic_block();

    TEST_ASSERT_TRUE( TMD_NVM_get_statistics().write_count > 0u );
    TEST_ASSERT_EQUAL_UINT32( 0u, TMD_NVM_get_statistics().error_count );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a flash that fails every program makes the save fail: the flash controller sets PGERR, the read
*                  back no longer matches what was written, and NVM_GEN2 goes to its fault state
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_write_error_puts_nvm_gen2_in_fault( void )
{
    TMD_NVM_inject_write_error( 0u, NVM_TOTAL_SIZE );

    test_save_generic_block();

    TEST_ASSERT_EQUAL( NVM_GEN2_STATE_FAULT, NVM_GEN2_get_state() );
    TEST_ASSERT_TRUE( TMD_NVM_get_statistics().error_count > 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the NVM works again after a power cycle that clears the fault: the flash is the same, nothing fails
*                  now, and a save reaches it
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cycle_recovers_nvm_gen2_from_a_write_error( void )
{
    TMD_NVM_inject_write_error( 0u, NVM_TOTAL_SIZE );
    test_save_generic_block();

    TMD_INIT_restart_ecu( NON_VIRGIN );
    test_save_generic_block();

    TEST_ASSERT_EQUAL( NVM_GEN2_STATE_IDLE, NVM_GEN2_get_state() );
    TEST_ASSERT_TRUE( NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset != NVM_GEN2_OFFSET_NONE );
}

/****************************** END OF FILE *******************************************************/
