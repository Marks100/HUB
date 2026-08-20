/*! \file
*               Author: mstewart
*   \brief      BM (Boot Manager) entry point and platform wiring for STM32F103C8
*
*   Reset handler -> BM_init()/BM_run() (xCOMMON_MODULES/Src/BM) with every platform dependency
*   supplied here via bm_config_s. BM_run() validates the APP header's CRC and (once
*   signature_enabled is flipped on - see secure_boot_hmac_secret.h) its HMAC-SHA256 signature,
*   then jumps to APP or FBL - see BM.h for the full decision sequence.
*
*   HMAC-SHA256 chosen as the first algorithm wired up here (over ECDSA, which app_signer also
*   supports) to prove out the full header/CRC/signature/boot pipeline with the simplest primitive
*   first - it's a straight SHA-256 + HMAC construction, no big-integer EC math, so there's very
*   little surface for a subtle implementation bug. BM_signature_verify_func_t is dependency-
*   injected specifically so swapping this for ECDSA later only means changing this file - BM.c
*   and app_signer's interfaces don't move.
*
*   Ed25519 (xCOMMON_MODULES/Src/CRYPTO/ed25519) was evaluated and ruled out for this target: its
*   verify path costs ~16.6KB (ref10-derived, heavily unrolled sc_reduce/sha512_compress), which
*   doesn't fit BM's 8KB region. ECDSA P-256 via micro-ecc costs ~3.5KB instead (generic bignum
*   code, not unrolled) and reuses the SHA-256 already needed for HMAC - see
*   xCOMMON_MODULES/Src/CRYPTO/ECDSA_P256/ECDSA_P256_verify.c for the validated-but-not-yet-live
*   implementation.
*
*   HMAC_SHA256_verify()/ECDSA_P256_verify() are shaped to match BM_signature_verify_func_t
*   exactly and assigned to .signature_verify directly below - no adapter needed here, the same
*   way .crc_calculate takes CHKSUM_calc_hw_crc32 straight from the CHKSUM module.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "BM.h"
#include "CHKSUM.h"
#include "CLK_STM32F1.h"
#include "SHARED_RAM.h"
#include "MCU_JUMP.h"
#include "HMAC_SHA256.h"
#include "secure_boot_hmac_secret.h"
#include "ECDSA_P256_verify.h"
#include "secure_boot_public_key.h"

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
/* See BM/linker_script/STM32F103C8_BM_flash.ld - absolute flash addresses BM validates but does
   not itself occupy any section in. */
extern u32_t __app_start__;
extern u32_t __fbl_start__;
extern u32_t __fbl_header_start__;
extern u32_t __fbl_code_start__;
extern u32_t __fbl_code_end__;
extern u32_t __app_header_start__;
extern u32_t __app_code_start__;
extern u32_t __app_code_end__;

/***************************************************************************************************
**                              CRC Configuration                                                **
***************************************************************************************************/
/* CRC32 IEEE 802.3 - must match app_crc_injector's default --crc-type crc32 (see
   BuildEnv/xBuildEnv/bin/app_crc_injector). STM32F103's hardware CRC only supports this
   polynomial - see CHKSUM.h's hw_crc_config_st comment. */
STATIC const hw_crc_config_st hw_crc_cfg_s =
{
    .width             = HW_CRC_WIDTH_32BIT,
    .polynomial        = 0x04C11DB7UL,
    .seed              = 0xFFFFFFFFUL,
    .complement_result = TRUE,
};

STATIC void crc_init( void )
{
    CHKSUM_init_hw_crc( &hw_crc_cfg_s );
}

/***************************************************************************************************
**                              Clock Configuration                                              **
***************************************************************************************************/
/* Same profile as APP (see APP/Src/MAIN/main.c) - BM hands off to APP without APP redoing clock
   init on the fast path, so the two must agree on what SYSCLK already is. */
STATIC void clk_init( void )
{
    CLK_STM32F1_init( &hse8_72mhz_s );
}

/***************************************************************************************************
**                              Boot Manager Configuration                                       **
***************************************************************************************************/
STATIC const bm_config_st bm_config_s =
{
    /* Hardware initialisation */
    .clk_init  = clk_init,
    .wdg_init  = NULL_P,  /* APP controls the watchdog, matching AUTOCFG_HUB's BM convention */
    .wdg_kick  = NULL_P,
    .crc_init  = crc_init,

    /* CRC calculation */
    .crc_calculate = CHKSUM_calc_hw_crc32,

    /* Signature verification - see HMAC_SHA256_verify() in xCOMMON_MODULES/Src/CRYPTO/HMAC_SHA256 */
    .signature_verify = HMAC_SHA256_verify,

    /* Platform-specific jump function */
    .jump_to_address = MCU_JUMP_to_address,

    /* Shared RAM interface */
    .shared_ram_init               = SHARED_RAM_init,
    .shared_ram_set_fbl_request    = SHARED_RAM_set_fbl_request,
    .shared_ram_get_fbl_request    = SHARED_RAM_get_fbl_request,
    .shared_ram_set_failure_reason = SHARED_RAM_set_last_failure_reason,
    .shared_ram_get_failure_reason = SHARED_RAM_get_last_failure_reason,
    .shared_ram_is_valid           = SHARED_RAM_is_valid,

    /* Memory addresses from the linker script */
    .app_start_address       = (u32_t)&__app_start__,
    .fbl_start_address       = (u32_t)&__fbl_start__,
    .app_header_address      = (u32_t)&__app_header_start__,
    .app_code_start_address  = (u32_t)&__app_code_start__,
    .app_code_end_address    = (u32_t)&__app_code_end__,
    .app_presence_pattern    = APP_HEADER_PRESENCE_PATTERN,

    /* FBL memory addresses - FBL carries its own header (see FBL/linker_script/
       STM32F103C8_FBL_flash.ld), validated the same way APP is before BM_jump_to_fbl() jumps */
    .fbl_header_address      = (u32_t)&__fbl_header_start__,
    .fbl_code_start_address  = (u32_t)&__fbl_code_start__,
    .fbl_code_end_address    = (u32_t)&__fbl_code_end__,

    /* Signature configuration - see secure_boot_hmac_secret.h before touching this */
    .firmware_public_key_p    = FIRMWARE_HMAC_SECRET,
    .firmware_public_key_size = FIRMWARE_HMAC_SECRET_SIZE,
    .app_signature_size       = HMAC_SHA256_TAG_SIZE,

    /* Feature flags - FIRMWARE_HMAC_SECRET is now a real generated secret (see
       secure_boot_hmac_secret.h), so both checks are live. Every APP build must be signed with
       app_signer --algorithm=hmac-sha256 using the matching Tool_cfg/SigningKeys/hmac_secret.txt,
       or BM_run() traps rather than jumping to an unsigned/wrongly-signed image. */
    .crc_enabled             = TRUE,
    .signature_enabled       = TRUE,
    .bypass_validity_checks  = FALSE,
};

/***************************************************************************************************
**                              Entry Point                                                      **
***************************************************************************************************/
/* Same split as APP/Src/MAIN/main.c's app_main()/main() - the vector table, Reset_Handler
   (.data/.bss init), SystemInit() and the call into this function all come from
   startup_stm32f10x_md.c (BM_C_SRCS in BM/Makefile), unmodified, exactly as APP uses it. BM no
   longer overrides any of that itself, so both partitions boot through the identical startup
   flow - see BM/Makefile's BM_C_SRCS comment for what that flow does to BM specifically. */
extern u32_t __isr_vector_start;   /* Linker symbol - BM/linker_script/STM32F103C8_BM_flash.ld */

void bm_main( void )
{
    /* Re-assert VTOR from the linker's own placement of .isr_vector, not a hand-maintained
       constant - see MCU_JUMP_set_vector_table()'s comment. Correct for BM by
       coincidence even without this (BM's table happens to sit at SystemInit()'s FLASH_BASE
       default), but calling it here anyway keeps all three partitions doing the identical thing
       rather than BM being the one exception nobody has to explain. */
    MCU_JUMP_set_vector_table( (u32_t)&__isr_vector_start );

    BM_init( &bm_config_s );
    BM_run();
}

void main( void )
{
    bm_main();
}

/****************************** END OF FILE *******************************************************/
