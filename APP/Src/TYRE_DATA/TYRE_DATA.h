/*! \file
*               Author: mstewart
*   \brief      TYRE_DATA - recommended tyre pressures by make / model / year range, read from DATASET0
*
*   \details    The table lives in DATASET0's payload (see APP/linker_script/STM32F103C8_flash.ld), not
*               in APP, so it can be changed by downloading a new dataset through FBL - no APP
*               rebuild. It's generated from Tool_cfg/TYRE_DATA/tyre_pressures.csv by
*               scripts/tyre_dataset_gen (APP/Makefile's `dataset` target), which must build exactly
*               the layout below.
*
*               PAYLOAD LAYOUT (format 2 - packed, no padding, multi-byte fields little-endian)
*               =========================
*                 TYRE_DATA_header_st                    8 B
*                 u8   model_counts[make_count]          models per make
*                 u8   variant_counts[model_count]       year ranges per model
*                 char names[]                           every make name, then every model name, each
*                                                        NUL-terminated, in table order
*                 TYRE_DATA_variant_st [variant_count]   4 B each
*
*               Makes are sorted by name, models by name within their make, year ranges by year
*               within their model, so each menu level is a contiguous run. Nothing stores where a
*               run starts - it's the sum of the counts before it, and a name is found by counting
*               NULs - which costs a short scan per lookup and saves every per-record index and all
*               name padding. Totals are 16-bit, so the payload size is the only real limit; a
*               single make still holds at most 255 models and a model 255 year ranges.
*
*               Nothing here is trusted until TYRE_DATA_init() has checked it: BM's valid bit for
*               DATASET0 (CRC + signature), then the magic, the format version, that the counts add
*               up, that every name is terminated within TYRE_DATA_NAME_MAX_LEN, and that the whole
*               table fits in the payload. Any failure and the table reads as empty.
*/
#ifndef TYRE_DATA_H
#define TYRE_DATA_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define TYRE_DATA_MAGIC_0            ( 'T' )
#define TYRE_DATA_MAGIC_1            ( 'P' )
#define TYRE_DATA_FORMAT_VERSION     ( 2u )     /*!< Bump on any layout change - generator too */

#define TYRE_DATA_NAME_MAX_LEN       ( 20u )    /*!< Longest make/model name, excluding the NUL -
                                                     one short of a 21-character display row */
#define TYRE_DATA_NAME_BUF_LEN       ( TYRE_DATA_NAME_MAX_LEN + 1u )

#define TYRE_DATA_YEAR_BASE          ( 1900u )  /*!< year = TYRE_DATA_YEAR_BASE + year_from/year_to */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef struct
{
    u8_t magic[2];           /*!< TYRE_DATA_MAGIC_0, TYRE_DATA_MAGIC_1 */
    u8_t format_version;     /*!< TYRE_DATA_FORMAT_VERSION */
    u8_t make_count;
    u8_t model_count[2];     /*!< Total over all makes, little-endian */
    u8_t variant_count[2];   /*!< Total over all models, little-endian */
} TYRE_DATA_header_st;

/*! One year range of one model. Pressures are in 0.1 bar (23 = 2.3 bar) - a byte covers 0-25.5
    bar at the resolution placards quote, the smallest field that holds every real car value. */
typedef struct
{
    u8_t year_from;          /*!< Offset from TYRE_DATA_YEAR_BASE */
    u8_t year_to;            /*!< Offset from TYRE_DATA_YEAR_BASE, inclusive */
    u8_t front_dbar;
    u8_t rear_dbar;
} TYRE_DATA_variant_st;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/*!< Checks DATASET0 once - call after SHARED_RAM_init(), before anything reads the table */
void                        TYRE_DATA_init( void );
false_true_et               TYRE_DATA_is_valid( void );

/*!< Counts are 0, name getters return FALSE and TYRE_DATA_get_variant() NULL_P for an invalid table
 *   or an out-of-range index. Model and variant indices are relative to their parent (0 = that
 *   make's first model). */
u8_t                        TYRE_DATA_get_make_count( void );
u8_t                        TYRE_DATA_get_model_count( u8_t make_idx );
u8_t                        TYRE_DATA_get_variant_count( u8_t make_idx, u8_t model_idx );
const TYRE_DATA_variant_st* TYRE_DATA_get_variant( u8_t make_idx, u8_t model_idx, u8_t variant_idx );

/*!< Copy a name into dest_p (dest_len bytes, always NUL-terminated, truncated if short) */
false_true_et               TYRE_DATA_get_make_name( u8_t make_idx, char* dest_p, u8_t dest_len );
false_true_et               TYRE_DATA_get_model_name( u8_t make_idx, u8_t model_idx, char* dest_p, u8_t dest_len );

#endif /* TYRE_DATA_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
