/*! \file
*               Author: mstewart
*   \brief      Firmware signing public key (ECDSA P-256) - PLACEHOLDER, not yet generated
*
*   All zero bytes on purpose - this is NOT a real key. BM_app_signature_is_valid() in BM.c
*   fails closed regardless of this array's contents (signature_enabled is FALSE and
*   signature_verify is a stub in BM/Src/bm_main.c until real ECDSA verification is wired up -
*   see that file's signature_verify_not_implemented()), so a placeholder here doesn't create a
*   false sense of security: nothing currently trusts this key.
*
*   To generate the real one once BearSSL (or another ECDSA implementation) is wired in:
*   HUB already has a keypair at Tool_cfg/SigningKeys/signing_key.pem (private) and
*   signing_key_public.pem (public) - app_signer already signs with the private half. Extract
*   the public half's raw X||Y coordinates (64 bytes, no leading 0x04 point-format byte) with:
*
*     openssl ec -in Tool_cfg/SigningKeys/signing_key_public.pem -pubin -text -noout
*
*   and take the "pub:" byte dump, drop the leading 04 byte, and paste the remaining 64 bytes
*   below. Do NOT copy a key from another project (e.g. AUTOCFG_HUB) - it must be the public
*   half of the exact private key app_signer uses, or every signature will fail to verify.
*/
#ifndef SECURE_BOOT_PUBLIC_KEY_H
#define SECURE_BOOT_PUBLIC_KEY_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define ECDSA_P256_PUBLIC_KEY_SIZE ( 64u ) /* Raw X||Y, 32 bytes each - no point-format prefix */

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/

/* PLACEHOLDER - see file header. Not the real public key yet. */
static const u8_t FIRMWARE_PUBLIC_KEY[ECDSA_P256_PUBLIC_KEY_SIZE] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#endif /* SECURE_BOOT_PUBLIC_KEY_H */

/****************************** END OF FILE *******************************************************/
