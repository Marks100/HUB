/*! \file
*               Author: mstewart
*   \brief      BM (Boot Manager) entry point and platform wiring for STM32F103C8
*
*   Reset handler -> BM_init()/BM_run() (xCOMMON_MODULES/Src/BM) with every platform dependency
*   supplied here via bm_config_s. BM_run() validates the APP header's CRC and (once
*   signature_enabled is flipped on - see secure_boot_hmac_secret.h) its HMAC-SHA256 signature,
*   then jumps to APP or FBL - see BM.h for the full decision sequence.
*
*   HMAC-SHA256 chosen as the first algorithm wired up here (over ECDSA/Ed25519, which
*   app_signer also supports) to prove out the full header/CRC/signature/boot pipeline with the
*   simplest primitive first - it's a straight SHA-256 + HMAC construction, no big-integer EC
*   math, so there's very little surface for a subtle implementation bug. BM_signature_verify_func_t
*   is dependency-injected specifically so swapping this for ECDSA/Ed25519 later, once the
*   pipeline is proven, only means changing this file - BM.c and app_signer's interfaces don't move.
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

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
/* See BM/linker_script/STM32F103C8_BM_flash.ld - absolute flash addresses BM validates but does
   not itself occupy any section in. */
extern u32_t __app_start__;
extern u32_t __fbl_start__;
extern u32_t __app_header_start__;
extern u32_t __app_code_start__;
extern u32_t __app_code_end__;
extern u32_t _estack;

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
**                              Signature Verification                                           **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Constant-time byte-buffer comparison
*
*   \author        MS
*
*   \param         a_p    - first buffer
*   \param         b_p    - second buffer
*   \param         length - number of bytes to compare
*
*   \return        TRUE if every byte matches, FALSE otherwise
*
*   \note          Deliberately not STDC_memcompare()/memcmp() - those are free to (and typically
*                  do) return on the first mismatching byte, which leaks how many leading bytes of
*                  an attacker's guess were correct via timing. Always touches every byte,
*                  regardless of where the first difference is.
*
***************************************************************************************************/
STATIC false_true_et constant_time_equal( const u8_t* a_p, const u8_t* b_p, u32_t length )
{
    u8_t  diff = 0u;
    u32_t i;

    for( i = 0u; i < length; i++ )
    {
        diff |= ( a_p[i] ^ b_p[i] );
    }

    return( ( diff == 0u ) ? TRUE : FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         HMAC-SHA256 signature verification
*
*   \author        MS
*
*   \param         code_p          - start of the code region that was hashed/signed
*   \param         code_len        - length of code_p in bytes
*   \param         signature_p     - header's 64-byte signature field; only the first
*                                     HMAC_SHA256_TAG_SIZE (32) bytes are meaningful - app_signer's
*                                     --algorithm=hmac-sha256 zero-pads the rest to fill the same
*                                     fixed-width field ECDSA/Ed25519 also use
*   \param         public_key_p    - shared secret (see secure_boot_hmac_secret.h - despite the
*                                     generic "public_key" name this dependency-injection slot
*                                     shares with every algorithm, this one is NOT public)
*   \param         public_key_len  - shared secret length in bytes
*
*   \return        TRUE only if the computed HMAC-SHA256 tag matches the stored one
*
***************************************************************************************************/
STATIC false_true_et signature_verify_hmac_sha256( const u8_t* code_p,
                                                     u32_t       code_len,
                                                     const u8_t* signature_p,
                                                     const u8_t* public_key_p,
                                                     u32_t       public_key_len )
{
    u8_t computed_tag[HMAC_SHA256_TAG_SIZE];

    HMAC_SHA256_calculate( public_key_p, public_key_len, code_p, code_len, computed_tag );

    return( constant_time_equal( computed_tag, signature_p, HMAC_SHA256_TAG_SIZE ) );
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

    /* Signature verification - see signature_verify_hmac_sha256() above */
    .signature_verify = signature_verify_hmac_sha256,

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
**                              Reset Handler / Vector Table                                     **
***************************************************************************************************/
void BM_Reset_Handler( void );

__attribute__((section(".isr_vector"), used))
const u32_t bm_vector_table[2] =
{
    (u32_t)&_estack,          /* Initial SP: top of 20 KB SRAM, see linker script */
    (u32_t)BM_Reset_Handler,
};

void BM_Reset_Handler( void )
{
    BM_init( &bm_config_s );
    BM_run();
}

/****************************** END OF FILE *******************************************************/
