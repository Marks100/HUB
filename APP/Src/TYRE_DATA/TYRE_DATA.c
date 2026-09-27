/*! \file
*               Author: mstewart
*   \brief      TYRE_DATA - recommended tyre pressures read from DATASET0 - see TYRE_DATA.h
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "TYRE_DATA.h"
#include "SHARED_RAM.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define TYRE_DATA_DATASET_VALID_BIT  ( 0x01u )   /*!< DATASET0 = bit 0 of the BM valid mask */

/* Both records are bytes only, so the compiler has nothing to pad - these only fail if a field is
   changed to a wider type, which would also break the generator's layout. */
typedef char tyre_data_header_size_check[  ( sizeof( TYRE_DATA_header_st )  == 8u ) ? 1 : -1 ];
typedef char tyre_data_variant_size_check[ ( sizeof( TYRE_DATA_variant_st ) == 4u ) ? 1 : -1 ];

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
/* DATASET0's payload - APP/linker_script/STM32F103C8_flash.ld, end inclusive */
extern const u8_t __dataset0_code_start__;
extern const u8_t __dataset0_code_end__;

STATIC false_true_et               tyre_data_valid_s          = FALSE;
STATIC u8_t                        tyre_data_make_count_s     = 0u;
STATIC u16_t                       tyre_data_model_total_s    = 0u;
STATIC u16_t                       tyre_data_variant_total_s  = 0u;
STATIC const u8_t*                 tyre_data_model_counts_p   = NULL_P;   /* [make_count]  */
STATIC const u8_t*                 tyre_data_variant_counts_p = NULL_P;   /* [model_total] */
STATIC const char*                 tyre_data_names_p          = NULL_P;
STATIC const TYRE_DATA_variant_st* tyre_data_variants_p       = NULL_P;

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u16_t         tyre_data_read_u16( const u8_t* bytes_p );
STATIC false_true_et tyre_data_counts_add_up( void );
STATIC u32_t         tyre_data_skip_names( const u8_t* payload_p, u32_t offset, u32_t payload_len, u16_t name_count );
STATIC false_true_et tyre_data_model_index( u8_t make_idx, u8_t model_idx, u16_t* index_p );
STATIC false_true_et tyre_data_copy_name( u16_t name_idx, char* dest_p, u8_t dest_len );

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Check DATASET0 and point at its sections if it holds a usable tyre table
*
*   \author        MS
*
*   \return        none
*
*   \note          BM's CRC/signature result comes first - nothing in the payload is read unless it
*                  passed. Then each section's size is checked against the payload before the next
*                  one is located, the counts are checked to add up to the header's totals, and the
*                  name pool is walked once so every later lookup can count NULs without a bounds
*                  check of its own.
*
***************************************************************************************************/
void TYRE_DATA_init( void )
{
    const u8_t*                payload_p   = &__dataset0_code_start__;
    u32_t                      payload_len = (u32_t)( &__dataset0_code_end__ - &__dataset0_code_start__ ) + 1u;
    const TYRE_DATA_header_st* header_p    = (const TYRE_DATA_header_st*)payload_p;
    u32_t                      offset;

    tyre_data_valid_s = FALSE;

    if( ( ( SHARED_RAM_get_dataset_valid_mask() & TYRE_DATA_DATASET_VALID_BIT ) != 0u ) &&
        ( header_p->magic[0] == (u8_t)TYRE_DATA_MAGIC_0 ) &&
        ( header_p->magic[1] == (u8_t)TYRE_DATA_MAGIC_1 ) &&
        ( header_p->format_version == TYRE_DATA_FORMAT_VERSION ) )
    {
        tyre_data_make_count_s    = header_p->make_count;
        tyre_data_model_total_s   = tyre_data_read_u16( header_p->model_count );
        tyre_data_variant_total_s = tyre_data_read_u16( header_p->variant_count );

        offset = (u32_t)sizeof( TYRE_DATA_header_st ) + tyre_data_make_count_s + tyre_data_model_total_s;

        if( offset <= payload_len )
        {
            tyre_data_model_counts_p   = &payload_p[sizeof( TYRE_DATA_header_st )];
            tyre_data_variant_counts_p = &tyre_data_model_counts_p[tyre_data_make_count_s];
            tyre_data_names_p          = (const char*)&payload_p[offset];

            offset = tyre_data_skip_names( payload_p, offset, payload_len,
                                           (u16_t)( tyre_data_make_count_s + tyre_data_model_total_s ) );

            if( ( offset != 0u ) &&
                ( ( offset + ( (u32_t)tyre_data_variant_total_s * (u32_t)sizeof( TYRE_DATA_variant_st ) ) ) <= payload_len ) )
            {
                tyre_data_variants_p = (const TYRE_DATA_variant_st*)&payload_p[offset];
                tyre_data_valid_s    = tyre_data_counts_add_up();
            }
        }
    }
}

false_true_et TYRE_DATA_is_valid( void )
{
    return( tyre_data_valid_s );
}

u8_t TYRE_DATA_get_make_count( void )
{
    return( ( tyre_data_valid_s == TRUE ) ? tyre_data_make_count_s : 0u );
}

u8_t TYRE_DATA_get_model_count( u8_t make_idx )
{
    return( ( ( tyre_data_valid_s == TRUE ) && ( make_idx < tyre_data_make_count_s ) ) ?
            tyre_data_model_counts_p[make_idx] : 0u );
}

u8_t TYRE_DATA_get_variant_count( u8_t make_idx, u8_t model_idx )
{
    u16_t model;

    return( ( tyre_data_model_index( make_idx, model_idx, &model ) == TRUE ) ?
            tyre_data_variant_counts_p[model] : 0u );
}

/*!
****************************************************************************************************
*
*   \brief         One year range of one model
*
*   \author        MS
*
*   \return        The record, or NULL_P for an invalid table or an out-of-range index
*
*   \note          The record's position is the sum of the year-range counts of every model before
*                  this one - a short loop rather than a stored index, see TYRE_DATA.h.
*
***************************************************************************************************/
const TYRE_DATA_variant_st* TYRE_DATA_get_variant( u8_t make_idx, u8_t model_idx, u8_t variant_idx )
{
    const TYRE_DATA_variant_st* variant_p = NULL_P;
    u16_t                       model;
    u16_t                       first     = 0u;
    u16_t                       i;

    if( ( tyre_data_model_index( make_idx, model_idx, &model ) == TRUE ) &&
        ( variant_idx < tyre_data_variant_counts_p[model] ) )
    {
        for( i = 0u; i < model; i++ )
        {
            first = (u16_t)( first + tyre_data_variant_counts_p[i] );
        }

        variant_p = &tyre_data_variants_p[first + variant_idx];
    }

    return( variant_p );
}

false_true_et TYRE_DATA_get_make_name( u8_t make_idx, char* dest_p, u8_t dest_len )
{
    false_true_et found = FALSE;

    if( ( tyre_data_valid_s == TRUE ) && ( make_idx < tyre_data_make_count_s ) )
    {
        found = tyre_data_copy_name( make_idx, dest_p, dest_len );
    }

    return( found );
}

false_true_et TYRE_DATA_get_model_name( u8_t make_idx, u8_t model_idx, char* dest_p, u8_t dest_len )
{
    false_true_et found = FALSE;
    u16_t         model;

    /* Model names follow every make name in the pool */
    if( tyre_data_model_index( make_idx, model_idx, &model ) == TRUE )
    {
        found = tyre_data_copy_name( (u16_t)( tyre_data_make_count_s + model ), dest_p, dest_len );
    }

    return( found );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

STATIC u16_t tyre_data_read_u16( const u8_t* bytes_p )
{
    return( (u16_t)( (u16_t)bytes_p[0] | (u16_t)( (u16_t)bytes_p[1] << 8u ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Check the per-make and per-model counts sum to the header's totals
*
*   \author        MS
*
*   \return        TRUE if both sums match - which is what keeps every run the getters compute inside
*                  its table
*
***************************************************************************************************/
STATIC false_true_et tyre_data_counts_add_up( void )
{
    u32_t models   = 0u;
    u32_t variants = 0u;
    u16_t i;

    for( i = 0u; i < tyre_data_make_count_s; i++ )
    {
        models += tyre_data_model_counts_p[i];
    }

    for( i = 0u; i < tyre_data_model_total_s; i++ )
    {
        variants += tyre_data_variant_counts_p[i];
    }

    return( ( ( models == tyre_data_model_total_s ) && ( variants == tyre_data_variant_total_s ) ) ? TRUE : FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Walk the name pool, checking every name as it goes
*
*   \author        MS
*
*   \param         payload_p   - Start of the payload
*   \param         offset      - Where the first name starts
*   \param         payload_len - Payload size in bytes
*   \param         name_count  - Names to walk
*
*   \return        Offset just past the last name's NUL, or 0 if a name is empty, longer than
*                  TYRE_DATA_NAME_MAX_LEN, or runs off the end of the payload
*
***************************************************************************************************/
STATIC u32_t tyre_data_skip_names( const u8_t* payload_p, u32_t offset, u32_t payload_len, u16_t name_count )
{
    u16_t name;
    u32_t len;

    for( name = 0u; ( offset != 0u ) && ( name < name_count ); name++ )
    {
        len = 0u;

        while( ( ( offset + len ) < payload_len ) && ( payload_p[offset + len] != 0u ) &&
               ( len <= TYRE_DATA_NAME_MAX_LEN ) )
        {
            len++;
        }

        if( ( ( offset + len ) < payload_len ) && ( payload_p[offset + len] == 0u ) &&
            ( len != 0u ) && ( len <= TYRE_DATA_NAME_MAX_LEN ) )
        {
            offset += len + 1u;
        }
        else
        {
            offset = 0u;
        }
    }

    return( offset );
}

/*!
****************************************************************************************************
*
*   \brief         A make-relative model index as a position in the whole model table
*
*   \author        MS
*
*   \return        TRUE with *index_p set, or FALSE for an invalid table or an out-of-range index
*
***************************************************************************************************/
STATIC false_true_et tyre_data_model_index( u8_t make_idx, u8_t model_idx, u16_t* index_p )
{
    false_true_et found = FALSE;
    u16_t         first = 0u;
    u8_t          i;

    if( ( tyre_data_valid_s == TRUE ) && ( make_idx < tyre_data_make_count_s ) &&
        ( model_idx < tyre_data_model_counts_p[make_idx] ) )
    {
        for( i = 0u; i < make_idx; i++ )
        {
            first = (u16_t)( first + tyre_data_model_counts_p[i] );
        }

        *index_p = (u16_t)( first + model_idx );
        found    = TRUE;
    }

    return( found );
}

/*!
****************************************************************************************************
*
*   \brief         Copy the name_idx'th name in the pool
*
*   \author        MS
*
*   \return        TRUE - callers have already range-checked name_idx, and TYRE_DATA_init() checked
*                  every name is terminated, so the NUL count below can't run off the pool
*
***************************************************************************************************/
STATIC false_true_et tyre_data_copy_name( u16_t name_idx, char* dest_p, u8_t dest_len )
{
    const char* name_p = tyre_data_names_p;
    u16_t       skipped;
    u8_t        i;

    for( skipped = 0u; skipped < name_idx; skipped++ )
    {
        while( *name_p != '\0' )
        {
            name_p++;
        }

        name_p++;
    }

    for( i = 0u; ( name_p[i] != '\0' ) && ( (u16_t)i + 1u < dest_len ); i++ )
    {
        dest_p[i] = name_p[i];
    }

    if( dest_len != 0u )
    {
        dest_p[i] = '\0';
    }

    return( TRUE );
}

/****************************** END OF FILE *******************************************************/
