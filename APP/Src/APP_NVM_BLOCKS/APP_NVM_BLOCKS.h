#ifndef APP_NVM_BLOCKS_H
#define APP_NVM_BLOCKS_H

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
    APP_GENERIC_BLOCK_ID = NVM_GEN2_BLOCK_ID_OWNER_B_FIRST,
    APP_KEY_1_BLOCK_ID,
    APP_KEY_2_BLOCK_ID,
    APP_CHASSIS_NUM_BLOCK_ID,
    APP_BLOCK_ID_MAX
} APP_block_id_et;

#define APP_KEY_LEN          ( 128u )
#define APP_CHASSIS_NUM_LEN  ( 16u )

typedef struct
{
    u8_t          screen_brightness;
    false_true_et reset_request;
    u8_t          reset_type;
    false_true_et bl_request;
    u16_t         weld_time_ms;
    u32_t         total_num_welds;
    u32_t         total_weld_time_ms;  /* Cumulative ON-time across every weld, milliseconds. Added
                                           in version 2 - see app_generic_data_migrate() in
                                           APP_NVM_BLOCKS_MIGRATE.c for the upgrade from version 1;
                                           that file's worked example. */
} APP_generic_data_blk_st;

/* Version 1 - before total_weld_time_ms existed. Kept ONLY so app_generic_data_migrate() (see
   APP_NVM_BLOCKS_MIGRATE.c) can read an old record still sitting on flash; nothing else should
   ever construct one. */
typedef struct
{
    u8_t          screen_brightness;
    false_true_et reset_request;
    u8_t          reset_type;
    false_true_et bl_request;
    u16_t         weld_time_ms;
    u32_t         total_num_welds;
} APP_generic_data_blk_v1_st;

typedef struct
{
    u8_t key[APP_KEY_LEN];
} APP_key_blk_st;

typedef struct
{
    u8_t chassis_num[APP_CHASSIS_NUM_LEN];
} APP_chassis_num_blk_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern APP_generic_data_blk_st       app_generic_data_blk_g;
extern const APP_generic_data_blk_st APP_GENERIC_DEFAULT_DATA_BLK_s;

/* Key/chassis blocks have no default consts - their reset value is all-zero, so their
   NVM_GEN2_block_cfg_st.default_data is NULL_P (see INTEGRATION_STUBS.c) rather than pointing at
   a const zero-filled array that would only cost ROM. */
extern APP_key_blk_st         app_key_1_blk_g;
extern APP_key_blk_st         app_key_2_blk_g;
extern APP_chassis_num_blk_st app_chassis_num_blk_g;

/* One-shot debug snapshot of FBL's fingerprint, populated by app_main()'s APP_read_fbl_
   fingerprint() call at start-up - see that function's comment below. Not a live mirror; do not
   write to these. */
extern FBL_fingerprint_blk_st app_fbl_fingerprint_g;
extern pass_fail_et           app_fbl_fingerprint_result_g;

/* Same one-shot-snapshot pattern as app_fbl_fingerprint_g above, for FBL's remaining NVM
   blocks - none of these are APP's own (all inside NVM_GEN2_BLOCK_ID_OWNER_A_FIRST..OWNER_A_LAST,
   see FBL_NVM_BLOCKS.h), so there is no RAM mirror for any of them either. Populated by
   app_main()'s matching APP_read_fbl_*() calls at start-up. Not live mirrors; do not write to
   these - call the matching APP_read_fbl_*() function directly for a current value. */
extern FBL_counter_blk_st app_fbl_boot_count_g;
extern pass_fail_et       app_fbl_boot_count_result_g;

extern FBL_counter_blk_st app_fbl_download_attempt_count_g;
extern pass_fail_et       app_fbl_download_attempt_count_result_g;

extern FBL_dataset_download_count_blk_st app_fbl_dataset_download_count_g;
extern pass_fail_et                      app_fbl_dataset_download_count_result_g;

/* NVM_GEN2_block_cfg_st for each block above, passed to NVM_GEN2_register_block() from
   APP/Src/MAIN/main.c. Defined here rather than APP/Src/INT_STUBS/INTEGRATION_STUBS.c: default_
   data/version/event_fn are properties of the block's data, not of this board's hardware, so they
   belong with the data they describe. INTEGRATION_STUBS.c keeps only nvm_gen2_hw_interface_st -
   the actual FLS_STM32F1_* wiring - and the register_block() calls themselves. */
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_generic_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_key_1_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_key_2_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_chassis_num_block_s;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* Reads FBL's fingerprint block - APP does not own or register this block (see APP_block_id_et's
   comment: it's inside NVM_GEN2_BLOCK_ID_OWNER_A_FIRST..OWNER_A_LAST, not APP's range), so there
   is no RAM mirror for it. This reads it straight out of the shared NVM partitions each call.
   app_main() calls this once at start-up (after NVM_GEN2_init()) and stores the result into
   app_fbl_fingerprint_g/app_fbl_fingerprint_result_g purely so a debugger has something to watch;
   anything else needing a live/current read should call this directly instead of relying on that
   snapshot. */
pass_fail_et APP_read_fbl_fingerprint( FBL_fingerprint_blk_st* dest_p );

/* Same shape/contract as APP_read_fbl_fingerprint() above, one per remaining FBL-owned block - see
   FBL_NVM_BLOCKS.h for what each block actually holds. */
pass_fail_et APP_read_fbl_boot_count( FBL_counter_blk_st* dest_p );
pass_fail_et APP_read_fbl_download_attempt_count( FBL_counter_blk_st* dest_p );
pass_fail_et APP_read_fbl_dataset_download_count( FBL_dataset_download_count_blk_st* dest_p );

#endif /* APP_NVM_BLOCKS_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
