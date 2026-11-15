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
**                              Defines                                                           **
***************************************************************************************************/
/* One row of the table for DATASETn: its header at its start, its payload after the header to its last byte, and its id */
#define DATASET_ROW( n )  { .header_address     = DATASET##n##_START,                           \
                            .code_start_address = DATASET##n##_START + DATASET_HEADER_SIZE,     \
                            .code_end_address   = DATASET##n##_START + DATASET##n##_SIZE - 1ul, \
                            .dataset_id         = DATASET##n##_ID }

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
const HEADER_region_st dataset_table_g[DATASET_COUNT] =
{
    [DATASET_TYRE_PRESSURES] = DATASET_ROW( 0 ),
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
