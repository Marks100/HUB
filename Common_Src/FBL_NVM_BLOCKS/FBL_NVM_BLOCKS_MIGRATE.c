/*! \file
*               Author: mstewart
*   \brief      Version-migration chain for FBL's fingerprint NVM_GEN2 block - see
*               FBL_NVM_BLOCKS_MIGRATE.h for why this is split out of FBL_NVM_BLOCKS.c
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL_NVM_BLOCKS_MIGRATE.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void fbl_fingerprint_migrate_v1_to_v2( const FBL_fingerprint_v1_blk_st* old_p, FBL_fingerprint_v2_blk_st* new_p );
STATIC void fbl_fingerprint_migrate_v2_to_v3( const FBL_fingerprint_v2_blk_st* old_p, FBL_fingerprint_v3_blk_st* new_p );
STATIC void fbl_fingerprint_migrate_v3_to_v4( const FBL_fingerprint_v3_blk_st* old_p, FBL_fingerprint_v4_blk_st* new_p );
STATIC void fbl_fingerprint_migrate_v4_to_v5( const FBL_fingerprint_v4_blk_st* old_p, FBL_fingerprint_blk_st* new_p );

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Migrates a version-1 fingerprint record into the version-2 shape
*
*   \author        MS
*
*   \param[in]     old_p   Version-1 record, read straight out of flash
*   \param[out]    new_p   Receives the fully-populated version-2 value
*
*   \return        none
*
*   \note          flash_count did not exist in version 1, so there is nothing on flash to
*                  recover it from - it starts at 0, the same value a virgin block would get.
*
***************************************************************************************************/
STATIC void fbl_fingerprint_migrate_v1_to_v2( const FBL_fingerprint_v1_blk_st* old_p, FBL_fingerprint_v2_blk_st* new_p )
{
    new_p->len = old_p->len;
    STDC_memcpy( new_p->data, old_p->data, FBL_FINGERPRINT_MAX_LEN );
    new_p->flash_count = 0u;
}

/*!
****************************************************************************************************
*
*   \brief         Migrates a version-2 fingerprint record into the version-3 shape
*
*   \author        MS
*
*   \param[in]     old_p   Version-2 record, either read straight out of flash or produced by
*                          fbl_fingerprint_migrate_v1_to_v2() one stage back
*   \param[out]    new_p   Receives the fully-populated version-3 value
*
*   \return        none
*
*   \note          last_flash_timestamp_ms did not exist in version 2, so there is nothing on
*                  flash to recover it from - it starts at 0.
*
***************************************************************************************************/
STATIC void fbl_fingerprint_migrate_v2_to_v3( const FBL_fingerprint_v2_blk_st* old_p, FBL_fingerprint_v3_blk_st* new_p )
{
    new_p->len = old_p->len;
    STDC_memcpy( new_p->data, old_p->data, FBL_FINGERPRINT_MAX_LEN );
    new_p->flash_count             = old_p->flash_count;
    new_p->last_flash_timestamp_ms = 0u;
}

/*!
****************************************************************************************************
*
*   \brief         Migrates a version-3 fingerprint record into the current (version 4) shape
*
*   \author        MS
*
*   \param[in]     old_p   Version-3 record, either read straight out of flash or produced by
*                          fbl_fingerprint_migrate_v2_to_v3() one stage back
*   \param[out]    new_p   Receives the fully-populated current-version value
*
*   \return        none
*
*   \note          boot_count_at_flash did not exist in version 3, so there is nothing on flash to
*                  recover it from - it starts at 0.
*
***************************************************************************************************/
STATIC void fbl_fingerprint_migrate_v3_to_v4( const FBL_fingerprint_v3_blk_st* old_p, FBL_fingerprint_v4_blk_st* new_p )
{
    new_p->len = old_p->len;
    STDC_memcpy( new_p->data, old_p->data, FBL_FINGERPRINT_MAX_LEN );
    new_p->flash_count             = old_p->flash_count;
    new_p->last_flash_timestamp_ms = old_p->last_flash_timestamp_ms;
    new_p->boot_count_at_flash     = 0u;
}

/*!
****************************************************************************************************
*
*   \brief         Migrates a version-4 fingerprint record into the current (version 5) shape
*
*   \author        MS
*
*   \param[in]     old_p   Version-4 record, either read straight out of flash or produced by
*                          fbl_fingerprint_migrate_v3_to_v4() one stage back
*   \param[out]    new_p   Receives the fully-populated current-version value
*
*   \return        none
*
*   \note          download_attempt_count_at_flash did not exist in version 4, so there is
*                  nothing on flash to recover it from - it starts at 0, same as every other
*                  genuinely-new field this chain has migrated so far.
*
*   \note          Unlike every field before it, this one is FIRST in FBL_fingerprint_blk_st, not
*                  appended last - and this function reads/writes every field by NAME (old_p->len,
*                  old_p->data, ...), never by raw memcpy of the whole struct or by assuming
*                  matching offsets between old_p and new_p. So field position never mattered to
*                  any stage in this chain; putting the new field first here changes nothing about
*                  how migration works, only proves it.
*
***************************************************************************************************/
STATIC void fbl_fingerprint_migrate_v4_to_v5( const FBL_fingerprint_v4_blk_st* old_p, FBL_fingerprint_blk_st* new_p )
{
    new_p->len = old_p->len;
    STDC_memcpy( new_p->data, old_p->data, FBL_FINGERPRINT_MAX_LEN );
    new_p->flash_count                      = old_p->flash_count;
    new_p->last_flash_timestamp_ms          = old_p->last_flash_timestamp_ms;
    new_p->boot_count_at_flash              = old_p->boot_count_at_flash;
    new_p->download_attempt_count_at_flash  = 0u;
}

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         NVM_GEN2_migrate_fn for the fingerprint block
*
*   \author        MS
*
*   \param[in]     old_version     Version stored in the existing record
*   \param[in]     old_data_p      Existing record's payload, in flash
*   \param[in]     old_data_len    Existing record's payload length in bytes
*   \param[out]    current_data_p  Same buffer as fbl_fingerprint_g - write the migrated value here
*
*   \return        PASS if old_version was recognised and migrated, FAIL otherwise (falls back to
*                  defaults - see NVM_GEN2_register_block() in NVM_GEN2.c)
*
*   \note          CHAINED, not direct-to-current: each stage only encodes what changed at that one
*                  version boundary, then hands off to the next by reassigning old_version/
*                  old_data_p/old_data_len to point at its own output before falling into the next
*                  "if". A v1 record falls through all four stages below; a v2 record enters at
*                  the second; a v3 record enters at the third; a v4 record enters at the fourth -
*                  all reuse whatever later stages they need rather than each having its own
*                  direct-to-current converter. The "if"s are deliberately not "else if": after a
*                  stage reassigns old_version, the next "if" has to still be reachable, which
*                  "else if" would prevent.
*
***************************************************************************************************/
pass_fail_et fbl_fingerprint_migrate( u8_t old_version, const u8_t* old_data_p, u16_t old_data_len, void* current_data_p )
{
    pass_fail_et               result = FAIL;
    FBL_fingerprint_v2_blk_st  v2_stage;
    FBL_fingerprint_v3_blk_st  v3_stage;
    FBL_fingerprint_v4_blk_st  v4_stage;

    if( ( old_version == 1u ) && ( old_data_len == sizeof( FBL_fingerprint_v1_blk_st ) ) )
    {
        fbl_fingerprint_migrate_v1_to_v2( (const FBL_fingerprint_v1_blk_st*)old_data_p, &v2_stage );

        old_version  = 2u;
        old_data_p   = (const u8_t*)&v2_stage;
        old_data_len = sizeof( v2_stage );
    }

    if( ( old_version == 2u ) && ( old_data_len == sizeof( FBL_fingerprint_v2_blk_st ) ) )
    {
        fbl_fingerprint_migrate_v2_to_v3( (const FBL_fingerprint_v2_blk_st*)old_data_p, &v3_stage );

        old_version  = 3u;
        old_data_p   = (const u8_t*)&v3_stage;
        old_data_len = sizeof( v3_stage );
    }

    if( ( old_version == 3u ) && ( old_data_len == sizeof( FBL_fingerprint_v3_blk_st ) ) )
    {
        fbl_fingerprint_migrate_v3_to_v4( (const FBL_fingerprint_v3_blk_st*)old_data_p, &v4_stage );

        old_version  = 4u;
        old_data_p   = (const u8_t*)&v4_stage;
        old_data_len = sizeof( v4_stage );
    }

    if( ( old_version == 4u ) && ( old_data_len == sizeof( FBL_fingerprint_v4_blk_st ) ) )
    {
        fbl_fingerprint_migrate_v4_to_v5( (const FBL_fingerprint_v4_blk_st*)old_data_p,
                                          (FBL_fingerprint_blk_st*)current_data_p );
        result = PASS;
    }

    return( result );
}

/****************************** END OF FILE *******************************************************/
