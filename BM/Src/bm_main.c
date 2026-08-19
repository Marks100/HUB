/*! \file
*               Author: mstewart
*   \brief      BM (Boot Manager) entry point and platform wiring for STM32F103C8
*
*   Reset handler -> BM_init()/BM_run() (xCOMMON_MODULES/Src/BM) with every platform dependency
*   supplied here via bm_config_s. BM_run() validates the APP header's CRC and (once
*   signature_verify is a real implementation, not the stub below) ECDSA signature, then jumps
*   to APP or FBL - see BM.h for the full decision sequence.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "BM.h"
#include "CHKSUM.h"
#include "CLK_STM32F1.h"
#include "SHARED_RAM.h"
#include "MCU_JUMP.h"
#include "secure_boot_public_key.h"

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
*   \brief         ECDSA signature verification - NOT YET IMPLEMENTED
*
*   \return        FALSE, always
*
*   \note          HUB has no BearSSL (or other ECDSA) library wired in yet - see app_signer's
*                  history for why this is deliberately absent rather than guessed at. Fails
*                  closed: while signature_enabled stays FALSE in bm_config_s below this function
*                  is never called, but if someone flips that flag on before replacing this
*                  implementation, every boot must fail validation, not silently "pass" a check
*                  that was never actually performed.
*
***************************************************************************************************/
STATIC false_true_et signature_verify_not_implemented( const u8_t* code_p,
                                                         u32_t       code_len,
                                                         const u8_t* signature_p,
                                                         const u8_t* public_key_p,
                                                         u32_t       public_key_len )
{
    (void)code_p;
    (void)code_len;
    (void)signature_p;
    (void)public_key_p;
    (void)public_key_len;

    return( FALSE );
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

    /* Signature verification - see signature_verify_not_implemented() above */
    .signature_verify = signature_verify_not_implemented,

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

    /* Signature configuration */
    .firmware_public_key_p    = FIRMWARE_PUBLIC_KEY,
    .firmware_public_key_size = ECDSA_P256_PUBLIC_KEY_SIZE,
    .app_signature_size       = 64u,

    /* Feature flags - signature checking stays off until signature_verify is a real
       implementation (see above); CRC checking is real and on today. */
    .crc_enabled             = TRUE,
    .signature_enabled       = FALSE,
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
