/*! \file
*               Author: mstewart
*   \brief      DATASET_config - see DATASET_config.h
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "DATASET_config.h"
#include "BOOT_HANDOFF.h"

/***************************************************************************************************
**                              Linker Symbols                                                    **
***************************************************************************************************/
/* DATASET0, as BM's, the FBL's and the application's linker scripts all define it (the same names and values in each): the header at the
   start of the region, the payload after it, and the last byte of both. A host build gives the three addresses as macros instead, so there
   is nothing to link */
#if !defined( DATASET0_HEADER_ADDRESS )
extern u32_t __dataset0_header_start__;
extern u32_t __dataset0_code_start__;
extern u32_t __dataset0_code_end__;

#define DATASET0_HEADER_ADDRESS       ( (u32_t)&__dataset0_header_start__ )
#define DATASET0_CODE_START_ADDRESS   ( (u32_t)&__dataset0_code_start__ )
#define DATASET0_CODE_END_ADDRESS     ( (u32_t)&__dataset0_code_end__ )
#endif

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
const HEADER_region_st dataset_table_g[DATASET_COUNT] =
{
    [DATASET_TYRE_PRESSURES] =
    {
        .header_address     = DATASET0_HEADER_ADDRESS,
        .code_start_address = DATASET0_CODE_START_ADDRESS,
        .code_end_address   = DATASET0_CODE_END_ADDRESS
    },
};

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Find a dataset's payload, if BM accepted the dataset
*
*   \author        MS
*
*   \param[in]     index        Which dataset, a DATASET_index_et
*   \param[out]    payload_pp   The payload, in place in the flash; NULL_P if the dataset is not usable
*   \param[out]    length_p     Its size in bytes (the table's end address is the last byte, so the size is one past the difference); 0 if not
*
*   \return        TRUE if the dataset is in the table and BM recorded it as valid, FALSE if not
*
*   \note          BM records the dataset's result as bit n of the handoff RAM's valid mask on every start, whatever it found, so a
*                  dataset it did not accept is a clear bit and not a missing one. The payload is the region the header's CRC and
*                  signature cover, not the header: code_start_address to code_end_address.
*
***************************************************************************************************/
false_true_et DATASET_get_payload( DATASET_index_et index, const u8_t** payload_pp, u32_t* length_p )
{
    false_true_et usable = FALSE;
    const HEADER_region_st* region_p;

    if( ( payload_pp != NULL_P ) && ( length_p != NULL_P ) )
    {
        *payload_pp = NULL_P;
        *length_p   = 0u;

        if( ( (u32_t)index < (u32_t)DATASET_COUNT ) && ( (u32_t)index < DATASET_MAX_COUNT ) &&
            ( ( BOOT_HANDOFF_get_dataset_valid_mask() & (u8_t)( 1u << index ) ) != 0u ) )
        {
            region_p    = &dataset_table_g[index];
            *payload_pp = (const u8_t*)region_p->code_start_address;
            *length_p   = ( region_p->code_end_address - region_p->code_start_address ) + 1u;
            usable      = TRUE;
        }
    }

    return( usable );
}

/****************************** END OF FILE *******************************************************/
