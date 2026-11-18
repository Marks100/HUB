/*! \file
*               Author: mstewart
*   \brief      TMD_FBL module
*/

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "BOOT_HANDOFF.h"
#include "NVM_GEN2.h"
#include "FBL_NVM_BLOCKS.h"
#include "INTEGRATION_STUBS.h"
#include "TMD_MBA.h"
#include "TMD_STARTUP.h"
#include "TMD_FBL.h"

/***************************************************************************************************
**                                    Private Variables                                           **
***************************************************************************************************/
STATIC u8_t tmd_fbl_baud_param_s    = 0u;
STATIC u8_t tmd_fbl_response_owed_s = BOOT_HANDOFF_RESPONSE_NONE;

/***************************************************************************************************
**                                  Public Functions                                              **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Start the model: the FBL has not run, so it has found nothing
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void TMD_FBL_init( void )
{
    tmd_fbl_baud_param_s    = 0u;
    tmd_fbl_response_owed_s = BOOT_HANDOFF_RESPONSE_NONE;
}
/*!
****************************************************************************************************
*
*   \brief         The FBL starts, if the application asked for it
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Run after BM, which has started the handoff RAM. The application asks for the FBL by the request in it. The FBL takes the baud
*                  rate and the response the application hands over, once (they are gone from the handoff RAM), starts its NVM as its own start does
*                  and registers the block it keeps its note in (which the application only reads). If the ECU kept a write (0x3D
*                  WriteMemoryByAddress) it makes it, and clears the request as its start does: the request is gone whatever happened, and a power cut
*                  in the middle of it loses the handoff RAM. The firmware's RAM is wiped when the FBL is done, as the reset to the application does.
*                  A start the application did not hand over at leaves the FBL not run, with nothing found.
*
*****************************************************************************************************/
void TMD_FBL_start( void )
{
    tmd_fbl_baud_param_s    = 0u;
    tmd_fbl_response_owed_s = BOOT_HANDOFF_RESPONSE_NONE;

    if( BOOT_HANDOFF_get_fbl_request() == TRUE )
    {
        /* A LinkControl hand-over: the FBL sets its bit rate to the baud rate asked for and sends the response it owes, once */
        tmd_fbl_baud_param_s    = BOOT_HANDOFF_get_baud_param();
        tmd_fbl_response_owed_s = BOOT_HANDOFF_get_response_owed();
        BOOT_HANDOFF_set_baud_param( 0u );
        BOOT_HANDOFF_set_response_owed( BOOT_HANDOFF_RESPONSE_NONE );

        NVM_GEN2_init( &nvm_gen2_hw_interface_s );
        NVM_GEN2_register_block( FBL_VPM_APPLIED_BLOCK_ID, &fbl_nvm_gen2_vpm_applied_block_s );

        if( TMD_MBA_is_write_pending() == TRUE )
        {
            (void)TMD_MBA_start_server();
            BOOT_HANDOFF_set_fbl_request( FALSE );
        }

        TMD_STARTUP_init_ram();    /* the reset to the application */
    }
}

/*!
****************************************************************************************************
*
*   \brief         The baud rate the FBL found at its last start
*
*   \author        mstewart
*
*   \return        The baud rate a LinkControl hand-over asked for, 0 for a start the application did not hand over at
*
*****************************************************************************************************/
u8_t TMD_FBL_get_baud_param( void )
{
    return( tmd_fbl_baud_param_s );
}

/*!
****************************************************************************************************
*
*   \brief         The response the FBL found at its last start that the application owed
*
*   \author        mstewart
*
*   \return        BOOT_HANDOFF_RESPONSE_*, BOOT_HANDOFF_RESPONSE_NONE if the application owed none
*
*****************************************************************************************************/
u8_t TMD_FBL_get_response_owed( void )
{
    return( tmd_fbl_response_owed_s );
}

/****************************** END OF FILE *******************************************************/