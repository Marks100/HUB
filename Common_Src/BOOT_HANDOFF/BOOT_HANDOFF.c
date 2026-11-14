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
/* Every field starts at zero: no FBL request, no datasets confirmed, no baud rate change, nothing owed */
static const boot_handoff_st boot_handoff_defaults_c =
{
    .fbl_request         = 0u,
    .dataset_valid_mask  = 0u,
    .fbl_timeout         = 0u,
    .baud_param          = 0u,
    .response_owed       = BOOT_HANDOFF_RESPONSE_NONE
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
*                  jumping to APP, or vice versa) must not wipe fbl_request/dataset state that was
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

/*!
****************************************************************************************************
*
*   \brief         Set the CAN baud rate the FBL starts at
*
*   \author        MS
*
*   \param         baud_param - LinkControl baud rate parameter (see HAL_CAN_baud_is_supported()), 0 for none
*
*   \return        none
*
*   \note          Set by the application's LinkControl (0x87), read and cleared by the FBL at its start.
*
***************************************************************************************************/
void BOOT_HANDOFF_set_baud_param( u8_t baud_param )
{
    (void)SHARED_RAM_write_field( boot_handoff_st, baud_param, &baud_param );
}

/*!
****************************************************************************************************
*
*   \brief         Get the CAN baud rate the FBL starts at
*
*   \author        MS
*
*   \return        LinkControl baud rate parameter, 0 for none (also if the region is not valid)
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_baud_param( void )
{
    u8_t value = 0u;

    (void)SHARED_RAM_read_field( boot_handoff_st, baud_param, &value );

    return( value );
}

/*!
****************************************************************************************************
*
*   \brief         Set the response the next image owes the tester
*
*   \author        MS
*
*   \param         response - BOOT_HANDOFF_RESPONSE_*
*
*   \return        none
*
***************************************************************************************************/
void BOOT_HANDOFF_set_response_owed( u8_t response )
{
    (void)SHARED_RAM_write_field( boot_handoff_st, response_owed, &response );
}

/*!
****************************************************************************************************
*
*   \brief         Get the response the next image owes the tester
*
*   \author        MS
*
*   \return        BOOT_HANDOFF_RESPONSE_*, none if nothing is owed (also if the region is not valid)
*
***************************************************************************************************/
u8_t BOOT_HANDOFF_get_response_owed( void )
{
    u8_t value = BOOT_HANDOFF_RESPONSE_NONE;

    (void)SHARED_RAM_read_field( boot_handoff_st, response_owed, &value );

    return( value );
}

/****************************** END OF FILE *******************************************************/
