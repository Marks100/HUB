/*! \file
*               Author: mstewart
*   \brief      BM CRYPTO configuration - only what an 8 KB bootloader region can afford.
*               Included by CRYPTO.h, which has already defined the CRYPTO_BACKEND_xxx ids.
*/
#ifndef CRYPTO_CONFIG_H
#define CRYPTO_CONFIG_H

/* Live: HMAC-SHA256 signature check */
#define CRYPTO_CFG_HMAC_SHA256           ( CRYPTO_BACKEND_SHA256_CUSTOM )

/* Compiled but not yet assigned to .signature_verify - see bm_main.c. Costs no flash until it is */
#define CRYPTO_CFG_SHA256                ( CRYPTO_BACKEND_SHA256_CUSTOM )
#define CRYPTO_CFG_ECDSA_P256_VERIFY     ( CRYPTO_BACKEND_MICRO_ECC )

/* Ed25519 verify alone is ~16.6 KB; the rest need BearSSL */
#define CRYPTO_CFG_ED25519_VERIFY        ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_CMAC           ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_CBC            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_CTR            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_GCM            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_RSA_PKCS1V15_DECRYPT  ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_RSA_PKCS1V15_VERIFY   ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_RNG                   ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_HKDF_SHA256           ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_ECB            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_ECC_P256_KEYGEN       ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_ECDH_P256             ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_ECDSA_P256_SIGN       ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES256_CBC            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES256_CTR            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES256_GCM            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_SHA384                ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_SHA512                ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_CHACHA20_POLY1305     ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_AES128_CCM            ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_X25519_KEYGEN         ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_X25519_ECDH           ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_RSA_PKCS1V15_SIGN     ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_ED25519_SIGN          ( CRYPTO_BACKEND_NONE )
#define CRYPTO_CFG_ED25519_KEYGEN        ( CRYPTO_BACKEND_NONE )

#endif /* CRYPTO_CONFIG_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
