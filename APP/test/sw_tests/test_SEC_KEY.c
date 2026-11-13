/*! \file
*               Author: mstewart
*   \brief      The key the tester's security model answers a seed with (TMD_SEC_make_key) - software test
*
*   \details    By each algorithm, against the published test vectors: the AES-128 CMAC of RFC 4493 and the HMAC-SHA-256 of RFC 4231, from the
*               CRYPTO facade (BearSSL and the facade's own SHA-256, built in the firmware's struct layout like everything the tests link),
*               and the XOR the ECU does today. The key is as long as the seed, the first bytes of the code. No ECU is started: this is the
*               model's arithmetic, so a tester that unlocks a real ECU with a real algorithm can be trusted before the ECU has one.
*/
#include "unity.h"
#include <string.h>

#include "STDC.h"
#include "TMD_SEC.h"

/* RFC 4493 section 4: the AES-128 key, and the 16-byte message of example 2 with its CMAC */
static const u8_t test_cmac_key_c[16]     = { 0x2Bu, 0x7Eu, 0x15u, 0x16u, 0x28u, 0xAEu, 0xD2u, 0xA6u, 0xABu, 0xF7u, 0x15u, 0x88u, 0x09u, 0xCFu, 0x4Fu, 0x3Cu };
static const u8_t test_cmac_message_c[16] = { 0x6Bu, 0xC1u, 0xBEu, 0xE2u, 0x2Eu, 0x40u, 0x9Fu, 0x96u, 0xE9u, 0x3Du, 0x7Eu, 0x11u, 0x73u, 0x93u, 0x17u, 0x2Au };
static const u8_t test_cmac_tag_c[16]     = { 0x07u, 0x0Au, 0x16u, 0xB4u, 0x6Bu, 0x4Du, 0x41u, 0x44u, 0xF7u, 0x9Bu, 0xDDu, 0x9Du, 0xD0u, 0x4Au, 0x28u, 0x7Cu };

/* RFC 4231 test case 2: the key "Jefe", the data "what do ya want for nothing?", and the HMAC-SHA-256 */
static const u8_t test_hmac_key_c[4]      = { 0x4Au, 0x65u, 0x66u, 0x65u };
static const u8_t test_hmac_data_c[28]    = { 'w', 'h', 'a', 't', ' ', 'd', 'o', ' ', 'y', 'a', ' ', 'w', 'a', 'n', 't', ' ', 'f', 'o', 'r', ' ', 'n', 'o', 't', 'h', 'i', 'n', 'g', '?' };
static const u8_t test_hmac_mac_c[32]     = { 0x5Bu, 0xDCu, 0xC1u, 0x46u, 0xBFu, 0x60u, 0x75u, 0x4Eu, 0x6Au, 0x04u, 0x24u, 0x26u, 0x08u, 0x95u, 0x75u, 0xC7u,
                                              0x5Au, 0x00u, 0x3Fu, 0x08u, 0x9Du, 0x27u, 0x39u, 0x83u, 0x9Du, 0xECu, 0x58u, 0xB9u, 0x64u, 0xECu, 0x38u, 0x43u };

/* RFC 4231 test case 1: a 20-byte key of 0x0B, the data "Hi There", and the first 8 bytes of the HMAC-SHA-256 */
static const u8_t test_hmac_key_1_c[20]   = { 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu, 0x0Bu };
static const u8_t test_hmac_data_1_c[8]   = { 'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e' };
static const u8_t test_hmac_mac_1_c[8]    = { 0xB0u, 0x34u, 0x4Cu, 0x61u, 0xD8u, 0xDBu, 0x38u, 0x53u };

void setUp( void )
{
}

void tearDown( void )
{
}

/*!
****************************************************************************************************
*
*   \brief         Verify the XOR key is the seed XOR the secret, the ECU's own: a 4-byte seed and secret
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_xor_key_is_the_seed_xor_the_secret( void )
{
    const u8_t secret[4]   = { 0xA5u, 0xA5u, 0xA5u, 0xA5u };
    const u8_t seed[4]     = { 0x01u, 0x02u, 0x03u, 0x04u };
    const u8_t expected[4] = { 0xA4u, 0xA7u, 0xA6u, 0xA1u };
    u8_t       key[4];

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, secret, sizeof( secret ), seed, sizeof( seed ), key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected, key, sizeof( key ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a XOR secret shorter than the seed repeats along it, and one longer is not all used
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_xor_secret_repeats_along_a_longer_seed( void )
{
    const u8_t short_secret[2]   = { 0xFFu, 0x00u };
    const u8_t long_secret[8]    = { 0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u };
    const u8_t seed[4]           = { 0x11u, 0x22u, 0x33u, 0x44u };
    const u8_t expected_short[4] = { 0xEEu, 0x22u, 0xCCu, 0x44u };
    const u8_t expected_long[4]  = { 0x10u, 0x20u, 0x37u, 0x4Cu };
    u8_t       key[4];

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, short_secret, sizeof( short_secret ), seed, sizeof( seed ), key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_short, key, sizeof( key ) );

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, long_secret, sizeof( long_secret ), seed, sizeof( seed ), key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( expected_long, key, sizeof( key ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the AES-128 CMAC key of a 16-byte seed is the RFC 4493 tag, and of a shorter seed is a key of that length
*                  and no more, which is not a piece of that tag: a shorter seed is a shorter message
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_aes_cmac_key_is_the_rfc_4493_tag_and_as_long_as_the_seed( void )
{
    u8_t key[16];

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, sizeof( test_cmac_key_c ), test_cmac_message_c, 16u, key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( test_cmac_tag_c, key, 16u );

    memset( key, 0xEEu, sizeof( key ) );
    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, sizeof( test_cmac_key_c ), test_cmac_message_c, 4u, key ) );
    TEST_ASSERT_TRUE( memcmp( key, test_cmac_tag_c, 4u ) != 0 );
    TEST_ASSERT_EQUAL_HEX8( 0xEEu, key[4] );    /* no more of the key than the seed is long */
}

/*!
****************************************************************************************************
*
*   \brief         Verify the HMAC-SHA-256 key of the RFC 4231 messages is the first bytes of their HMAC, as many as the seed is long
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_hmac_sha256_key_is_the_rfc_4231_mac_as_long_as_the_seed( void )
{
    u8_t key[TMD_SEC_KEY_LEN_MAX];

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, test_hmac_key_c, sizeof( test_hmac_key_c ), test_hmac_data_c, sizeof( test_hmac_data_c ), key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( test_hmac_mac_c, key, sizeof( test_hmac_data_c ) );    /* the 28 bytes of the seed: the first 28 of the MAC */

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, test_hmac_key_1_c, sizeof( test_hmac_key_1_c ), test_hmac_data_1_c, sizeof( test_hmac_data_1_c ), key ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( test_hmac_mac_1_c, key, sizeof( test_hmac_data_1_c ) );    /* an 8-byte seed and a 20-byte secret */
}

/*!
****************************************************************************************************
*
*   \brief         Verify a different seed or a different secret gives a different key, for each real algorithm
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_different_seed_or_secret_gives_a_different_key( void )
{
    u8_t other_seed[16];
    u8_t other_secret[16];
    u8_t a[16];
    u8_t b[16];

    memcpy( other_seed, test_cmac_message_c, sizeof( other_seed ) );
    other_seed[15] ^= 0x01u;
    memcpy( other_secret, test_cmac_key_c, sizeof( other_secret ) );
    other_secret[0] ^= 0x01u;

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, 16u, test_cmac_message_c, 16u, a ) );
    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, 16u, other_seed, 16u, b ) );
    TEST_ASSERT_TRUE( memcmp( a, b, 16u ) != 0 );
    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, other_secret, 16u, test_cmac_message_c, 16u, b ) );
    TEST_ASSERT_TRUE( memcmp( a, b, 16u ) != 0 );

    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, test_cmac_key_c, 16u, test_cmac_message_c, 16u, a ) );
    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, test_cmac_key_c, 16u, other_seed, 16u, b ) );
    TEST_ASSERT_TRUE( memcmp( a, b, 16u ) != 0 );
    TEST_ASSERT_EQUAL( PASS, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, other_secret, 16u, test_cmac_message_c, 16u, b ) );
    TEST_ASSERT_TRUE( memcmp( a, b, 16u ) != 0 );
}

/*!
****************************************************************************************************
*
*   \brief         Verify what does not fit an algorithm is a FAIL and writes no key: an AES-CMAC secret that is not 16 bytes, a seed
*                  of more than a tag, an empty seed or secret, a seed too long for any, no pointer, an algorithm that is not known
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_what_does_not_fit_an_algorithm_is_a_fail_and_writes_no_key( void )
{
    u8_t key[TMD_SEC_KEY_LEN_MAX + 1u];

    memset( key, 0x5Au, sizeof( key ) );

    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, 15u, test_cmac_message_c, 16u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_AES128_CMAC, test_cmac_key_c, 16u, test_hmac_data_c, 17u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, test_cmac_key_c, 0u, test_cmac_message_c, 4u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, test_cmac_key_c, 4u, test_cmac_message_c, 0u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_HMAC_SHA256, test_hmac_key_c, 4u, test_hmac_data_c, TMD_SEC_KEY_LEN_MAX + 1u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, NULL_P, 4u, test_cmac_message_c, 4u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, test_cmac_key_c, 4u, NULL_P, 4u, key ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( TMD_SEC_ALGO_XOR, test_cmac_key_c, 4u, test_cmac_message_c, 4u, NULL_P ) );
    TEST_ASSERT_EQUAL( FAIL, TMD_SEC_make_key( (TMD_SEC_algo_et)99u, test_cmac_key_c, 4u, test_cmac_message_c, 4u, key ) );

    TEST_ASSERT_EQUAL_HEX8( 0x5Au, key[0] );    /* nothing of a failed key was written */
}

/****************************** END OF FILE *******************************************************/
