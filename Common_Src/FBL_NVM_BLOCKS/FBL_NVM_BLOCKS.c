/*! \file
*               Author: mstewart
*   \brief      Storage for everything FBL persists - see FBL_NVM_BLOCKS.h for the shapes and IDs
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL_NVM_BLOCKS.h"
#include "FBL_NVM_BLOCKS_MIGRATE.h"

FBL_fingerprint_blk_st            fbl_fingerprint_g;
FBL_counter_blk_st                fbl_boot_count_g;
FBL_counter_blk_st                fbl_download_attempt_count_g;
FBL_dataset_download_count_blk_st fbl_dataset_download_count_g;

/* version bumped 1 -> 2 -> 3 -> 4 -> 5 as flash_count, then last_flash_timestamp_ms, then
   boot_count_at_flash, then download_attempt_count_at_flash were added. migrate_fn preserves an
   existing v1..v4 record's fields across all four bumps instead of losing them to defaults - see
   fbl_fingerprint_migrate()'s \note for how the chain works. default_data is NULL_P below rather
   than a const zero-filled struct - the reset value here is all-zero anyway (for a genuinely
   virgin block; a v1..v4-record device goes through migrate_fn instead), so NULL_P has NVM_GEN2
   zero-fill the mirror directly and costs no ROM - see NVM_GEN2_block_cfg_st.default_data's
   comment in NVM_GEN2.h. */
const NVM_GEN2_block_cfg_st fbl_nvm_gen2_fingerprint_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_fingerprint_g,
    .data_len     = sizeof( fbl_fingerprint_g ),
    .version      = 5u,
    .event_fn     = NULL_P,
    .migrate_fn   = fbl_fingerprint_migrate
};

/* Both default to NULL_P/zero for the same reason the fingerprint's default does. See this file's
   header for why there is no separate "failed downloads" counter. migrate_fn is NULL_P - neither
   has ever been anything but version 1, so there is no prior layout to migrate from yet. */
const NVM_GEN2_block_cfg_st fbl_nvm_gen2_boot_count_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_boot_count_g,
    .data_len     = sizeof( fbl_boot_count_g ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

const NVM_GEN2_block_cfg_st fbl_nvm_gen2_download_attempt_count_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_download_attempt_count_g,
    .data_len     = sizeof( fbl_download_attempt_count_g ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

/* Defaults to NULL_P/zero for the same reason the others do; never been anything but version 1. */
const NVM_GEN2_block_cfg_st fbl_nvm_gen2_dataset_download_count_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_dataset_download_count_g,
    .data_len     = sizeof( fbl_dataset_download_count_g ),
    .version      = 1u,
    .event_fn     = NULL_P,
    .migrate_fn   = NULL_P
};

/****************************** END OF FILE *******************************************************/
