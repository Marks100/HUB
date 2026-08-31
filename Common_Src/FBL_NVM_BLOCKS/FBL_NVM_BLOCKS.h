#ifndef FBL_NVM_BLOCKS_H
#define FBL_NVM_BLOCKS_H

/*! \file
*               Author: mstewart
*   \brief      Shape and NVM_GEN2 block IDs of everything FBL persists
*
*   \note       Lives in Common_Src, not xCOMMON_MODULES: the fact that FBL persists a fingerprint/
*               boot count/download-attempt count via NVM_GEN2 at all, and the exact layout of
*               each, is a HUB-project decision, not something the generic FBL.c engine (in
*               xCOMMON_MODULES/Src/FBL) requires of every project that reuses it - it only asks
*               for a fingerprint_write_func_p/download_attempt_notify_func_p callback with a
*               generic signature. Common_Src is this project's own shared-across-BM/FBL/APP tree
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
*   \note       All three IDs stay inside NVM_GEN2_BLOCK_ID_OWNER_A_FIRST..OWNER_A_LAST (see that
*               constant's own comment in NVM_GEN2.h) - it's the RANGE, not the exact ID, that
*               keeps FBL's and APP's blocks apart, so nothing here needs to avoid APP's IDs by
*               hand. This project maps FBL onto owner A and APP onto owner B (see PERSIST_BLK.h's
*               own use of NVM_GEN2_BLOCK_ID_OWNER_B_FIRST) - NVM_GEN2 itself has no opinion on
*               which image is which, that mapping is entirely this pair of headers.
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

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef struct
{
    u8_t  len;                             /* Bytes actually used in data[], 0..FBL_FINGERPRINT_MAX_LEN */
    u8_t  data[FBL_FINGERPRINT_MAX_LEN];
    u32_t flash_count;                     /* Incremented on every successful fingerprint write - see
                                               fbl_fingerprint_write() in FBL/Src/INT_STUBS/
                                               INTEGRATION_STUBS.c. Every write of this DID already
                                               means a flash succeeded - see that function's comment. */
} FBL_fingerprint_blk_st;

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

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* RAM mirrors and NVM_GEN2_block_cfg_st configs for the three blocks above - defined in
   FBL_NVM_BLOCKS.c, not INTEGRATION_STUBS.c. Everything a config needs (default_data, version,
   event_fn) is a property of the BLOCK, not the board - nothing here touches hardware. The only
   thing that stays in FBL/Src/INT_STUBS/INTEGRATION_STUBS.c is the NVM_GEN2_hw_interface_st (the
   actual FLS_STM32F1_* wiring) and the NVM_GEN2_register_block() calls themselves, since
   registration order/timing is board_init's call. */
extern FBL_fingerprint_blk_st fbl_fingerprint_g;
extern FBL_counter_blk_st     fbl_boot_count_g;
extern FBL_counter_blk_st     fbl_download_attempt_count_g;

extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_fingerprint_block_s;
extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_boot_count_block_s;
extern const NVM_GEN2_block_cfg_st fbl_nvm_gen2_download_attempt_count_block_s;

#endif /* FBL_NVM_BLOCKS_H */

/****************************** END OF FILE *******************************************************/
