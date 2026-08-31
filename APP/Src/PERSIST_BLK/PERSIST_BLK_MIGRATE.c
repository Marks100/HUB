/*! \file
*               Author: mstewart
*   \brief      Version-migration chains for APP's PERSIST_BLK NVM_GEN2 blocks - see
*               PERSIST_BLK_MIGRATE.h for what belongs here and why
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "PERSIST_BLK_MIGRATE.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void persist_generic_data_migrate_v1_to_v2( const PERSIST_generic_data_blk_v1_st* old_p,
                                                     PERSIST_generic_data_blk_st* new_p );

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Migrates a version-1 generic data record into the version-2 shape
*
*   \author        MS
*
*   \param[in]     old_p   Version-1 record, read straight out of flash
*   \param[out]    new_p   Receives the fully-populated version-2 value
*
*   \return        none
*
*   \note          total_weld_time_ms did not exist in version 1, so there is nothing on flash to
*                  recover it from - it starts at 0, the same value a virgin block would get.
*
***************************************************************************************************/
STATIC void persist_generic_data_migrate_v1_to_v2( const PERSIST_generic_data_blk_v1_st* old_p,
                                                     PERSIST_generic_data_blk_st* new_p )
{
    new_p->screen_brightness  = old_p->screen_brightness;
    new_p->reset_request      = old_p->reset_request;
    new_p->reset_type         = old_p->reset_type;
    new_p->bl_request         = old_p->bl_request;
    new_p->weld_time_ms       = old_p->weld_time_ms;
    new_p->total_num_welds    = old_p->total_num_welds;
    new_p->total_weld_time_ms = 0u;
}

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         NVM_GEN2_migrate_fn for the generic data block
*
*   \author        MS
*
*   \param[in]     old_version     Version stored in the existing record
*   \param[in]     old_data_p      Existing record's payload, in flash
*   \param[in]     old_data_len    Existing record's payload length in bytes
*   \param[out]    current_data_p  Same buffer as PERSIST_generic_data_blk_g - write the migrated
*                                  value here
*
*   \return        PASS if old_version was recognised and migrated, FAIL otherwise (falls back to
*                  defaults - see NVM_GEN2_register_block() in NVM_GEN2.c)
*
*   \note          Only one version boundary exists so far (1 -> 2), so this is a single "if" rather
*                  than FBL_NVM_BLOCKS_MIGRATE.c's fbl_fingerprint_migrate() chain of four - but the
*                  shape matches so a version-3 stage could be appended the same way: reassign
*                  old_version/old_data_p/old_data_len to the version-2 stage's output and fall into
*                  a second "if", exactly as that chain does.
*
***************************************************************************************************/
pass_fail_et persist_generic_data_migrate( u8_t old_version, const u8_t* old_data_p,
                                            u16_t old_data_len, void* current_data_p )
{
    pass_fail_et result = FAIL;

    if( ( old_version == 1u ) && ( old_data_len == sizeof( PERSIST_generic_data_blk_v1_st ) ) )
    {
        persist_generic_data_migrate_v1_to_v2( (const PERSIST_generic_data_blk_v1_st*)old_data_p,
                                                (PERSIST_generic_data_blk_st*)current_data_p );
        result = PASS;
    }

    return( result );
}

/****************************** END OF FILE *******************************************************/
