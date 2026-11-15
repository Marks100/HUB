/*! \file
*               Author: mstewart
*   \brief      DATASET_config - the downloadable datasets: where each is in the flash, and which it is
*
*   BM, the FBL and the application all build dataset_table_g from this file. APP/Makefile builds each dataset with its own
*   script, given the same start, end and id written out there: the two must agree.
*
*   TO ADD A DATASET
*       1. Here:         copy the last DATASETn block below, with the next n and its own ID and SIZE
*       2. Here:         add its name to DATASET_index_et, before DATASET_COUNT
*       3. DATASET_config.c: add its row to dataset_table_g
*       4. APP/Makefile: add its block (script, source, start, end, id), its target under `dataset`, and its hex to variant_image
*
*   TO RESIZE A DATASET
*       Change its SIZE here, and its START and END in the Makefile (and those of every dataset after it, which move, so download
*       them again). A size is a multiple of 0x400 (the flash page), bigger than DATASET_HEADER_SIZE. Nothing checks the layout
*       in the build; the software test test_TYRE_PRESSURES does, and it does not look at the Makefile.
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
#define DATASET_REGION_START   0x0801E800ul    /* where DATASET0 starts; the datasets end before the NVM (0x0801F800) */
#define DATASET_HEADER_SIZE    0x60ul          /* the header at the start of every dataset: its payload starts this far in */
#define DATASET_MAX_COUNT      8u              /* BM's valid mask is one byte, one bit per dataset */

/* DATASET0: the tyre pressure table (APP's TYRE_DATA) */
#define DATASET0_ID      0x54595245ul          /* 'TYRE' */
#define DATASET0_SIZE    0x1000ul
#define DATASET0_START   ( DATASET_REGION_START )

/* DATASET1: copy this block when there is a second dataset
    #define DATASET1_ID      0x00000000ul
    #define DATASET1_SIZE    0x0400ul
    #define DATASET1_START   ( DATASET0_START + DATASET0_SIZE )
*/

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
/*! \brief The position of each dataset in dataset_table_g (DATASET0 is the first), and the table's size */
typedef enum
{
    DATASET_TYRE_PRESSURES = 0u,    /*!< DATASET0 */
    DATASET_COUNT
} DATASET_index_et;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* One row per DATASET_index_et: header, payload start and payload end (inclusive) in the flash, and the id its header must carry */
extern const HEADER_region_st dataset_table_g[DATASET_COUNT];

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/*!< TRUE if the dataset is in the table and BM found it valid (presence, id, CRC and signature): *payload_pp is its payload, in the flash, and
 *   *length_p its size in bytes. FALSE, with NULL_P and 0 in them, if not, so a caller can hand them straight on. Either pointer NULL_P is FALSE */
false_true_et DATASET_get_payload( DATASET_index_et index, const u8_t** payload_pp, u32_t* length_p );

#endif /* DATASET_CONFIG_H */

/****************************** END OF FILE *******************************************************/
