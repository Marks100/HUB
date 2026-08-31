#ifndef PERSIST_BLK_MIGRATE_H
#define PERSIST_BLK_MIGRATE_H

/*! \file
*               Author: mstewart
*   \brief      Version-migration chains for APP's PERSIST_BLK NVM_GEN2 blocks
*
*   \note       Mirrors Common_Src/FBL_NVM_BLOCKS/FBL_NVM_BLOCKS_MIGRATE.c's pattern: each old
*               struct shape lives in PERSIST_BLK.h next to the current one, a STATIC function
*               here handles exactly one version boundary, and a chained NVM_GEN2_migrate_fn (see
*               NVM_GEN2.h's own note on that typedef) walks through however many stages a given
*               block needs. PERSIST_BLK.c points the affected block's migrate_fn at the chain.
*
*   \note       persist_generic_data_migrate() below is currently the only real chain - added as a
*               worked example (version 1 -> 2, adding total_weld_time_ms) so the pattern could be
*               exercised end-to-end. The other three PERSIST_BLK.c blocks still have no prior
*               layout to migrate from, so their migrate_fn stays NULL_P until one of them changes.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "PERSIST_BLK.h"

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
pass_fail_et persist_generic_data_migrate( u8_t old_version, const u8_t* old_data_p,
                                            u16_t old_data_len, void* current_data_p );

#endif /* PERSIST_BLK_MIGRATE_H */

/****************************** END OF FILE *******************************************************/
