/*! \file
*               Author: mstewart
*   \brief      BOOT_HANDOFF - see BOOT_HANDOFF.h
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "BOOT_HANDOFF.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
/* Every field starts at zero: no FBL request, no failures, no datasets confirmed */
static const boot_handoff_st boot_handoff_defaults_c =
{
    .fbl_request         = 0u,
    .boot_failure_count  = 0u,
    .last_failure_reason = 0u,
    .dataset_valid_mask  = 0u,
    .fbl_timeout         = 0u
};

static const SHARED_RAM_cfg_st boot_handoff_cfg_c =
{
    .layout     = BOOT_HANDOFF_LAYOUT,
    .size       = (u8_t)sizeof( boot_handoff_st ),
    .defaults_p = &boot_handoff_defaults_c
};

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Set up the shared RAM region for the handoff data
*
*   \author        MS
*
*   \return        none
*
*   \note          Only writes defaults where the region is missing or short - a soft reset (BM
*                  jumping to APP, or vice versa) must not wipe fbl_request/failure state that was
*                  just set for the next stage to read.
*
***************************************************************************************************/
void BOOT_HANDOFF_init( void )
{
    (void)SHARED_RAM_init( &boot_handoff_cfg_c );
}

/*!
****************************************************************************************************
*
*   \brief         Set the FBL entry request flag
*
*   \author        MS
*
*   \param         request - TRUE to request FBL on next boot
*
*   \return        none
*
***************************************************************************************************/
void BOOT_HANDOFF_set_fbl_request( false_true_et request )
{
    u8_t value = ( request == TRUE ) ? 1u : 0u;

    (void)SHARED_RAM_write_field( boot_handoff_st, fbl_request, &value );
}

/*!
****************************************************************************************************
*
*   \brief         Get the FBL entry request flag
*
*   \author        MS
*
*   \return        TRUE if FBL entry was requested. FALSE if not, or if the region is not valid.
*
***************************************************************************************************/
false_true_et BOOT_HANDOFF_get_fbl_request( void )
{
    u8_t value;

    (void)SHARED_RAM_read_field( boot_handoff_st, fbl_request, &value );

    return( ( value != 0u ) ? TRUE : FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         Set the FBL entry timeout - reserved for future use
*
*   \author        MS
*
*   \param         timeout_ms - Timeout value in milliseconds
*
*   \return        none
*
*   \note          Stored but not currently read by anything - no code enforces this timeout yet.
*
***************************************************************************************************/
void BOOT_HANDOFF_set_fbl_timeout( u32_t timeout_ms )
{
    u16_t value = (u16_t)timeout_ms;

    (void)SHARED_RAM_write_field( boot_handoff_st, fbl_timeout, &value );
}

/*!
****************************************************************************************************
*
*   \brief         Get the FBL entry timeout - reserved for future use
*
*   \author        MS
*
*   \return        Stored timeout value in milliseconds
*
***************************************************************************************************/
u32_t BOOT_HANDOFF_get_fbl_timeout( void )
{
    u16_t value;

    (void)SHARED_RAM_read_field( boot_handoff_st, fbl_timeout, &value );

    return( value );
}

/*!
****************************************************************************************************
*
*   \brief         Set the last boot failure reason
*
*   \author        MS
*
*   \param         reason - See bm_failure_reason_et in BM.h
*
*   \return        none
*
***************************************************************************************************/
void BOOT_HANDOFF_set_last_failure_reason( u8_t reason )
{
    (void)SHARED_RAM_write_field( boot_handoff_st, last_failure_reason, &reason );
}

/*!
****************************************************************************************************
*
*   \brief         Get the last boot failure reason
*
*   \author        MS
*
*   \return        See bm_failure_reason_et in BM.h
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_last_failure_reason( void )
{
    u8_t value;

    (void)SHARED_RAM_read_field( boot_handoff_st, last_failure_reason, &value );

    return( value );
}

/*!
****************************************************************************************************
*
*   \brief         Set the count of APP launches not yet confirmed good
*
*   \author        MS
*
*   \param         count - New count. 0 = APP confirmed good (APP) / new image coming (FBL)
*
*   \return        none
*
***************************************************************************************************/
void BOOT_HANDOFF_set_boot_failure_count( u8_t count )
{
    (void)SHARED_RAM_write_field( boot_handoff_st, boot_failure_count, &count );
}

/*!
****************************************************************************************************
*
*   \brief         Get the count of APP launches not yet confirmed good
*
*   \author        MS
*
*   \return        Current count - see boot_handoff_st.boot_failure_count
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_boot_failure_count( void )
{
    u8_t value;

    (void)SHARED_RAM_read_field( boot_handoff_st, boot_failure_count, &value );

    return( value );
}

/*!
****************************************************************************************************
*
*   \brief         Record which datasets BM found valid this boot
*
*   \author        MS
*
*   \param         mask - Bit n set = dataset n valid
*
*   \return        none
*
***************************************************************************************************/
void BOOT_HANDOFF_set_dataset_valid_mask( u8_t mask )
{
    (void)SHARED_RAM_write_field( boot_handoff_st, dataset_valid_mask, &mask );
}

/*!
****************************************************************************************************
*
*   \brief         Which datasets BM found valid this boot
*
*   \author        MS
*
*   \return        Bit n set = dataset n valid
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_dataset_valid_mask( void )
{
    u8_t value;

    (void)SHARED_RAM_read_field( boot_handoff_st, dataset_valid_mask, &value );

    return( value );
}

/****************************** END OF FILE *******************************************************/
