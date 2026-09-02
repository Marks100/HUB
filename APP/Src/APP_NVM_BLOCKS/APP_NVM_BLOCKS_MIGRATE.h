#ifndef APP_NVM_BLOCKS_MIGRATE_H
#define APP_NVM_BLOCKS_MIGRATE_H

/*! \file
*               Author: mstewart
*   \brief      Version-migration chains for APP's APP_NVM_BLOCKS NVM_GEN2 blocks
*
*   \note       Mirrors Common_Src/FBL_NVM_BLOCKS/FBL_NVM_BLOCKS_MIGRATE.c's pattern: each old
*               struct shape lives in APP_NVM_BLOCKS.h next to the current one, a STATIC function
*               here handles exactly one version boundary, and a chained NVM_GEN2_migrate_fn (see
*               NVM_GEN2.h's own note on that typedef) walks through however many stages a given
*               block needs. APP_NVM_BLOCKS.c points the affected block's migrate_fn at the chain.
*
*   \note       app_generic_data_migrate() below is currently the only real chain - added as a
*               worked example (version 1 -> 2, adding total_weld_time_ms) so the pattern could be
*               exercised end-to-end. The other three APP_NVM_BLOCKS.c blocks still have no prior
*               layout to migrate from, so their migrate_fn stays NULL_P until one of them changes.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "APP_NVM_BLOCKS.h"

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
pass_fail_et app_generic_data_migrate( u8_t old_version, const u8_t* old_data_p,
                                        u16_t old_data_len, void* current_data_p );

#endif /* APP_NVM_BLOCKS_MIGRATE_H */

/****************************** END OF FILE *******************************************************/
