/*
****************************************************************************************************
*
*   \file          seedkey.cpp
*
*   \brief         HUB (BM/FBL/APP) UDS security seed-to-key DLL
*
*   \details       Exports GenerateKeyEx using the full 7-parameter Vector/CANoe signature
*                  (KeyGenerator.h from Vector Informatik GmbH) - same contract CAN_FLASH's
*                  security.py loads via ctypes, and the same shape as CAN_FLASH's other
*                  seedkeydll examples (see examples/seedkeydll/erx2/seedkey.cpp).
*                  Compile as 64-bit DLL - see build.bat.
*
*                  Security level map (iSecurityLevel == seed request subfunction byte):
*                    0x01  Level 1  -  seed XOR 0xA5A5A5A5
*                    0x03  Level 2  -  seed XOR 0xA5A5A5A5  (same algorithm)
*                    0x05  Level 3  -  seed XOR 0xA5A5A5A5  (same algorithm)
*                    0x07  Level 4  -  seed XOR 0xA5A5A5A5  (same algorithm)
*
*                  All four levels are "four doors to one room, not graded access" on both the APP
*                  and FBL side - see APP_SECURITY_LEVEL_*_SEED/_KEY (APP/Src/UDS_CFG/UDS_config.h)
*                  and FBL_SECURITY_LEVEL_*_SEED/_KEY (xCOMMON_MODULES/Src/FBL/FBL.h): every level's
*                  seed/key pair drives the exact same single lock/unlock state on whichever side is
*                  currently active, there is no per-level algorithm difference to encode here.
*
*                  0xA5A5A5A5 is FBL_SECURITY_KEY_XOR_MASK (see xCOMMON_MODULES/Src/FBL/FBL.h) -
*                  the formula FBL_security_verify_key() would check if a real
*                  security_calculate_key_func_p were ever wired on this target (currently
*                  NULL_P, so FBL accepts any key once a seed has been requested - see that
*                  function's comment in FBL.c). APP's own SendKey handler accepts any key
*                  unconditionally regardless of value (its seed is a fixed all-zero placeholder -
*                  see APP/Src/UDS_CFG/UDS_config.c's file header). This DLL computes the one real
*                  formula that exists anywhere in this codebase rather than returning a dummy
*                  value, so it keeps working correctly if/when either side's placeholder is
*                  ever replaced with a real check.
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

/* FBL_SECURITY_KEY_XOR_MASK - xCOMMON_MODULES/Src/FBL/FBL.h. Must stay in lockstep with that
   constant; it is not derived from anything at build time, so a change on the firmware side
   needs the matching change made here by hand. Shared by all four levels - see file header. */
static const uint32_t SECURITY_KEY_XOR_MASK = 0xA5A5A5A5u;

static const uint32_t SEED_BYTES_NEEDED             = 4u;
static const uint32_t KEY_BYTES_4                   = 4u;

/***************************************************************************************************
**                              Private function prototypes                                       **
***************************************************************************************************/

static uint32_t bytes_to_u32_be( const unsigned char* buf );
static void     u32_to_bytes_be( uint32_t val, unsigned char* buf );

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

/***************************************************************************************************
**                              Exported function                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Generate a UDS security key from a seed - Vector/CANoe GenerateKeyEx contract.
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
            case 0x01u:   /* Level 1 - seed XOR 0xA5A5A5A5, 4-byte key */
            case 0x03u:   /* Level 2 - same algorithm, see file header */
            case 0x05u:   /* Level 3 - same algorithm, see file header */
            case 0x07u:   /* Level 4 - same algorithm, see file header */
                if( ( iSeedArraySize < SEED_BYTES_NEEDED ) ||
                    ( iMaxKeyArraySize < KEY_BYTES_4 ) )
                {
                    result = KGRE_BufferToSmall;
                }
                else
                {
                    seed = bytes_to_u32_be( ipSeedArray );
                    key  = seed ^ SECURITY_KEY_XOR_MASK;
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
