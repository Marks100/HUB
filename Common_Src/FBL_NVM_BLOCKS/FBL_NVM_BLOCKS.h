#ifndef FBL_NVM_BLOCKS_H
#define FBL_NVM_BLOCKS_H

/*! \file
*               Author: mstewart
*   \brief      Shape and NVM_GEN2 block IDs of everything FBL persists
*
*   \note       Lives in Common_Src, not xCOMMON_MODULES: the fact that FBL persists a fingerprint/
*               boot count/download-attempt count via NVM_GEN2 at all, and the exact layout of
*               each, is a HUB-project decision, not something the generic FBL.c engine (in
*               xCOMMON_MODULES/Src/FBL) requires of every project that reuses it - fingerprint
*               persistence lives entirely in the platform's own UDS_config.c (project-specific,
*               like the rest of WriteDataByIdentifier), and FBL.c only asks for a
*               download_attempt_notify_func_p callback with a generic signature for the other
*               counter. Common_Src is this project's own shared-across-BM/FBL/APP tree
*               (see rules.mk's COMMON_SRC_DIRS) - the right place for something project-specific
*               that still needs to be visible to more than one image, as opposed to xCOMMON_MODULES
*               (portable across projects) or FBL/Src alone (APP's build cannot see it there).
*
*   \note       Deliberately its own header, not part of FBL.h: FBL.h pulls in the whole bootloader
*               engine (UDS handlers, flash erase state machine, ...), none of which any other
*               image needs. A reader of one of these blocks - currently APP, via
*               NVM_GEN2_read_block( FBL_..._BLOCK_ID, ... ) - only needs the ID and the matching
*               struct to decode what it gets back, so those are the only things worth sharing.
*
*   \note       FBL is the only writer of every block here; nothing in this header changes that.
*               NVM_GEN2_read_block() reads straight out of flash without going through FBL's RAM
*               mirror, so a reader always sees the last value FBL actually committed.
*
*   \note       All four IDs stay inside NVM_GEN2_BLOCK_ID_OWNER_A_FIRST..OWNER_A_LAST (see that
*               constant's own comment in NVM_GEN2.h) - it's the RANGE, not the exact ID, that
*               keeps FBL's and APP's blocks apart, so nothing here needs to avoid APP's IDs by
*               hand. This project maps FBL onto owner A and APP onto owner B (see
*               APP_NVM_BLOCKS.h's own use of NVM_GEN2_BLOCK_ID_OWNER_B_FIRST) - NVM_GEN2 itself
*               has no opinion on which image is which, that mapping is entirely this pair of
*               headers.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "FBL.h"       /* FBL_FINGERPRINT_MAX_LEN */
#include "NVM_GEN2.h"  /* NVM_GEN2_BLOCK_ID_OWNER_A_FIRST */

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/

/* Named here, not left as a bare NVM_GEN2_BLOCK_ID_OWNER_A_FIRST(+N) reference at each call site,
   so both FBL (which registers these) and any reader say "the fingerprint" / "the boot count"
   rather than "the Nth FBL-owned slot, which happens to currently be...". */
#define FBL_FINGERPRINT_BLOCK_ID             ( NVM_GEN2_BLOCK_ID_OWNER_A_FIRST )
#define FBL_BOOT_COUNT_BLOCK_ID              ( NVM_GEN2_BLOCK_ID_OWNER_A_FIRST + 1u )
#define FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID  ( NVM_GEN2_BLOCK_ID_OWNER_A_FIRST + 2u )
#define FBL_DATASET_DOWNLOAD_COUNT_BLOCK_ID  ( NVM_GEN2_BLOCK_ID_OWNER_A_FIRST + 3u )

/* Matches this project's current dataset table size (see fbl_dataset_table_s in FBL/Src/INT_STUBS/
   INTEGRATION_STUBS.c and bm_dataset_table_s in BM/Src/bm_main.c) - NVM_GEN2 blocks are a fixed
   compile-time shape, so this can't track the table size dynamically the way FBL.c/BM.c's own
   dataset_table_size does. Bump this (and both table's entries) together if a dataset is ever
   added or removed. */
#define FBL_DATASET_DOWNLOAD_COUNT_MAX       ( 2u )

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef struct
{
    u32_t download_attempt_count_at_flash; /* fbl_download_attempt_count_g.count at the moment of
                                               the last successful write - how many download
                                               attempts (see FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID,
                                               successful or not) preceded this one. Deliberately
                                               placed FIRST, unlike every field added before it -
                                               proves fbl_fingerprint_migrate() does not depend on
                                               new fields always landing at the end; see
                                               fbl_fingerprint_migrate_v4_to_v5()'s comment for why
                                               field position never matters to a migration stage. */
    u8_t  len;                             /* Bytes actually used in data[], 0..FBL_FINGERPRINT_MAX_LEN */
    u8_t  data[FBL_FINGERPRINT_MAX_LEN];
    u32_t flash_count;                     /* Incremented on every successful fingerprint write - see
                                               fbl_uds_handle_write_fingerprint() in FBL/Src/UDS_CFG/
                                               UDS_config.c. Every write of this DID already means a
                                               flash succeeded - see that function's comment. */
    u32_t last_flash_timestamp_ms;         /* TIME_get_cumulative_run_time_ms_u32() at the moment of
                                               the last successful write - milliseconds since THIS
                                               boot, NOT a wall-clock time (FBL has no RTC). Reset to
                                               a small number on every power cycle; only meaningful
                                               within one power-on session (e.g. "how long after
                                               boot did the tester write this"). */
    u32_t boot_count_at_flash;             /* fbl_boot_count_g.count at the moment of the last
                                               successful write - which power cycle (see
                                               FBL_BOOT_COUNT_BLOCK_ID) the tester flashed this ECU
                                               in, independent of last_flash_timestamp_ms resetting
                                               every boot. */
} FBL_fingerprint_blk_st;

/* Every prior version this block has had lives here, next to the current shape, so the full
   history is in one place - see NVM_GEN2/README.md's migration section. Kept ONLY so
   fbl_fingerprint_migrate() (see FBL_NVM_BLOCKS_MIGRATE.c) can read an old record still sitting on
   flash; nothing else should ever construct one. */

/* Version 1 - before flash_count existed. */
typedef struct
{
    u8_t len;
    u8_t data[FBL_FINGERPRINT_MAX_LEN];
} FBL_fingerprint_v1_blk_st;

/* Version 2 - before last_flash_timestamp_ms existed. */
typedef struct
{
    u8_t  len;
    u8_t  data[FBL_FINGERPRINT_MAX_LEN];
    u32_t flash_count;
} FBL_fingerprint_v2_blk_st;

/* Version 3 - before boot_count_at_flash existed. */
typedef struct
{
    u8_t  len;
    u8_t  data[FBL_FINGERPRINT_MAX_LEN];
    u32_t flash_count;
    u32_t last_flash_timestamp_ms;
} FBL_fingerprint_v3_blk_st;

/* Version 4 - before download_attempt_count_at_flash existed (and before the struct had any
   field ahead of len). */
typedef struct
{
    u8_t  len;
    u8_t  data[FBL_FINGERPRINT_MAX_LEN];
    u32_t flash_count;
    u32_t last_flash_timestamp_ms;
    u32_t boot_count_at_flash;
} FBL_fingerprint_v4_blk_st;

/* Shared by both counter blocks above - two separate NVM_GEN2 registrations (different IDs,
   different RAM mirrors), identical shape, so one type instead of two that would only differ by
   name.
   \note   There is no direct "failed download" counter. Reliably detecting failure would mean
           catching every abort path (security denial, CRC mismatch, timeout, tester giving up
           mid-transfer, power loss...) and persisting before each one - fragile, and some of
           those (power loss) cannot be caught at all. Counting ATTEMPTS instead, against the
           fingerprint block's flash_count (successful completions, above), gives the same answer
           for free and robustly: failed_count = download_attempt_count - flash_count. */
typedef struct
{
    u32_t count;
} FBL_counter_blk_st;

/* Per-dataset accepted-download-attempt counts - same "attempts, not completions" semantics as
   FBL_counter_blk_st/FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID above (see that block's comment for why
   there is no separate "failed" counter), just one count per configured dataset instead of one
   combined total - see FBL_dataset_download_notify_func_t in FBL.h. */
typedef struct
{
    u32_t count[FBL_DATASET_DOWNLOAD_COUNT_MAX]; /* count[i] = accepted RequestDownload attempts
                                                     into dataset_table_p[i] */
} FBL_dataset_download_count_blk_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* RAM mirrors and NVM_GEN2_block_cfg_st configs for the blocks above - defined in
   FBL_NVM_BLOCKS.c, not INTEGRATION_STUBS.c. Everything a config needs (default_data, version,
   event_fn) is a property of the BLOCK, not the board - nothing here touches hardware. The only
   thing that stays in FBL/Src/INT_STUBS/INTEGRATION_STUBS.c is the NVM_GEN2_hw_interface_st (the
   actual FLS_STM32F1_* wiring) and the NVM_GEN2_register_block() calls themselves, since
   registration order/timing is board_init's call. */
extern FBL_fingerprint_blk_st           fbl_fingerprint_g;
extern FBL_counter_blk_st               fbl_boot_count_g;
extern FBL_counter_blk_st               fbl_download_attempt_count_g;
extern FBL_dataset_download_count_blk_st fbl_dataset_download_count_g;

extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_fingerprint_block_s;
extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_boot_count_block_s;
extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_download_attempt_count_block_s;
extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_dataset_download_count_block_s;

#endif /* FBL_NVM_BLOCKS_H */

/****************************** END OF FILE *******************************************************/
