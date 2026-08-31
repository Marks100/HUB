/*! \file
*               Author: mstewart
*   \brief      Storage for everything FBL persists - see FBL_NVM_BLOCKS.h for the shapes and IDs
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL_NVM_BLOCKS.h"

FBL_fingerprint_blk_st fbl_fingerprint_g;
FBL_counter_blk_st     fbl_boot_count_g;
FBL_counter_blk_st     fbl_download_attempt_count_g;

/* version bumped 1 -> 2 for the added flash_count field. An on-flash record from before this
   change has the old (shorter) data_len, so NVM_GEN2 treats it as a version mismatch and falls
   back to defaults rather than misreading its bytes - see NVM_GEN2/README.md's "version or
   length change discards the block" note. default_data is NULL_P below rather than a const
   zero-filled struct - the reset value here is all-zero anyway, so NULL_P has NVM_GEN2 zero-fill
   the mirror directly and costs no ROM - see NVM_GEN2_block_cfg_st.default_data's comment in
   NVM_GEN2.h. */
const NVM_GEN2_block_cfg_st fbl_nvm_gen2_fingerprint_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_fingerprint_g,
    .data_len     = sizeof( fbl_fingerprint_g ),
    .version      = 2u,
    .event_fn     = NULL_P
};

/* Both default to NULL_P/zero for the same reason the fingerprint's default does. See this file's
   header for why there is no separate "failed downloads" counter. */
const NVM_GEN2_block_cfg_st fbl_nvm_gen2_boot_count_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_boot_count_g,
    .data_len     = sizeof( fbl_boot_count_g ),
    .version      = 1u,
    .event_fn     = NULL_P
};

const NVM_GEN2_block_cfg_st fbl_nvm_gen2_download_attempt_count_block_s =
{
    .default_data = NULL_P,
    .current_data = &fbl_download_attempt_count_g,
    .data_len     = sizeof( fbl_download_attempt_count_g ),
    .version      = 1u,
    .event_fn     = NULL_P
};

/****************************** END OF FILE *******************************************************/
