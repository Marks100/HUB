#ifndef PERSIST_BLK_H
#define PERSIST_BLK_H

#include "STDC.h"
#include "NVM_GEN2.h"
#include "FBL_NVM_BLOCKS.h"

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/

/* APP's NVM block IDs. These MUST stay inside NVM_GEN2_BLOCK_ID_OWNER_B_FIRST..OWNER_B_LAST: APP
   and FBL share the same two NVM partitions and the ID range is the only thing keeping the two
   images' blocks apart - this project maps APP onto owner B and FBL onto owner A (see
   FBL_NVM_BLOCKS.h's own use of NVM_GEN2_BLOCK_ID_OWNER_A_FIRST). Base the enum on OWNER_B_FIRST
   rather than writing literals so a future block cannot drift into FBL's range by accident. */
typedef enum
{
    PERSIST_BLK_ID_GENERIC = NVM_GEN2_BLOCK_ID_OWNER_B_FIRST,
    PERSIST_BLK_ID_KEY_1,
    PERSIST_BLK_ID_KEY_2,
    PERSIST_BLK_ID_CHASSIS_NUM,
    PERSIST_BLK_ID_MAX
} PERSIST_blk_id_et;

#define PERSIST_KEY_LEN          ( 128u )
#define PERSIST_CHASSIS_NUM_LEN  ( 16u )

typedef struct
{
    u8_t          screen_brightness;
    false_true_et reset_request;
    u8_t          reset_type;
    false_true_et bl_request;
    u16_t         weld_time_ms;
    u32_t         total_num_welds;
    u32_t         total_weld_time_ms;  /* Cumulative ON-time across every weld, milliseconds. Added
                                           in version 2 - see persist_generic_data_migrate() in
                                           PERSIST_BLK_MIGRATE.c for the upgrade from version 1;
                                           that file's worked example. */
} PERSIST_generic_data_blk_st;

/* Version 1 - before total_weld_time_ms existed. Kept ONLY so persist_generic_data_migrate() (see
   PERSIST_BLK_MIGRATE.c) can read an old record still sitting on flash; nothing else should ever
   construct one. */
typedef struct
{
    u8_t          screen_brightness;
    false_true_et reset_request;
    u8_t          reset_type;
    false_true_et bl_request;
    u16_t         weld_time_ms;
    u32_t         total_num_welds;
} PERSIST_generic_data_blk_v1_st;

typedef struct
{
    u8_t key[PERSIST_KEY_LEN];
} PERSIST_key_blk_st;

typedef struct
{
    u8_t chassis_num[PERSIST_CHASSIS_NUM_LEN];
} PERSIST_chassis_num_blk_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern PERSIST_generic_data_blk_st       PERSIST_generic_data_blk_g;
extern const PERSIST_generic_data_blk_st PERSIST_GENERIC_DEFAULT_DATA_BLK_s;

/* Key/chassis blocks have no default consts - their reset value is all-zero, so their
   NVM_GEN2_block_cfg_st.default_data is NULL_P (see INTEGRATION_STUBS.c) rather than pointing at
   a const zero-filled array that would only cost ROM. */
extern PERSIST_key_blk_st         PERSIST_key_1_blk_g;
extern PERSIST_key_blk_st         PERSIST_key_2_blk_g;
extern PERSIST_chassis_num_blk_st PERSIST_chassis_num_blk_g;

/* One-shot debug snapshot of FBL's fingerprint, populated by app_main()'s PERSIST_read_fbl_
   fingerprint() call at start-up - see that function's comment below. Not a live mirror; do not
   write to these. */
extern FBL_fingerprint_blk_st PERSIST_fbl_fingerprint_g;
extern pass_fail_et           PERSIST_fbl_fingerprint_result_g;

/* NVM_GEN2_block_cfg_st for each block above, passed to NVM_GEN2_register_block() from
   APP/Src/MAIN/main.c. Defined here rather than APP/Src/INT_STUBS/INTEGRATION_STUBS.c: default_
   data/version/event_fn are properties of the block's data, not of this board's hardware, so they
   belong with the data they describe. INTEGRATION_STUBS.c keeps only nvm_gen2_hw_interface_st -
   the actual FLS_STM32F1_* wiring - and the register_block() calls themselves. */
extern const NVM_GEN2_block_cfg_st nvm_gen2_persist_block_s;
extern const NVM_GEN2_block_cfg_st nvm_gen2_key_1_block_s;
extern const NVM_GEN2_block_cfg_st nvm_gen2_key_2_block_s;
extern const NVM_GEN2_block_cfg_st nvm_gen2_chassis_num_block_s;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* Reads FBL's fingerprint block - APP does not own or register this block (see PERSIST_blk_id_et's
   comment: it's inside NVM_GEN2_BLOCK_ID_OWNER_A_FIRST..OWNER_A_LAST, not APP's range), so there
   is no RAM mirror for it. This reads it straight out of the shared NVM partitions each call.
   app_main() calls this once at start-up (after NVM_GEN2_init()) and stores the result into
   PERSIST_fbl_fingerprint_g/PERSIST_fbl_fingerprint_result_g purely so a debugger has something
   to watch; anything else needing a live/current read should call this directly instead of
   relying on that snapshot. */
pass_fail_et PERSIST_read_fbl_fingerprint( FBL_fingerprint_blk_st* dest_p );

#endif /* PERSIST_BLK_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/




