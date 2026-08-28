/*
****************************************************************************************************
*
*   \file          erx2_seedkey.cpp
*
*   \brief         CCCM3 / Volvo ERX UDS security seed-to-key DLL
*
*   \details       Exports GenerateKeyEx using the full 7-parameter Vector/CANoe signature
*                  (KeyGenerator.h from Vector Informatik GmbH).
*                  Compile as 64-bit DLL — see build.bat.
*
*                  Security level map (iSecurityLevel == seed subfunction byte):
*                    0x01  APP Level  1  — Dual LCG
*                    0x09  APP Level  9  — Dual LCG  (same algorithm)
*                    0x13  FBL Level 13  — Fixed key  (any seed -> 0x55 0xAA)
*                    0x35  FBL Level 35  — LFSR x35,  constant 0x54504D35
*
****************************************************************************************************
*/

#include <cstdint>

#ifdef _WIN32
#  define EXPORT extern "C" __declspec(dllexport)
#else
#  define EXPORT extern "C" __attribute__((visibility("default")))
#endif

/***************************************************************************************************
**                              Type definitions                                                  **
***************************************************************************************************/

enum VKeyGenResultEx
{
    KGRE_Ok                   = 0,
    KGRE_BufferToSmall        = 1,
    KGRE_SecurityLevelInvalid = 2,
    KGRE_VariantInvalid       = 3,
    KGRE_UnspecifiedError     = 4
};

/***************************************************************************************************
**                              Private constants                                                 **
***************************************************************************************************/

static const uint32_t DUAL_LCG_KEY_CONSTANT     = 0x458B7403u;
static const uint32_t DUAL_LCG_SEEDX_MULTIPLIER = 0x187F415Fu;
static const uint32_t DUAL_LCG_SEEDX_INCREMENT  = 0x1F2BA78Du;
static const uint32_t DUAL_LCG_SEEDY_MULTIPLIER = 0x39E21BF9u;
static const uint32_t DUAL_LCG_SEEDY_INCREMENT  = 0x36B3DFC2u;

static const uint32_t LFSR35_CONSTANT           = 0x54504D35u;
static const int      LFSR35_ITERATIONS         = 35;

static const uint32_t SEED_BYTES_NEEDED         = 4u;
static const uint32_t KEY_BYTES_4               = 4u;
static const uint32_t KEY_BYTES_2               = 2u;

/***************************************************************************************************
**                              Private function prototypes                                       **
***************************************************************************************************/

static uint32_t bytes_to_u32_be( const unsigned char* buf );
static void     u32_to_bytes_be( uint32_t val, unsigned char* buf );
static uint32_t lfsr35(          uint32_t seed, uint32_t constant );
static uint32_t dual_lcg(        uint32_t seed );

/***************************************************************************************************
**                              Private function implementations                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Pack four big-endian bytes into a uint32.
*
*   \param[in]     buf   Pointer to at least 4 bytes.
*
*   \return        uint32_t assembled from buf[0..3] in big-endian order.
*
***************************************************************************************************/
static uint32_t bytes_to_u32_be( const unsigned char* buf )
{
    uint32_t result;

    result  = ( static_cast<uint32_t>( buf[0] ) << 24u );
    result |= ( static_cast<uint32_t>( buf[1] ) << 16u );
    result |= ( static_cast<uint32_t>( buf[2] ) <<  8u );
    result |=   static_cast<uint32_t>( buf[3] );

    return result;
}

/*!
****************************************************************************************************
*
*   \brief         Write a uint32 as four big-endian bytes.
*
*   \param[in]     val   Value to write.
*   \param[out]    buf   Pointer to at least 4 bytes.
*
*   \return        void
*
***************************************************************************************************/
static void u32_to_bytes_be( uint32_t val, unsigned char* buf )
{
    buf[0] = static_cast<unsigned char>( ( val >> 24u ) & 0xFFu );
    buf[1] = static_cast<unsigned char>( ( val >> 16u ) & 0xFFu );
    buf[2] = static_cast<unsigned char>( ( val >>  8u ) & 0xFFu );
    buf[3] = static_cast<unsigned char>(   val           & 0xFFu );
}

/*!
****************************************************************************************************
*
*   \brief         LFSR-based key derivation — 35 shift-register iterations.
*
*   \param[in]     seed      32-bit seed value.
*   \param[in]     constant  XOR constant applied on each '1' bit shift.
*
*   \return        Derived 32-bit key.
*
***************************************************************************************************/
static uint32_t lfsr35( uint32_t seed, uint32_t constant )
{
    uint32_t key = seed;
    int      i;

    if( key != 0u )
    {
        for( i = 0; i < LFSR35_ITERATIONS; i++ )
        {
            if( ( key & 0x80000000u ) != 0u )
            {
                key = ( ( key << 1u ) & 0xFFFFFFFFu ) ^ constant;
            }
            else
            {
                key = ( key << 1u ) & 0xFFFFFFFFu;
            }
        }
    }

    return key;
}

/*!
****************************************************************************************************
*
*   \brief         Dual LCG key derivation.
*
*   \details       Two linear congruential generators are chained:
*                    x = LCG_X( seed XOR KEY_CONSTANT )
*                    y = LCG_Y( x    XOR KEY_CONSTANT )
*                    key = x XOR y
*                  Seeds of 0x00000000 and 0xFFFFFFFF yield key 0x00000000 (invalid seed guard).
*
*   \param[in]     seed   32-bit seed value.
*
*   \return        Derived 32-bit key.
*
***************************************************************************************************/
static uint32_t dual_lcg( uint32_t seed )
{
    uint32_t x;
    uint32_t y;
    uint32_t key;

    if( ( seed == 0u ) || ( seed == 0xFFFFFFFFu ) )
    {
        key = 0u;
    }
    else
    {
        x = ( seed ^ DUAL_LCG_KEY_CONSTANT ) & 0xFFFFFFFFu;
        x = static_cast<uint32_t>(
                ( static_cast<uint64_t>( x ) * DUAL_LCG_SEEDX_MULTIPLIER
                  + DUAL_LCG_SEEDX_INCREMENT ) & 0xFFFFFFFFu );

        y = ( x ^ DUAL_LCG_KEY_CONSTANT ) & 0xFFFFFFFFu;
        y = static_cast<uint32_t>(
                ( static_cast<uint64_t>( y ) * DUAL_LCG_SEEDY_MULTIPLIER
                  + DUAL_LCG_SEEDY_INCREMENT ) & 0xFFFFFFFFu );

        key = ( x ^ y ) & 0xFFFFFFFFu;
    }

    return key;
}

/***************************************************************************************************
**                              Exported function                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Generate a UDS security key from a seed — Vector/CANoe GenerateKeyEx contract.
*
*   \param[in]     ipSeedArray           Pointer to the seed byte array (big-endian).
*   \param[in]     iSeedArraySize        Number of bytes in the seed array.
*   \param[in]     iSecurityLevel        UDS security level (== seed request subfunction byte).
*   \param[in]     ipVariant             Optional variant string (unused, may be NULL).
*   \param[out]    iopKeyArray           Buffer to receive the computed key bytes (big-endian).
*   \param[in]     iMaxKeyArraySize      Capacity of iopKeyArray in bytes.
*   \param[out]    oActualKeyArraySize   Set to the number of key bytes written.
*
*   \return        KGRE_Ok on success, or an appropriate KGRE_* error code.
*
***************************************************************************************************/
EXPORT VKeyGenResultEx GenerateKeyEx(
    const unsigned char* ipSeedArray,
    unsigned int         iSeedArraySize,
    const unsigned int   iSecurityLevel,
    const char*          ipVariant,
    unsigned char*       iopKeyArray,
    unsigned int         iMaxKeyArraySize,
    unsigned int&        oActualKeyArraySize )
{
    VKeyGenResultEx result;
    uint32_t        seed;
    uint32_t        key;

    (void)ipVariant;

    if( ( ipSeedArray == 0 ) || ( iopKeyArray == 0 ) )
    {
        result = KGRE_UnspecifiedError;
    }
    else
    {
        switch( iSecurityLevel )
        {
            case 0x01u:   /* APP Level  1 — Dual LCG, 4-byte key */
            case 0x09u:   /* APP Level  9 — Dual LCG, 4-byte key */
                if( ( iSeedArraySize < SEED_BYTES_NEEDED ) ||
                    ( iMaxKeyArraySize < KEY_BYTES_4 ) )
                {
                    result = KGRE_BufferToSmall;
                }
                else
                {
                    seed = bytes_to_u32_be( ipSeedArray );
                    key  = dual_lcg( seed );
                    u32_to_bytes_be( key, iopKeyArray );
                    oActualKeyArraySize = KEY_BYTES_4;
                    result = KGRE_Ok;
                }
                break;

            case 0x13u:   /* FBL Level 13 — fixed 2-byte key 0x55 0xAA */
                if( iMaxKeyArraySize < KEY_BYTES_2 )
                {
                    result = KGRE_BufferToSmall;
                }
                else
                {
                    iopKeyArray[0]      = 0x55u;
                    iopKeyArray[1]      = 0xAAu;
                    oActualKeyArraySize = KEY_BYTES_2;
                    result = KGRE_Ok;
                }
                break;

            case 0x35u:   /* FBL Level 35 — LFSR x35, 4-byte key */
                if( ( iSeedArraySize < SEED_BYTES_NEEDED ) ||
                    ( iMaxKeyArraySize < KEY_BYTES_4 ) )
                {
                    result = KGRE_BufferToSmall;
                }
                else
                {
                    seed = bytes_to_u32_be( ipSeedArray );
                    key  = lfsr35( seed, LFSR35_CONSTANT );
                    u32_to_bytes_be( key, iopKeyArray );
                    oActualKeyArraySize = KEY_BYTES_4;
                    result = KGRE_Ok;
                }
                break;

            default:
                result = KGRE_SecurityLevelInvalid;
                break;
        }
    }

    return result;
}
