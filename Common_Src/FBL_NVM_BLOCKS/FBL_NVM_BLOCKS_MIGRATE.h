#ifndef FBL_NVM_BLOCKS_MIGRATE_H
#define FBL_NVM_BLOCKS_MIGRATE_H

/*! \file
*               Author: mstewart
*   \brief      Version-migration chain for FBL's fingerprint NVM_GEN2 block
*
*   \note       Split out of FBL_NVM_BLOCKS.c: that file owns what FBL persists (the RAM mirrors
*               and NVM_GEN2_block_cfg_st configs), this one owns turning an old on-flash fingerprint
*               record into the current shape. fbl_fingerprint_migrate() is the only thing here with
*               external linkage - it is an NVM_GEN2_migrate_fn, referenced by
*               fbl_nvm_gen2_fingerprint_block_s.migrate_fn back in FBL_NVM_BLOCKS.c. The v1..v4
*               struct shapes it migrates from stay in FBL_NVM_BLOCKS.h, next to the current shape,
*               per that header's own note.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL_NVM_BLOCKS.h"

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
pass_fail_et fbl_fingerprint_migrate( u8_t old_version, const u8_t* old_data_p, u16_t old_data_len,
                                       void* current_data_p );

#endif /* FBL_NVM_BLOCKS_MIGRATE_H */

/****************************** END OF FILE *******************************************************/
