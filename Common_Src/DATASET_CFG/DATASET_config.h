/*! \file
*               Author: mstewart
*   \brief      DATASET_config - the one table of this product's downloadable datasets: where each is in the flash
*
*   BM validates every dataset, the FBL downloads into them and the application reads their payloads, and all three
*   need the same three addresses of each. They are here once, as dataset_table_g, and each image takes the table
*   (BM and the FBL in their configuration, the application where it reads a dataset) instead of naming the linker
*   symbols itself.
*
*   The table is built from the linker symbols in DATASET_config.c. A host build has no linker script, so it gives
*   the three addresses of DATASET0 as macros instead (DATASET0_HEADER_ADDRESS, DATASET0_CODE_START_ADDRESS and
*   DATASET0_CODE_END_ADDRESS, in its build flags) and compiles the same file: that is why the table holds addresses and
*   not pointers.
*
*   DATASET_get_payload() is how a module that reads a dataset is given its payload: the table joined to BM's verdict
*   (bit n of the handoff RAM's valid mask for dataset n). The module checks the format of what it is given and
*   nothing else.
*
*   To add a dataset: add its linker symbols to the three linker scripts, an index here and its row in
*   DATASET_config.c. FBL_DATASET_DOWNLOAD_COUNT_MAX (FBL_NVM_BLOCKS.h) follows DATASET_COUNT, and NVM_GEN2 resets the
*   stored download counts at the first start after, as that block's length changes.
*/
#ifndef DATASET_CONFIG_H
#define DATASET_CONFIG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "HEADER.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define DATASET_MAX_COUNT  ( 8u )    /*!< BM's valid mask is one byte, one bit per dataset */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
/*! \brief The position of each dataset in dataset_table_g, and the table's size */
typedef enum
{
    DATASET_TYRE_PRESSURES = 0u,    /*!< DATASET0: the tyre pressure table (APP's TYRE_DATA) */
    DATASET_COUNT
} DATASET_index_et;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* One row per DATASET_index_et: header, payload start and payload end (inclusive) in the flash */
extern const HEADER_region_st dataset_table_g[DATASET_COUNT];

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/*!< TRUE if the dataset is in the table and BM found it valid (presence, CRC and signature): *payload_pp is its payload, in the flash, and
 *   *length_p its size in bytes. FALSE, with NULL_P and 0 in them, if not, so a caller can hand them straight on. Either pointer NULL_P is FALSE */
false_true_et DATASET_get_payload( DATASET_index_et index, const u8_t** payload_pp, u32_t* length_p );

#endif /* DATASET_CONFIG_H */

/****************************** END OF FILE *******************************************************/
