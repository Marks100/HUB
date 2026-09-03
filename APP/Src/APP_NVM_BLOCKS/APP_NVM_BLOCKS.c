/*! \file
*               Author: mstewart
*   \brief      Storage for everything APP persists, plus debug snapshots of FBL's NVM blocks -
*               see APP_NVM_BLOCKS.h for the shapes and IDs
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "APP_NVM_BLOCKS.h"
#include "APP_NVM_BLOCKS_MIGRATE.h"

/* Module Identification for assert functionality */
#define STDC_MODULE_ID APP_NVM_BLOCKS

const APP_generic_data_blk_st APP_GENERIC_DEFAULT_DATA_BLK_s =
{
	23u,
    FALSE,
    0u,
    FALSE,
    40,
    0u,
    0u
};

APP_generic_data_blk_st app_generic_data_blk_g;
APP_key_blk_st          app_key_1_blk_g;
APP_key_blk_st          app_key_2_blk_g;
APP_chassis_num_blk_st  app_chassis_num_blk_g;

/* Populated once at start-up by app_main()'s APP_read_fbl_fingerprint() call - this is NOT a
   live mirror the way app_generic_data_blk_g is: FBL owns this block, APP never registers it,
   so there is nothing to keep in sync after boot. It exists so the fingerprint has somewhere to
   sit in RAM for a debugger to watch, rather than requiring a manual flash dump every time.
   Anything that needs a live/current read should call APP_read_fbl_fingerprint() directly
   instead of relying on this snapshot. */
FBL_fingerprint_blk_st            app_fbl_fingerprint_g;
FBL_counter_blk_st                app_fbl_boot_count_g;
FBL_counter_blk_st                app_fbl_download_attempt_count_g;
FBL_dataset_download_count_blk_st app_fbl_dataset_download_count_g;

/*!
****************************************************************************************************
*
*   \brief         Read FBL's fingerprint block out of the shared NVM partitions
*
*   \author        MS
*
*   \param[out]    dest_p   Receives the fingerprint block
*
*   \return        PASS if a valid fingerprint record exists and was copied out, FAIL if dest_p is
*                  NULL_P or FBL has never written one (e.g. a never-flashed unit)
*
***************************************************************************************************/
pass_fail_et APP_read_fbl_fingerprint( FBL_fingerprint_blk_st* dest_p )
{
    pass_fail_et result = FAIL;

    if( dest_p != NULL_P )
    {
        result = NVM_GEN2_read_block( FBL_FINGERPRINT_BLOCK_ID,
                                      (u8_t*)dest_p,
                                      (u16_t)sizeof( FBL_fingerprint_blk_st ),
                                      NULL_P );
    }

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief         Read FBL's boot count block out of the shared NVM partitions
*
*   \author        MS
*
*   \param[out]    dest_p   Receives the counter block
*
*   \return        PASS if a valid record exists and was copied out, FAIL if dest_p is NULL_P or
*                  FBL has never written one
*
***************************************************************************************************/
pass_fail_et APP_read_fbl_boot_count( FBL_counter_blk_st* dest_p )
{
    pass_fail_et result = FAIL;

    if( dest_p != NULL_P )
    {
        result = NVM_GEN2_read_block( FBL_BOOT_COUNT_BLOCK_ID,
                                      (u8_t*)dest_p,
                                      (u16_t)sizeof( FBL_counter_blk_st ),
                                      NULL_P );
    }

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief         Read FBL's download attempt count block out of the shared NVM partitions
*
*   \author        MS
*
*   \param[out]    dest_p   Receives the counter block
*
*   \return        PASS if a valid record exists and was copied out, FAIL if dest_p is NULL_P or
*                  FBL has never written one
*
***************************************************************************************************/
pass_fail_et APP_read_fbl_download_attempt_count( FBL_counter_blk_st* dest_p )
{
    pass_fail_et result = FAIL;

    if( dest_p != NULL_P )
    {
        result = NVM_GEN2_read_block( FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID,
                                      (u8_t*)dest_p,
                                      (u16_t)sizeof( FBL_counter_blk_st ),
                                      NULL_P );
    }

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief         Read FBL's per-dataset download count block out of the shared NVM partitions
*
*   \author        MS
*
*   \param[out]    dest_p   Receives the counter block (count[i] per dataset_table_p[i] - see
*                  FBL_NVM_BLOCKS.h)
*
*   \return        PASS if a valid record exists and was copied out, FAIL if dest_p is NULL_P or
*                  FBL has never written one
*
***************************************************************************************************/
pass_fail_et APP_read_fbl_dataset_download_count( FBL_dataset_download_count_blk_st* dest_p )
{
    pass_fail_et result = FAIL;

    if( dest_p != NULL_P )
    {
        result = NVM_GEN2_read_block( FBL_DATASET_DOWNLOAD_COUNT_BLOCK_ID,
                                      (u8_t*)dest_p,
                                      (u16_t)sizeof( FBL_dataset_download_count_blk_st ),
                                      NULL_P );
    }

    return( result );
}

/* version bumped 1 -> 2 when total_weld_time_ms was added - see APP_generic_data_blk_st's
   comment in APP_NVM_BLOCKS.h and app_generic_data_migrate() in APP_NVM_BLOCKS_MIGRATE.c, which
   preserves an existing version-1 record's fields across the bump instead of losing them to
   defaults. */
const NVM_GEN2_block_cfg_st app_nvm_gen2_generic_block_s =
{
    .default_data = &APP_GENERIC_DEFAULT_DATA_BLK_s,
    .current_data = &app_generic_data_blk_g,
    .data_len     = sizeof( APP_generic_data_blk_st ),
    .version      = 2u,
    .event_fn     = NULL_P,
    .migrate_fn   = app_generic_data_migrate
};

const NVM_GEN2_block_cfg_st app_nvm_gen2_key_1_block_s =
{
    .default_data = NULL_P,
    .current_data = &app_key_1_blk_g,
    .data_len     = sizeof( APP_key_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

const NVM_GEN2_block_cfg_st app_nvm_gen2_key_2_block_s =
{
    .default_data = NULL_P,
    .current_data = &app_key_2_blk_g,
    .data_len     = sizeof( APP_key_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

const NVM_GEN2_block_cfg_st app_nvm_gen2_chassis_num_block_s =
{
    .default_data = NULL_P,
    .current_data = &app_chassis_num_blk_g,
    .data_len     = sizeof( APP_chassis_num_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

/****************************** END OF FILE *******************************************************/
