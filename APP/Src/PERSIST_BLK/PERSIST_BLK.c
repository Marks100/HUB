/*! \file
*               Author: mstewart
*   \brief      Persistance  module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "PERSIST_BLK.h"

/* Module Identification for assert functionality */
#define STDC_MODULE_ID PERSIST_BLK

const PERSIST_generic_data_blk_st PERSIST_GENERIC_DEFAULT_DATA_BLK_s =
{
	23u,
    FALSE,
    0u,
    FALSE,
    40,
    0u
};

PERSIST_generic_data_blk_st PERSIST_generic_data_blk_g;

/* No default consts here - see the extern block in PERSIST_BLK.h for why. */
PERSIST_key_blk_st         PERSIST_key_1_blk_g;
PERSIST_key_blk_st         PERSIST_key_2_blk_g;
PERSIST_chassis_num_blk_st PERSIST_chassis_num_blk_g;

/* Populated once at start-up by PERSIST_read_fbl_fingerprint_at_boot() (called from app_main()) -
   this is NOT a live mirror the way PERSIST_generic_data_blk_g is: FBL owns this block, APP never
   registers it, so there is nothing to keep in sync after boot. It exists so the fingerprint has
   somewhere to sit in RAM for a debugger to watch, rather than requiring a manual flash dump every
   time. PERSIST_fbl_fingerprint_result_g distinguishes "never flashed" (FAIL, block stays
   zeroed) from a genuine all-zero fingerprint, which PERSIST_fbl_fingerprint_g alone cannot. */
FBL_fingerprint_blk_st  PERSIST_fbl_fingerprint_g;
pass_fail_et            PERSIST_fbl_fingerprint_result_g = FAIL;

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
pass_fail_et PERSIST_read_fbl_fingerprint( FBL_fingerprint_blk_st* dest_p )
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
*   \brief         Read FBL's fingerprint block into PERSIST_fbl_fingerprint_g, once, at start-up
*
*   \author        MS
*
*   \return        none
*
*   \note          Call once from app_main(), after NVM_GEN2_init(). Exists purely to give a
*                  debugger something to watch - see PERSIST_fbl_fingerprint_g's comment. Anything
*                  that needs a live/current read should call PERSIST_read_fbl_fingerprint()
*                  directly instead of relying on this snapshot.
*
***************************************************************************************************/
void PERSIST_read_fbl_fingerprint_at_boot( void )
{
    PERSIST_fbl_fingerprint_result_g = PERSIST_read_fbl_fingerprint( &PERSIST_fbl_fingerprint_g );
}

const NVM_GEN2_block_cfg_st nvm_gen2_persist_block_s =
{
    .default_data = &PERSIST_GENERIC_DEFAULT_DATA_BLK_s,
    .current_data = &PERSIST_generic_data_blk_g,
    .data_len     = sizeof( PERSIST_generic_data_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P
};

/* default_data = NULL_P on these three: their reset value is all-zero, so NVM_GEN2 zero-fills
   the RAM mirror itself rather than this needing a const zero-filled array to point at - see
   NVM_GEN2_block_cfg_st.default_data's comment in NVM_GEN2.h. */
const NVM_GEN2_block_cfg_st nvm_gen2_key_1_block_s =
{
    .default_data = NULL_P,
    .current_data = &PERSIST_key_1_blk_g,
    .data_len     = sizeof( PERSIST_key_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P
};

const NVM_GEN2_block_cfg_st nvm_gen2_key_2_block_s =
{
    .default_data = NULL_P,
    .current_data = &PERSIST_key_2_blk_g,
    .data_len     = sizeof( PERSIST_key_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P
};

const NVM_GEN2_block_cfg_st nvm_gen2_chassis_num_block_s =
{
    .default_data = NULL_P,
    .current_data = &PERSIST_chassis_num_blk_g,
    .data_len     = sizeof( PERSIST_chassis_num_blk_st ),
    .version      = 1u,
    .event_fn     = NULL_P
};

/****************************** END OF FILE *******************************************************/
