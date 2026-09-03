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

/* APP's block IDs - must stay within NVM_GEN2_BLOCK_ID_OWNER_B_FIRST..OWNER_B_LAST (APP=owner B,
   FBL=owner A; the ID range is what keeps their blocks apart in the shared NVM partitions).
   Based on OWNER_B_FIRST, not a literal, so a new block can't drift into FBL's range. */
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
   u32_t         total_weld_time_ms;  /* Cumulative ON-time across all welds, ms. Added in v2 -
                                           see app_generic_data_migrate(). */
} APP_generic_data_blk_st;

/* v1 shape (pre total_weld_time_ms) - kept only for app_generic_data_migrate() to read old flash
   records; never construct one directly. */
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
extern const APP_generic_data_blk_st APP_GENERIC_DEFAULT_DATA_BLK_s;

extern APP_generic_data_blk_st app_generic_data_blk_g;
extern APP_key_blk_st          app_key_1_blk_g;
extern APP_key_blk_st          app_key_2_blk_g;
extern APP_chassis_num_blk_st  app_chassis_num_blk_g;

/* FBL-owned blocks (none are APP's - see FBL_NVM_BLOCKS.h), snapshotted once by app_main()'s
   APP_read_fbl_*() calls. Not live mirrors - call the matching function directly for a current
   value. */
extern FBL_fingerprint_blk_st            app_fbl_fingerprint_g;
extern FBL_counter_blk_st                app_fbl_boot_count_g;
extern FBL_counter_blk_st                app_fbl_download_attempt_count_g;
extern FBL_dataset_download_count_blk_st app_fbl_dataset_download_count_g;

/* Config for each block above, used by main.c's NVM_GEN2_register_block() calls. Kept here, not
   INTEGRATION_STUBS.c, since default_data/version/event_fn describe the block's data, not this
   board's hardware. */
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_generic_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_key_1_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_key_2_block_s;
extern const NVM_GEN2_block_cfg_st app_nvm_gen2_chassis_num_block_s;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* Reads FBL's fingerprint straight out of shared NVM (APP doesn't own/register this block).
   app_main() snapshots it once at boot into app_fbl_fingerprint_g; call directly for a current
   value. */
pass_fail_et APP_read_fbl_fingerprint( FBL_fingerprint_blk_st* dest_p );
pass_fail_et APP_read_fbl_boot_count( FBL_counter_blk_st* dest_p );
pass_fail_et APP_read_fbl_download_attempt_count( FBL_counter_blk_st* dest_p );
pass_fail_et APP_read_fbl_dataset_download_count( FBL_dataset_download_count_blk_st* dest_p );

#endif /* APP_NVM_BLOCKS_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
