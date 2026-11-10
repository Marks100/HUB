/*! \file
*               Author: mstewart
*   \brief      TYRE_DATA module - Unit Test Suite
*
*   \details    The first APP unit test: TYRE_DATA.c is compiled by Ceedling itself (unit_project.yml), so it is
*               instrumented for coverage. The test builds a DATASET0 payload in RAM - 2 makes, 3 models and
*               4 year ranges, in the layout TYRE_DATA.h describes - and unit_project.yml points the two linker
*               symbols TYRE_DATA.c reads at it. Each validation check in TYRE_DATA_init() is then driven by
*               corrupting one thing in that payload, and the getters are checked against what was put in.
*               BOOT_HANDOFF's valid mask is a stub the test sets.
*/

#include "unity.h"

#include <string.h>
#include "STDC.h"
#include "printf.h"       /* Required by STDC.c (PRINTF_sprintf) - ensures it gets compiled */
#include "SELMATH.h"      /* Required by STDC.c (SELMATH_abs_val) - ensures it gets compiled */

/* Module being tested - Ceedling builds TYRE_DATA.c from this header, so it is instrumented for coverage */
#include "TYRE_DATA.h"

/***************************************************************************************************
**                                        Defines                                                 **
***************************************************************************************************/
#define TEST_PAYLOAD_MAX       ( 256u )
#define TEST_NAME_COUNT        ( 5u )       /* 2 make names, then 3 model names */
#define TEST_VARIANT_COUNT     ( 4u )
#define TEST_HEADER_LEN        ( 8u )

/* Offsets in the header - TYRE_DATA_header_st */
#define TEST_HDR_MAGIC_0       ( 0u )
#define TEST_HDR_MAGIC_1       ( 1u )
#define TEST_HDR_VERSION       ( 2u )
#define TEST_HDR_MODEL_TOTAL   ( 4u )
#define TEST_HDR_VARIANT_TOTAL ( 6u )

/***************************************************************************************************
**                                        Static variables                                        **
***************************************************************************************************/
/* The symbols unit_project.yml gives __dataset0_code_start__ / __dataset0_code_end__ in TYRE_DATA.c */
const u8_t* test_dataset0_start_p = NULL_P;
const u8_t* test_dataset0_end_p   = NULL_P;

/* TYRE_DATA.c's private state, visible because TEST makes STATIC empty */
extern false_true_et tyre_data_valid_s;

static u8_t  test_payload_s[TEST_PAYLOAD_MAX];
static u16_t test_payload_len_s;
static u16_t test_names_end_s;        /* Offset just past the last name's NUL */
static u16_t test_variants_off_s;     /* Offset of the first year range */
static u8_t  test_dataset_mask_s;     /* What the BOOT_HANDOFF stub returns */

/* Makes, then models, in table order: Audi has A3 and A4, BMW has M3 */
static const char* test_names_s[TEST_NAME_COUNT];
static const char* const test_default_names_s[TEST_NAME_COUNT] = { "Audi", "BMW", "A3", "A4", "M3" };

static const u8_t test_model_counts_s[2]   = { 2u, 1u };
static const u8_t test_variant_counts_s[3] = { 2u, 1u, 1u };

/* year_from, year_to, front_dbar, rear_dbar - in the order A3 (2), A4 (1), M3 (1) */
static const TYRE_DATA_variant_st test_variants_s[TEST_VARIANT_COUNT] =
{
    {  99u, 104u, 22u, 24u },
    { 105u, 110u, 23u, 25u },
    {  95u,  99u, 28u, 30u },
    { 100u, 120u, 26u, 27u }
};

/***************************************************************************************************
**                                        Stubs                                                   **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         BOOT_HANDOFF stub - the valid mask the test has set
*
*   \author        MS
*
*   \return        test_dataset_mask_s
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_dataset_valid_mask( void )
{
    return( test_dataset_mask_s );
}

/***************************************************************************************************
**                                        Test helpers                                            **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Lay out test_payload_s from test_names_s, the count tables and test_variants_s
*
*   \author        MS
*
*   \return        none
*
*   \note          Sets test_payload_len_s to the whole payload, and test_names_end_s and test_variants_off_s
*                  to where those sections end and start, for the tests that cut or corrupt it
*
***************************************************************************************************/
static void test_build_payload( void )
{
    u16_t pos = 0u;
    u8_t  i;

    memset( test_payload_s, 0, sizeof( test_payload_s ) );

    test_payload_s[TEST_HDR_MAGIC_0]       = (u8_t)TYRE_DATA_MAGIC_0;
    test_payload_s[TEST_HDR_MAGIC_1]       = (u8_t)TYRE_DATA_MAGIC_1;
    test_payload_s[TEST_HDR_VERSION]       = TYRE_DATA_FORMAT_VERSION;
    test_payload_s[3]                      = 2u;                      /* make_count */
    test_payload_s[TEST_HDR_MODEL_TOTAL]   = 3u;
    test_payload_s[TEST_HDR_VARIANT_TOTAL] = TEST_VARIANT_COUNT;
    pos = TEST_HEADER_LEN;

    memcpy( &test_payload_s[pos], test_model_counts_s, sizeof( test_model_counts_s ) );
    pos = (u16_t)( pos + sizeof( test_model_counts_s ) );

    memcpy( &test_payload_s[pos], test_variant_counts_s, sizeof( test_variant_counts_s ) );
    pos = (u16_t)( pos + sizeof( test_variant_counts_s ) );

    for( i = 0u; i < TEST_NAME_COUNT; i++ )
    {
        strcpy( (char*)&test_payload_s[pos], test_names_s[i] );
        pos = (u16_t)( pos + strlen( test_names_s[i] ) + 1u );
    }

    test_names_end_s    = pos;
    test_variants_off_s = pos;

    memcpy( &test_payload_s[pos], test_variants_s, sizeof( test_variants_s ) );
    pos = (u16_t)( pos + sizeof( test_variants_s ) );

    test_payload_len_s = pos;
}

/*!
****************************************************************************************************
*
*   \brief         Point the linker symbols at the first test_payload_len_s bytes of the payload and run init
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
static void test_init( void )
{
    test_dataset0_start_p = &test_payload_s[0];
    test_dataset0_end_p   = &test_payload_s[test_payload_len_s - 1u];

    TYRE_DATA_init();
}

/*!
****************************************************************************************************
*
*   \brief         Build the standard payload and init with it
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
static void test_init_valid( void )
{
    test_build_payload();
    test_init();
}

/***************************************************************************************************
**                                             setUp                                              **
***************************************************************************************************/
void setUp( void )
{
    u8_t i;

    for( i = 0u; i < TEST_NAME_COUNT; i++ )
    {
        test_names_s[i] = test_default_names_s[i];
    }

    test_dataset_mask_s = 0x01u;
    tyre_data_valid_s   = FALSE;
    test_build_payload();
}

/***************************************************************************************************
**                                            tearDown                                            **
***************************************************************************************************/
void tearDown( void )
{
}

/***************************************************************************************************
**                                     Init and a valid table                                     **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify a table that was never initialised reads as empty
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_uninitialised_table_should_read_empty( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_make_count() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_model_count( 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_variant_count( 0u, 0u ) );
    TEST_ASSERT_NULL( TYRE_DATA_get_variant( 0u, 0u, 0u ) );
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_get_make_name( 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_get_model_name( 0u, 0u, name, sizeof( name ) ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a well-formed payload with BOOT_HANDOFF's valid bit set is accepted
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_accept_a_well_formed_payload( void )
{
    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the counts come back as laid out in the payload
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_counts_should_match_the_payload( void )
{
    test_init_valid();

    TEST_ASSERT_EQUAL_UINT8( 2u, TYRE_DATA_get_make_count() );
    TEST_ASSERT_EQUAL_UINT8( 2u, TYRE_DATA_get_model_count( 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, TYRE_DATA_get_model_count( 1u ) );
    TEST_ASSERT_EQUAL_UINT8( 2u, TYRE_DATA_get_variant_count( 0u, 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, TYRE_DATA_get_variant_count( 0u, 1u ) );
    TEST_ASSERT_EQUAL_UINT8( 1u, TYRE_DATA_get_variant_count( 1u, 0u ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify make names are found in table order
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_make_names_should_match_the_payload( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "Audi", name );
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 1u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "BMW", name );
}

/*!
****************************************************************************************************
*
*   \brief         Verify model names are found relative to their make, after all the make names
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_model_names_should_match_the_payload( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_model_name( 0u, 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "A3", name );
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_model_name( 0u, 1u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "A4", name );
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_model_name( 1u, 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "M3", name );
}

/*!
****************************************************************************************************
*
*   \brief         Verify each year range is found by summing the ranges of the models before it
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_variants_should_match_the_payload( void )
{
    const TYRE_DATA_variant_st* variant_p;

    test_init_valid();

    variant_p = TYRE_DATA_get_variant( 0u, 0u, 1u );
    TEST_ASSERT_EQUAL_PTR( &test_payload_s[test_variants_off_s + ( 1u * sizeof( TYRE_DATA_variant_st ) )], variant_p );
    TEST_ASSERT_EQUAL_UINT8( 105u, variant_p->year_from );
    TEST_ASSERT_EQUAL_UINT8( 110u, variant_p->year_to );
    TEST_ASSERT_EQUAL_UINT8( 23u, variant_p->front_dbar );
    TEST_ASSERT_EQUAL_UINT8( 25u, variant_p->rear_dbar );

    variant_p = TYRE_DATA_get_variant( 0u, 1u, 0u );
    TEST_ASSERT_EQUAL_PTR( &test_payload_s[test_variants_off_s + ( 2u * sizeof( TYRE_DATA_variant_st ) )], variant_p );
    TEST_ASSERT_EQUAL_UINT8( 95u, variant_p->year_from );

    variant_p = TYRE_DATA_get_variant( 1u, 0u, 0u );
    TEST_ASSERT_EQUAL_PTR( &test_payload_s[test_variants_off_s + ( 3u * sizeof( TYRE_DATA_variant_st ) )], variant_p );
    TEST_ASSERT_EQUAL_UINT8( 100u, variant_p->year_from );
    TEST_ASSERT_EQUAL_UINT8( 120u, variant_p->year_to );
    TEST_ASSERT_EQUAL_UINT8( 26u, variant_p->front_dbar );
    TEST_ASSERT_EQUAL_UINT8( 27u, variant_p->rear_dbar );
}

/***************************************************************************************************
**                                         Name copying                                           **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify a name longer than the buffer is truncated and still NUL-terminated
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_name_should_truncate_to_the_buffer_length( void )
{
    char name[4];

    test_init_valid();
    memset( name, 'x', sizeof( name ) );

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 0u, name, 3u ) );
    TEST_ASSERT_EQUAL_STRING( "Au", name );
    TEST_ASSERT_EQUAL_UINT8( 'x', (u8_t)name[3] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a one-byte buffer receives just the terminator
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_name_with_one_byte_buffer_should_be_empty( void )
{
    char name[2] = { 'x', 'x' };

    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 0u, name, 1u ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, (u8_t)name[0] );
    TEST_ASSERT_EQUAL_UINT8( 'x', (u8_t)name[1] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a zero-length buffer is left untouched
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_name_with_zero_length_buffer_should_not_write( void )
{
    char name[2] = { 'x', 'x' };

    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_make_name( 0u, name, 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 'x', (u8_t)name[0] );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a name of the longest allowed length is accepted and copied whole
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_name_of_max_length_should_be_accepted( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    test_names_s[2] = "ABCDEFGHIJKLMNOPQRST";    /* TYRE_DATA_NAME_MAX_LEN characters */
    test_init_valid();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_get_model_name( 0u, 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL_STRING( "ABCDEFGHIJKLMNOPQRST", name );
}

/***************************************************************************************************
**                                  Out-of-range indices                                          **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify an index past the end of any level reads as nothing
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_out_of_range_index_should_read_empty( void )
{
    char name[TYRE_DATA_NAME_BUF_LEN];

    test_init_valid();

    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_model_count( 2u ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_variant_count( 2u, 0u ) );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_variant_count( 1u, 1u ) );
    TEST_ASSERT_NULL( TYRE_DATA_get_variant( 2u, 0u, 0u ) );
    TEST_ASSERT_NULL( TYRE_DATA_get_variant( 1u, 1u, 0u ) );
    TEST_ASSERT_NULL( TYRE_DATA_get_variant( 1u, 0u, 1u ) );
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_get_make_name( 2u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_get_model_name( 2u, 0u, name, sizeof( name ) ) );
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_get_model_name( 1u, 1u, name, sizeof( name ) ) );
}

/***************************************************************************************************
**                                  Rejected payloads - header                                    **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify nothing is read, and the table is empty, when BOOT_HANDOFF's DATASET0 bit is clear
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_when_dataset_bit_is_clear( void )
{
    test_dataset_mask_s = 0x00u;
    test_init_valid();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_make_count() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify only bit 0 of the mask decides: other bits alone reject, and extra bits do not
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_only_look_at_bit_0_of_the_mask( void )
{
    test_dataset_mask_s = 0xFEu;
    test_init_valid();
    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );

    test_dataset_mask_s = 0xFFu;
    test_init();
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a wrong first magic byte is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_bad_first_magic_byte( void )
{
    test_payload_s[TEST_HDR_MAGIC_0] = 'X';
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a wrong second magic byte is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_bad_second_magic_byte( void )
{
    test_payload_s[TEST_HDR_MAGIC_1] = 'X';
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a different format version is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_a_different_format_version( void )
{
    test_payload_s[TEST_HDR_VERSION] = (u8_t)( TYRE_DATA_FORMAT_VERSION + 1u );
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a payload too short to hold the header's count tables is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_a_payload_shorter_than_the_count_tables( void )
{
    test_payload_len_s = TEST_HEADER_LEN + 2u + 3u - 1u;    /* header + model_counts + variant_counts, less one */
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/***************************************************************************************************
**                                  Rejected payloads - counts                                    **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify per-make model counts that do not add up to the header total are rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_model_counts_that_do_not_add_up( void )
{
    test_payload_s[TEST_HEADER_LEN] = 3u;     /* Audi now claims 3 models: 3 + 1 != the header's 3 */
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify per-model year-range counts that do not add up to the header total are rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_variant_counts_that_do_not_add_up( void )
{
    test_payload_s[TEST_HEADER_LEN + 2u] = 3u;    /* first model now claims 3 ranges: 3 + 1 + 1 != the header's 4 */
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/***************************************************************************************************
**                                  Rejected payloads - names                                     **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify an empty name is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_an_empty_name( void )
{
    test_names_s[1] = "";
    test_init_valid();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a name one character over the limit is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_a_name_over_the_length_limit( void )
{
    test_names_s[3] = "ABCDEFGHIJKLMNOPQRSTU";    /* TYRE_DATA_NAME_MAX_LEN + 1 characters */
    test_init_valid();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a last name whose terminator is past the end of the payload is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_an_unterminated_last_name( void )
{
    test_payload_len_s = (u16_t)( test_names_end_s - 1u );    /* the final NUL is cut off */
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/***************************************************************************************************
**                                  Rejected payloads - size                                      **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify a payload one byte short of holding every year range is rejected
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_reject_a_payload_one_byte_short_of_the_variants( void )
{
    test_payload_len_s = (u16_t)( test_payload_len_s - 1u );
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a payload that ends exactly after the last year range is accepted
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_should_accept_a_payload_that_ends_after_the_last_variant( void )
{
    test_init();

    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT16( test_payload_len_s, (u16_t)( test_variants_off_s + sizeof( test_variants_s ) ) );
}

/***************************************************************************************************
**                                         Re-initialising                                        **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify a later init on a bad payload makes a table that was valid read empty again
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_TYRE_DATA_init_on_a_bad_payload_should_clear_a_valid_table( void )
{
    test_init_valid();
    TEST_ASSERT_EQUAL( TRUE, TYRE_DATA_is_valid() );

    test_payload_s[TEST_HDR_MAGIC_0] = 'X';
    test_init();

    TEST_ASSERT_EQUAL( FALSE, TYRE_DATA_is_valid() );
    TEST_ASSERT_EQUAL_UINT8( 0u, TYRE_DATA_get_make_count() );
    TEST_ASSERT_NULL( TYRE_DATA_get_variant( 0u, 0u, 0u ) );
}

/****************************** END OF FILE *******************************************************/
