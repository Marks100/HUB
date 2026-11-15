/*! \file
*               Author: mstewart
*   \brief      DATASET module - Unit Test Suite
*
*   \details    DATASET_get_payload() joins two things: the table of where each dataset is in the flash (dataset_table_g) and BM's verdict
*               on it (bit n of the handoff RAM's valid mask). The table is DATASET_config.c's, which the test compiles as the firmware does:
*               it needs no stub, as the datasets are in no linker script. The test stubs BOOT_HANDOFF's mask, so it checks what is given back for each mask and each index and that nothing is handed out for a
*               dataset BM did not accept.
*/

#include "unity.h"

#include "STDC.h"
#include "printf.h"       /* Required by STDC.c (PRINTF_sprintf) - ensures it gets compiled */
#include "SELMATH.h"      /* Required by STDC.c (SELMATH_abs_val) - ensures it gets compiled */

/* Module being tested - Ceedling builds DATASET_config.c from this header */
#include "DATASET_config.h"

/***************************************************************************************************
**                                        Defines                                                 **
***************************************************************************************************/
/* What the module is given for DATASET0: its payload, after the header to the end of its slot (DATASET_config.h) */
#define TEST_PAYLOAD_ADDRESS   ( DATASET0_START + DATASET_HEADER_SIZE )
#define TEST_PAYLOAD_LEN       ( DATASET0_SIZE - DATASET_HEADER_SIZE )
#define TEST_OTHER_ADDRESS     ( 0x00020000ul )    /* not the payload: what an output holds before a call that must not touch it */

/***************************************************************************************************
**                                        Static variables                                        **
***************************************************************************************************/
static u8_t test_dataset_mask_s;     /* What the BOOT_HANDOFF stub returns */

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
**                                             setUp                                              **
***************************************************************************************************/
void setUp( void )
{
    test_dataset_mask_s = 0x01u;
}

void tearDown( void )
{
}

/***************************************************************************************************
**                                             Tests                                              **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Verify a dataset BM accepted gives its payload in place, and its size: the end address is the last byte, so the size
*                  is one past the difference
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_DATASET_get_payload_should_give_the_payload_of_an_accepted_dataset( void )
{
    const u8_t* payload_p = NULL_P;
    u32_t       length    = 0u;

    TEST_ASSERT_EQUAL( TRUE, DATASET_get_payload( DATASET_TYRE_PRESSURES, &payload_p, &length ) );

    TEST_ASSERT_EQUAL_PTR( (const u8_t*)TEST_PAYLOAD_ADDRESS, payload_p );
    TEST_ASSERT_EQUAL_UINT32( TEST_PAYLOAD_LEN, length );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a dataset BM did not accept gives nothing: FALSE, no payload and no length, so a caller can hand them straight on
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_DATASET_get_payload_should_give_nothing_when_the_bit_is_clear( void )
{
    const u8_t* payload_p = (const u8_t*)TEST_OTHER_ADDRESS;    /* not what a refusal leaves behind */
    u32_t       length    = 99u;

    test_dataset_mask_s = 0x00u;

    TEST_ASSERT_EQUAL( FALSE, DATASET_get_payload( DATASET_TYRE_PRESSURES, &payload_p, &length ) );

    TEST_ASSERT_NULL( payload_p );
    TEST_ASSERT_EQUAL_UINT32( 0u, length );
}

/*!
****************************************************************************************************
*
*   \brief         Verify only the dataset's own bit decides: the others alone refuse it, and extra bits do not
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_DATASET_get_payload_should_only_look_at_the_datasets_own_bit( void )
{
    const u8_t* payload_p = NULL_P;
    u32_t       length    = 0u;

    test_dataset_mask_s = (u8_t)~( 1u << DATASET_TYRE_PRESSURES );
    TEST_ASSERT_EQUAL( FALSE, DATASET_get_payload( DATASET_TYRE_PRESSURES, &payload_p, &length ) );

    test_dataset_mask_s = 0xFFu;
    TEST_ASSERT_EQUAL( TRUE, DATASET_get_payload( DATASET_TYRE_PRESSURES, &payload_p, &length ) );
    TEST_ASSERT_EQUAL_UINT32( TEST_PAYLOAD_LEN, length );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an index past the table is refused, whatever the mask says
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_DATASET_get_payload_should_refuse_an_index_past_the_table( void )
{
    const u8_t* payload_p = (const u8_t*)TEST_OTHER_ADDRESS;
    u32_t       length    = 99u;

    test_dataset_mask_s = 0xFFu;

    TEST_ASSERT_EQUAL( FALSE, DATASET_get_payload( (DATASET_index_et)DATASET_COUNT, &payload_p, &length ) );

    TEST_ASSERT_NULL( payload_p );
    TEST_ASSERT_EQUAL_UINT32( 0u, length );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a NULL_P output is refused and the other output is left as it was: nothing is written through nothing
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void test_DATASET_get_payload_should_refuse_a_null_output( void )
{
    const u8_t* payload_p = (const u8_t*)TEST_OTHER_ADDRESS;
    u32_t       length    = 99u;

    TEST_ASSERT_EQUAL( FALSE, DATASET_get_payload( DATASET_TYRE_PRESSURES, NULL_P, &length ) );
    TEST_ASSERT_EQUAL_UINT32( 99u, length );

    TEST_ASSERT_EQUAL( FALSE, DATASET_get_payload( DATASET_TYRE_PRESSURES, &payload_p, NULL_P ) );
    TEST_ASSERT_EQUAL_PTR( (const u8_t*)TEST_OTHER_ADDRESS, payload_p );
}
