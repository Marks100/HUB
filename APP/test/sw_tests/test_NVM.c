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

#include <stddef.h>

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "APP_NVM_BLOCKS.h"
#include "NVM_GEN2.h"

#include "stm32f10x_flash.h"

#include "TMD_INIT.h"
#include "TMD_NVM.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_SAVED_WELD_TIME_MS     ( 1234u )    /* neither the default (40) nor the value a test overwrites it with */
#define TEST_OVERWRITTEN_WELD_TIME  ( 0u )

#define TEST_SECOND_WELD_TIME       ( 2222u )    /* a later save than TEST_SAVED_WELD_TIME_MS */
#define TEST_THIRD_WELD_TIME        ( 3333u )    /* and a later one than that */
#define TEST_MAX_MS_PER_SAVE                ( 5000u )   /* a save that takes longer than this is stuck */
#define TEST_NVM_SETTLED_MS                 ( 60u )     /* longer than the 50 ms period of the NVM tick (SCHED_config.c) */
#define TEST_MAX_SAVES_TO_FILL_A_PARTITION  ( 400u )    /* a record is 8 bytes of overhead and the block, in a page of the 2 KB */

/* Every field different from its default (23, FALSE, 0, FALSE, 40, 0, 0) */
static const APP_generic_data_blk_st test_saved_generic_blk_s =
{
    .screen_brightness  = 77u,
    .reset_request      = TRUE,
    .reset_type         = 3u,
    .bl_request         = TRUE,
    .weld_time_ms       = TEST_SAVED_WELD_TIME_MS,
    .total_num_welds    = 0x11223344u,
    .total_weld_time_ms = 0x55667788u
};

/* What the previous firmware saved: the generic block as it was in version 1, before total_weld_time_ms */
static const APP_generic_data_blk_v1_st test_saved_generic_blk_v1_s =
{
    .screen_brightness = 77u,
    .reset_request     = TRUE,
    .reset_type        = 3u,
    .bl_request        = TRUE,
    .weld_time_ms      = TEST_SAVED_WELD_TIME_MS,
    .total_num_welds   = 0x11223344u
};

/* A test case leaves a saved block in the flash and sets this: the next case looks for the block, and clears it. It is
   not reset by setUp(), which is the point - only the flash must not carry over. */
static false_true_et test_block_left_for_next_case_s = FALSE;

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
*   \brief         Let time pass, a millisecond at a time, until the save in flight has ended
*
*   \author        mstewart
*
*   \param[in]     stop_state    Also stop when NVM_GEN2 is in this state (a compaction state), NVM_GEN2_STATE_MAX for none
*
*   \return        none
*
*   \note          The scheduler runs NVM_GEN2_tick(), as it does in the firmware: the state is looked at after every
*                  millisecond. NVM_GEN2 goes back to IDLE between one pending write and the next, and a request
*                  waits for the scheduler's next NVM tick, so the saves have ended when it has stayed IDLE for longer
*                  than the tick period (TEST_NVM_SETTLED_MS), or is in FAULT
*
*****************************************************************************************************/
static void test_advance_time_until_the_save_ends( NVM_GEN2_state_et stop_state )
{
    u32_t elapsed_ms = 0u;
    u32_t idle_ms    = 0u;

    do
    {
        TMD_TIME_advance_time_ms( 1u );
        elapsed_ms++;

        if( NVM_GEN2_get_state() == NVM_GEN2_STATE_IDLE )
        {
            idle_ms++;
        }
        else
        {
            idle_ms = 0u;
        }
    }
    while( ( idle_ms < TEST_NVM_SETTLED_MS ) &&
           ( NVM_GEN2_get_state() != stop_state ) &&
           ( NVM_GEN2_get_state() != NVM_GEN2_STATE_FAULT ) &&
           ( elapsed_ms < TEST_MAX_MS_PER_SAVE ) );

    TEST_ASSERT_TRUE_MESSAGE( elapsed_ms < TEST_MAX_MS_PER_SAVE, "the save did not end" );
}

/*!
****************************************************************************************************
*
*   \brief         Request the write of a block and let time pass until it has reached the flash
*
*   \author        mstewart
*
*   \param[in]     block_id    A registered block
*
*   \return        none
*
*   \note          What NVM_GEN2_flush_block() does by pumping the tick with no time passing, done through the time model
*
*****************************************************************************************************/
static void test_save_block( u8_t block_id )
{
    NVM_GEN2_request_write_block( block_id );
    test_advance_time_until_the_save_ends( NVM_GEN2_STATE_MAX );
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

    test_save_block( APP_GENERIC_BLOCK_ID );
}

/*!
****************************************************************************************************
*
*   \brief         Overwrite the RAM copy of every block, so what a block holds after a restart can only be what registration
*                  took from the flash
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The fill is not any value the tests save, and not a default
*
*****************************************************************************************************/
static void test_scramble_the_ram_copies( void )
{
    STDC_memset( &app_generic_data_blk_g, 0xA5, sizeof( app_generic_data_blk_g ) );
    STDC_memset( &app_key_1_blk_g, 0xA5, sizeof( app_key_1_blk_g ) );
    STDC_memset( &app_key_2_blk_g, 0xA5, sizeof( app_key_2_blk_g ) );
    STDC_memset( &app_chassis_num_blk_g, 0xA5, sizeof( app_chassis_num_blk_g ) );
}

/*!
****************************************************************************************************
*
*   \brief         The address in the flash of the generic block's latest record
*
*   \author        mstewart
*
*   \return        The address of its header
*
*****************************************************************************************************/
static u32_t test_latest_generic_record_address( void )
{
    return( NVM_GEN2_get_active_base_address() + NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset );
}

/*!
****************************************************************************************************
*
*   \brief         The address in the flash of the first payload byte of the generic block's latest record
*
*   \author        mstewart
*
*   \return        The address, for a test to damage the record the way a power cut or a flipped bit would
*
*****************************************************************************************************/
static u32_t test_latest_generic_payload_address( void )
{
    return( test_latest_generic_record_address() + (u32_t)sizeof( NVM_GEN2_rec_hdr_st ) );
}

/*!
****************************************************************************************************
*
*   \brief         Save the generic block, then put a record of another version or length where it is on the flash
*
*   \author        mstewart
*
*   \param[in]     version        The version the record claims
*   \param[in]     payload_p      What the record holds
*   \param[in]     payload_len    How many bytes of it, no more than the current block
*
*   \return        none
*
*   \note          What an older firmware would have left. The record is built with valid CRCs, so only its version or
*                  length is out of step with the block's configuration; the rest of the old record is erased flash
*
*****************************************************************************************************/
static void test_replace_the_latest_generic_record( u8_t version, const void* payload_p, u16_t payload_len )
{
    NVM_GEN2_rec_hdr_st hdr;
    u8_t*               record_p;
    u32_t               old_size;

    test_save_generic_block();

    record_p = (u8_t*)test_latest_generic_record_address();
    old_size = (u32_t)sizeof( NVM_GEN2_rec_hdr_st ) + ( ( (u32_t)sizeof( APP_generic_data_blk_st ) + 1u ) & ~1u );

    hdr.block_id = APP_GENERIC_BLOCK_ID;
    hdr.version  = version;
    hdr.data_len = payload_len;
    hdr.hdr_crc  = CHKSUM_calc_crc16_ccitt_false( (const u8_t*)&hdr, (u16_t)offsetof( NVM_GEN2_rec_hdr_st, hdr_crc ) );
    hdr.data_crc = CHKSUM_calc_crc16_ccitt_false( (const u8_t*)payload_p, payload_len );

    STDC_memset( record_p, 0xFF, old_size );
    STDC_memcpy( record_p, &hdr, sizeof( hdr ) );
    STDC_memcpy( record_p + sizeof( hdr ), payload_p, payload_len );
}

/*!
****************************************************************************************************
*
*   \brief         Save the generic block with ever-changing values until NVM_GEN2 is in a state of its compaction
*
*   \author        mstewart
*
*   \param[in]     state    One of NVM_GEN2_STATE_GC_*
*
*   \return        The value of the last save that reached the flash: the one in flight when the state was reached is not it
*
*   \note          Time passes through the time model, so the scheduler runs the compaction a tick at a time, and the
*                  state is looked at after every millisecond. NVM_GEN2_flush() would run the compaction through
*
*****************************************************************************************************/
static u16_t test_save_until_the_compaction_is_in( NVM_GEN2_state_et state )
{
    u16_t last_on_flash = 0u;
    u16_t save;

    for( save = 1u; ( save <= TEST_MAX_SAVES_TO_FILL_A_PARTITION ) && ( NVM_GEN2_get_state() != state ); save++ )
    {
        app_generic_data_blk_g.weld_time_ms = save;
        NVM_GEN2_request_write_block( APP_GENERIC_BLOCK_ID );

        test_advance_time_until_the_save_ends( state );

        if( NVM_GEN2_get_state() == NVM_GEN2_STATE_IDLE )
        {
            last_on_flash = save;
        }
    }

    return( last_on_flash );
}

/*!
****************************************************************************************************
*
*   \brief         Cut the power when the compaction is in a state, and check what the restart recovers
*
*   \author        mstewart
*
*   \param[in]     state    One of NVM_GEN2_STATE_GC_*
*
*   \return        none
*
*   \note          A block saved once before it, and the generic block saved until the partition is full. The restart has to
*                  find both, with the generic block at the last save that reached the flash, and the NVM has to go on
*                  working: the next save and restart recover the new value
*
*****************************************************************************************************/
static void test_assert_a_power_cut_in_the_compaction_state_keeps_the_last_saves( NVM_GEN2_state_et state )
{
    static const u8_t chassis_num[APP_CHASSIS_NUM_LEN] = { 'C', 'U', 'T', '-', 'I', 'N', '-', 'G', 'C', '-', 'S', 'U', 'R', 'V', 'I', 'V' };
    u16_t             last_on_flash;

    STDC_memcpy( app_chassis_num_blk_g.chassis_num, chassis_num, APP_CHASSIS_NUM_LEN );
    test_save_block( APP_CHASSIS_NUM_BLOCK_ID );

    last_on_flash = test_save_until_the_compaction_is_in( state );

    TEST_ASSERT_EQUAL( state, NVM_GEN2_get_state() );    /* the cut is in the compaction, not before it and not after it */
    TEST_ASSERT_TRUE( last_on_flash > 1u );

    test_scramble_the_ram_copies();
    TMD_INIT_power_cut_ecu( TMD_INIT_KEEP_ALL );

    TEST_ASSERT_EQUAL_UINT16( last_on_flash, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_MEMORY( chassis_num, app_chassis_num_blk_g.chassis_num, APP_CHASSIS_NUM_LEN );

    app_generic_data_blk_g.weld_time_ms = TEST_THIRD_WELD_TIME;
    test_save_block( APP_GENERIC_BLOCK_ID );
    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_THIRD_WELD_TIME, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_MEMORY( chassis_num, app_chassis_num_blk_g.chassis_num, APP_CHASSIS_NUM_LEN );
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

    TMD_INIT_restart_ecu();

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

    TMD_INIT_init( &sw_test_init_cfg_g );

    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Leave a saved block in the flash at the end of this test case, for the next one to look for. Verify it was
*                  there while this case ran.
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Half of a pair with test_a_new_test_does_not_find_the_block_of_the_one_before(), which has to follow it
*
*****************************************************************************************************/
void test_a_test_leaves_a_saved_block_in_the_flash_for_the_next( void )
{
    test_save_generic_block();

    TEST_ASSERT_TRUE( NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset != NVM_GEN2_OFFSET_NONE );

    test_block_left_for_next_case_s = TRUE;
}

/*!
****************************************************************************************************
*
*   \brief         Verify the NVM does not persist into the start of a new test case: the block the one before left in the
*                  flash is not there, and the block holds its defaults
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Ignored unless test_a_test_leaves_a_saved_block_in_the_flash_for_the_next() ran just before it: run on its
*                  own there would be no block to lose. A new test case starts with TMD_INIT_init() in setUp(), a virgin ECU.
*
*****************************************************************************************************/
void test_a_new_test_does_not_find_the_block_of_the_one_before( void )
{
    if( test_block_left_for_next_case_s == FALSE )
    {
        TEST_IGNORE_MESSAGE( "the case that leaves a block in the flash did not run before this one" );
    }
    else
    {
        test_block_left_for_next_case_s = FALSE;

        TEST_ASSERT_EQUAL_HEX32( NVM_GEN2_OFFSET_NONE, NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset );
        TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    }
}

/***************************************************************************************************
**                          What a restart recovers from the flash                                **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify every field of the generic block is recovered, not only the weld time: each is saved with a value that
*                  is not its default and not what the RAM copy is scrambled to
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_every_field_of_a_saved_block_is_recovered_after_a_restart( void )
{
    app_generic_data_blk_g = test_saved_generic_blk_s;
    test_save_block( APP_GENERIC_BLOCK_ID );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT8( test_saved_generic_blk_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
    TEST_ASSERT_EQUAL( test_saved_generic_blk_s.reset_request, app_generic_data_blk_g.reset_request );
    TEST_ASSERT_EQUAL_UINT8( test_saved_generic_blk_s.reset_type, app_generic_data_blk_g.reset_type );
    TEST_ASSERT_EQUAL( test_saved_generic_blk_s.bl_request, app_generic_data_blk_g.bl_request );
    TEST_ASSERT_EQUAL_UINT16( test_saved_generic_blk_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT32( test_saved_generic_blk_s.total_num_welds, app_generic_data_blk_g.total_num_welds );
    TEST_ASSERT_EQUAL_UINT32( test_saved_generic_blk_s.total_weld_time_ms, app_generic_data_blk_g.total_weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the latest of several saves is the one recovered, not the first and not a mix
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_latest_of_several_saves_is_recovered( void )
{
    u16_t save;

    for( save = 1u; save <= 3u; save++ )
    {
        app_generic_data_blk_g = test_saved_generic_blk_s;
        app_generic_data_blk_g.weld_time_ms   = (u16_t)( 100u * save );
        app_generic_data_blk_g.total_num_welds = save;
        test_save_block( APP_GENERIC_BLOCK_ID );
    }

    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( 300u, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT32( 3u, app_generic_data_blk_g.total_num_welds );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the NVM is still there after two restarts in a row, with nothing saved in between
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_saved_block_survives_two_restarts_in_a_row( void )
{
    test_save_generic_block();

    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify all four blocks come back together after one restart, each with its own data: the two keys differ
*                  from each other, so a block recovered from another's record would be seen
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_all_the_blocks_are_recovered_together_after_a_restart( void )
{
    static const u8_t chassis_num[APP_CHASSIS_NUM_LEN] = { 'C', 'H', 'A', 'S', 'S', 'I', 'S', '-', 'T', 'E', 'S', 'T', '-', '0', '0', '1' };
    u8_t              key_1[APP_KEY_LEN];
    u8_t              key_2[APP_KEY_LEN];
    u8_t              i;

    for( i = 0u; i < APP_KEY_LEN; i++ )
    {
        key_1[i] = i;
        key_2[i] = (u8_t)( 0xFFu - i );
    }

    app_generic_data_blk_g = test_saved_generic_blk_s;
    STDC_memcpy( app_key_1_blk_g.key, key_1, APP_KEY_LEN );
    STDC_memcpy( app_key_2_blk_g.key, key_2, APP_KEY_LEN );
    STDC_memcpy( app_chassis_num_blk_g.chassis_num, chassis_num, APP_CHASSIS_NUM_LEN );

    test_save_block( APP_GENERIC_BLOCK_ID );
    test_save_block( APP_KEY_1_BLOCK_ID );
    test_save_block( APP_KEY_2_BLOCK_ID );
    test_save_block( APP_CHASSIS_NUM_BLOCK_ID );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_MEMORY( key_1, app_key_1_blk_g.key, APP_KEY_LEN );
    TEST_ASSERT_EQUAL_MEMORY( key_2, app_key_2_blk_g.key, APP_KEY_LEN );
    TEST_ASSERT_EQUAL_MEMORY( chassis_num, app_chassis_num_blk_g.chassis_num, APP_CHASSIS_NUM_LEN );
}

/*!
****************************************************************************************************
*
*   \brief         Verify saving one block does not touch another: a block saved first still has its value after a block
*                  saved later is changed and saved again
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_saving_one_block_leaves_another_blocks_record_alone( void )
{
    static const u8_t chassis_num[APP_CHASSIS_NUM_LEN] = { 'V', 'I', 'N', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1' };

    STDC_memcpy( app_chassis_num_blk_g.chassis_num, chassis_num, APP_CHASSIS_NUM_LEN );
    test_save_block( APP_CHASSIS_NUM_BLOCK_ID );

    test_save_generic_block();
    app_generic_data_blk_g.weld_time_ms = 4321u;
    test_save_block( APP_GENERIC_BLOCK_ID );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_MEMORY( chassis_num, app_chassis_num_blk_g.chassis_num, APP_CHASSIS_NUM_LEN );
    TEST_ASSERT_EQUAL_UINT16( 4321u, app_generic_data_blk_g.weld_time_ms );
}

/***************************************************************************************************
**                                  Power cuts and faults                                         **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify a record torn by a power cut is skipped and the save before it is recovered: not the defaults, and
*                  not the damaged value
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_torn_latest_record_falls_back_to_the_save_before( void )
{
    test_save_generic_block();

    app_generic_data_blk_g.weld_time_ms = TEST_SECOND_WELD_TIME;
    test_save_block( APP_GENERIC_BLOCK_ID );

    *(volatile u8_t*)test_latest_generic_payload_address() ^= 0x01u;
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a save the flash refuses leaves the last good record intact: after the power cycle that clears the
*                  fault, the value saved before the failed one is what comes back
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_failed_save_leaves_the_last_good_record_to_recover( void )
{
    test_save_generic_block();

    FLASH_SIM_fail_programs( NVM_BASE_ADDRESS, NVM_TOTAL_SIZE );
    app_generic_data_blk_g.weld_time_ms = TEST_SECOND_WELD_TIME;
    test_save_block( APP_GENERIC_BLOCK_ID );
    TEST_ASSERT_EQUAL( NVM_GEN2_STATE_FAULT, NVM_GEN2_get_state() );

    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );
}

/***************************************************************************************************
**                                         Compaction                                             **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify a block saved once survives the compaction that many saves of another block force: the active
*                  partition fills, the live records move to the other one, and both blocks are recovered from it after a restart
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_block_saved_once_survives_the_compaction_forced_by_many_saves_of_another( void )
{
    static const u8_t chassis_num[APP_CHASSIS_NUM_LEN] = { 'O', 'L', 'D', '-', 'B', 'L', 'O', 'C', 'K', '-', 'S', 'U', 'R', 'V', 'I', 'V' };
    const u32_t       first_seq = NVM_GEN2_get_active_seq();
    u16_t             save;

    STDC_memcpy( app_chassis_num_blk_g.chassis_num, chassis_num, APP_CHASSIS_NUM_LEN );
    test_save_block( APP_CHASSIS_NUM_BLOCK_ID );

    for( save = 1u; ( save <= TEST_MAX_SAVES_TO_FILL_A_PARTITION ) && ( NVM_GEN2_get_active_seq() == first_seq ); save++ )
    {
        app_generic_data_blk_g.weld_time_ms = save;
        test_save_block( APP_GENERIC_BLOCK_ID );
        TMD_TIME_advance_time_ms( 10u );
    }

    TEST_ASSERT_TRUE( NVM_GEN2_get_active_seq() != first_seq );    /* the other partition is active now: it did compact */

    TMD_TIME_advance_time_ms( 500u );    /* the compaction works a few records per tick */
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_MEMORY( chassis_num, app_chassis_num_blk_g.chassis_num, APP_CHASSIS_NUM_LEN );
    TEST_ASSERT_TRUE( app_generic_data_blk_g.weld_time_ms > 1u );    /* the generic block is a late save, not the default and not the first */
}

/*!
****************************************************************************************************
*
*   \brief         Verify a record whose header was torn by a power cut is skipped, the save before it is recovered, and the
*                  next save still reaches the flash: the damaged tail cannot be written over, so it is compacted away
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_torn_record_header_is_skipped_and_the_next_save_still_reaches_the_flash( void )
{
    u32_t seq_before;

    test_save_generic_block();

    app_generic_data_blk_g.weld_time_ms = TEST_SECOND_WELD_TIME;
    test_save_block( APP_GENERIC_BLOCK_ID );

    *(volatile u8_t*)( test_latest_generic_record_address() + offsetof( NVM_GEN2_rec_hdr_st, hdr_crc ) ) ^= 0x01u;
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_SAVED_WELD_TIME_MS, app_generic_data_blk_g.weld_time_ms );

    seq_before = NVM_GEN2_get_active_seq();
    app_generic_data_blk_g.weld_time_ms = TEST_THIRD_WELD_TIME;
    test_save_block( APP_GENERIC_BLOCK_ID );

    TEST_ASSERT_TRUE( NVM_GEN2_get_active_seq() != seq_before );    /* the tail was unusable, so the save compacted into the other partition */

    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( TEST_THIRD_WELD_TIME, app_generic_data_blk_g.weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut when a compaction has just opened the other partition loses only the save in flight: the
*                  half-made partition is discarded, the old one still holds the block saved before and the one saved once
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_as_a_compaction_starts_loses_only_the_save_in_flight( void )
{
    test_assert_a_power_cut_in_the_compaction_state_keeps_the_last_saves( NVM_GEN2_STATE_GC_COPY );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut when every live record is copied but the other partition is not yet valid loses only the
*                  save in flight
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_with_the_records_copied_but_not_committed_loses_only_the_save_in_flight( void )
{
    test_assert_a_power_cut_in_the_compaction_state_keeps_the_last_saves( NVM_GEN2_STATE_GC_FLUSH );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a power cut as the compaction is about to commit loses only the save in flight
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_power_cut_just_before_the_compaction_commits_loses_only_the_save_in_flight( void )
{
    test_assert_a_power_cut_in_the_compaction_state_keeps_the_last_saves( NVM_GEN2_STATE_GC_COMMIT );
}

/***************************************************************************************************
**                                  Version migration                                             **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Verify a record the previous firmware saved (generic block version 1, without total_weld_time_ms) is
*                  recovered by the new one: every field it had, and the new field at 0
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_version_1_record_is_migrated_to_version_2_without_losing_its_fields( void )
{
    test_replace_the_latest_generic_record( 1u, &test_saved_generic_blk_v1_s, (u16_t)sizeof( test_saved_generic_blk_v1_s ) );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT8( test_saved_generic_blk_v1_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
    TEST_ASSERT_EQUAL( test_saved_generic_blk_v1_s.reset_request, app_generic_data_blk_g.reset_request );
    TEST_ASSERT_EQUAL_UINT8( test_saved_generic_blk_v1_s.reset_type, app_generic_data_blk_g.reset_type );
    TEST_ASSERT_EQUAL( test_saved_generic_blk_v1_s.bl_request, app_generic_data_blk_g.bl_request );
    TEST_ASSERT_EQUAL_UINT16( test_saved_generic_blk_v1_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT32( test_saved_generic_blk_v1_s.total_num_welds, app_generic_data_blk_g.total_num_welds );
    TEST_ASSERT_EQUAL_UINT32( 0u, app_generic_data_blk_g.total_weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a migrated block is saved back in the new format, so the next restart reads a version 2 record and
*                  the old one is not migrated again
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_migrated_block_is_saved_back_in_the_new_format( void )
{
    const NVM_GEN2_rec_hdr_st* hdr_p;

    test_replace_the_latest_generic_record( 1u, &test_saved_generic_blk_v1_s, (u16_t)sizeof( test_saved_generic_blk_v1_s ) );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();
    TMD_TIME_advance_time_ms( 500u );    /* the write of the migrated value is requested at registration and goes on a tick */

    hdr_p = (const NVM_GEN2_rec_hdr_st*)test_latest_generic_record_address();

    TEST_ASSERT_EQUAL_UINT8( NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->cfg_p->version, hdr_p->version );
    TEST_ASSERT_EQUAL_UINT16( sizeof( APP_generic_data_blk_st ), hdr_p->data_len );

    test_scramble_the_ram_copies();
    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( test_saved_generic_blk_v1_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT32( test_saved_generic_blk_v1_s.total_num_welds, app_generic_data_blk_g.total_num_welds );
    TEST_ASSERT_EQUAL_UINT32( 0u, app_generic_data_blk_g.total_weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a record of a version nothing migrates from is replaced by the defaults, not read as if it fitted
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_record_of_an_unknown_version_is_replaced_by_the_defaults( void )
{
    test_replace_the_latest_generic_record( 9u, &test_saved_generic_blk_v1_s, (u16_t)sizeof( test_saved_generic_blk_v1_s ) );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT8( APP_GENERIC_DEFAULT_DATA_BLK_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT32( 0u, app_generic_data_blk_g.total_weld_time_ms );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a version 1 record of the wrong length is replaced by the defaults: the migration only trusts the
*                  size version 1 had
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_version_1_record_of_the_wrong_length_is_replaced_by_the_defaults( void )
{
    test_replace_the_latest_generic_record( 1u, &test_saved_generic_blk_v1_s, (u16_t)( sizeof( test_saved_generic_blk_v1_s ) - 2u ) );
    test_scramble_the_ram_copies();

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT8( APP_GENERIC_DEFAULT_DATA_BLK_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
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
    test_save_generic_block();

    *(volatile u8_t*)test_latest_generic_payload_address() ^= 0x01u;
    app_generic_data_blk_g.weld_time_ms = TEST_OVERWRITTEN_WELD_TIME;

    TMD_INIT_restart_ecu();

    TEST_ASSERT_EQUAL_UINT16( APP_GENERIC_DEFAULT_DATA_BLK_s.weld_time_ms, app_generic_data_blk_g.weld_time_ms );
    TEST_ASSERT_EQUAL_UINT8( APP_GENERIC_DEFAULT_DATA_BLK_s.screen_brightness, app_generic_data_blk_g.screen_brightness );
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
    FLASH_SIM_fail_programs( NVM_BASE_ADDRESS, NVM_TOTAL_SIZE );

    test_save_generic_block();

    TEST_ASSERT_EQUAL( NVM_GEN2_STATE_FAULT, NVM_GEN2_get_state() );
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
    FLASH_SIM_fail_programs( NVM_BASE_ADDRESS, NVM_TOTAL_SIZE );
    test_save_generic_block();

    TMD_INIT_restart_ecu();
    test_save_generic_block();

    TEST_ASSERT_EQUAL( NVM_GEN2_STATE_IDLE, NVM_GEN2_get_state() );
    TEST_ASSERT_TRUE( NVM_GEN2_get_block_info( APP_GENERIC_BLOCK_ID )->latest_offset != NVM_GEN2_OFFSET_NONE );
}

/****************************** END OF FILE *******************************************************/
