/*! \file
*               Author: mstewart
*   \brief      BM (Boot Manager) entry point
*
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
#include "SHA256.h"
#include "uECC.h"
#include "secure_boot_public_key.h"

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
/* See BM/linker_script/STM32F103C8_BM_flash.ld - absolute flash addresses BM validates but does
   not itself occupy any section in. */
extern u32_t __fbl_header_start__;
extern u32_t __fbl_code_start__;
extern u32_t __fbl_code_end__;
extern u32_t __app_header_start__;
extern u32_t __app_code_start__;
extern u32_t __app_code_end__;
extern u32_t __dataset0_header_start__;
extern u32_t __dataset0_code_start__;
extern u32_t __dataset0_code_end__;
extern u32_t __dataset1_header_start__;
extern u32_t __dataset1_code_start__;
extern u32_t __dataset1_code_end__;

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
**                              Signature Verification (ECDSA P-256, not yet wired live)         **
***************************************************************************************************/
/* Adapter for uECC_verify() (xCOMMON_MODULES/Src/CRYPTO/micro-ecc) - not shaped to match
   BM_signature_verify_func_t directly (it takes a pre-computed hash + curve object and returns
   int, not false_true_et), so this hashes data_p with SHA-256 and translates the result. Only
   BM-specific glue like this lives here rather than in the shared CRYPTO/ module - see
   xCOMMON_MODULES/Src/CRYPTO/README.md. Not yet assigned to .signature_verify below; switching
   over is a one-line change once a real P-256 keypair replaces secure_boot_public_key.h's
   placeholder. */
STATIC false_true_et signature_verify_ecdsa_p256( const u8_t* data_p, u32_t data_len,
                                                    const u8_t* signature_p,
                                                    const u8_t* public_key_p, u32_t public_key_len )
{
   u8_t       hash[SHA256_DIGEST_SIZE];
   uECC_Curve curve;

   (void)public_key_len;

   SHA256_calculate( data_p, data_len, hash );

   curve = uECC_secp256r1();

   return( uECC_verify( public_key_p, hash, SHA256_DIGEST_SIZE, signature_p, curve ) ? TRUE : FALSE );
}

/***************************************************************************************************
**                              Dataset Table                                                    **
***************************************************************************************************/
/* Two independently field-downloadable 1KB data blocks, carved out of APP's own flash budget (see
   APP/linker_script/STM32F103C8_flash.ld's comment). Not validated automatically anywhere - see
   BM_dataset_*() in BM.c for on-demand presence/CRC/signature checks. Add/remove entries here
   (and the matching extern symbols above, defined in this project's own linker script) to change
   how many datasets this platform has - BM.c itself has no hardcoded count. */
STATIC const bm_dataset_region_st bm_dataset_table_s[] =
{
   { .header_address = (u32_t)&__dataset0_header_start__, .code_start_address = (u32_t)&__dataset0_code_start__, .code_end_address = (u32_t)&__dataset0_code_end__ },
   { .header_address = (u32_t)&__dataset1_header_start__, .code_start_address = (u32_t)&__dataset1_code_start__, .code_end_address = (u32_t)&__dataset1_code_end__ },
};

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
   .app_header_address      = (u32_t)&__app_header_start__,
   .app_code_start_address  = (u32_t)&__app_code_start__,
   .app_code_end_address    = (u32_t)&__app_code_end__,
   .app_presence_pattern    = APP_HEADER_PRESENCE_PATTERN,

   /* FBL memory addresses - FBL carries its own header (see FBL/linker_script/
      STM32F103C8_FBL_flash.ld), validated the same way APP is before BM_jump_to_fbl() jumps */
   .fbl_header_address      = (u32_t)&__fbl_header_start__,
   .fbl_code_start_address  = (u32_t)&__fbl_code_start__,
   .fbl_code_end_address    = (u32_t)&__fbl_code_end__,

   /* Dataset table - see bm_dataset_table_s above */
   .dataset_table_p    = bm_dataset_table_s,
   .dataset_table_size = (u8_t)( sizeof( bm_dataset_table_s ) / sizeof( bm_dataset_table_s[0] ) ),

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
   MCU_JUMP_set_vector_table( (u32_t)&__isr_vector_start );

   BM_init( &bm_config_s );
   BM_run();
}

void main( void )
{
   bm_main();
}

/****************************** END OF FILE *******************************************************/
