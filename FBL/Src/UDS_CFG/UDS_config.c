/*! \file
*               Author: mstewart
*   \brief      FBL's UDS service table - see UDS_config.h for scope
*
*   Every handler in the tables below is FBL's own ready-made FBL_uds_handle_*() (see FBL.h) - this
*   file owns only the genuinely project-specific pieces: which subfunction/routine/DID values are
*   exposed, what session and security level each requires, and the RID/DID constants themselves
*   (UDS_config.h). See FBL.h's "UDS Service Handlers" section comment for why the handler bodies
*   live there instead of being duplicated per platform.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "FBL.h"

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* ISO 14229-1's standard SecurityAccess convention: odd = requestSeed, even = sendKey. FBL has no
   concept of graded levels - see FBL.h's comment on why this is "four doors to one room, not
   graded access" - so rather than enumerate every level's seed/key pair as its own row, match on
   the low bit alone and let the handler take whatever value actually arrived (see the subfunc
   parameter): any odd value is a seed request, any even value is a key send, full stop. */
STATIC UDS_subfunction_table_st security_access_subfuncs_s[] =
{
    { 0x01u, 0x01u, FBL_uds_handle_request_seed, UDS_SES_PROGRAMMING, 0u },  /* odd  - RequestSeed */
    { 0x02u, 0x01u, FBL_uds_handle_send_key,     UDS_SES_PROGRAMMING, 0u },  /* even - SendKey     */
};

STATIC UDS_subfunction_table_st routine_control_subfuncs_s[] =
{
    { ROUTINE_ID_ERASE_MEMORY, 0xFFFFu, FBL_uds_handle_erase_memory, UDS_SES_PROGRAMMING, 1u },
    { ROUTINE_ID_CHECK_MEMORY, 0xFFFFu, FBL_uds_handle_check_memory, UDS_SES_PROGRAMMING, 0u },
};

STATIC UDS_subfunction_table_st request_download_subfuncs_s[] =
{
    { 0x0000u, 0xFFFFu, FBL_uds_handle_request_download, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st transfer_data_subfuncs_s[] =
{
    { 0x0000u, 0xFFFFu, FBL_uds_handle_transfer_data, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st request_transfer_exit_subfuncs_s[] =
{
    { 0x0000u, 0xFFFFu, FBL_uds_handle_request_transfer_exit, UDS_SES_PROGRAMMING, 1u },
};

/***************************************************************************************************
**                              Service Table                                                     **
***************************************************************************************************/
STATIC UDS_service_table_st uds_service_table_s[] =
{
    { UDS_SID_SECURITY_ACCESS,   security_access_subfuncs_s,
      (u8_t)( sizeof( security_access_subfuncs_s ) / sizeof( security_access_subfuncs_s[0] ) ) },
    { UDS_SID_ROUTINE_CONTROL,   routine_control_subfuncs_s,        2u },
    { UDS_SID_REQUEST_DOWNLOAD,  request_download_subfuncs_s,       1u },
    { UDS_SID_TRANSFER_DATA,     transfer_data_subfuncs_s,          1u },
    { UDS_SID_REQUEST_TRANSFER_EXIT, request_transfer_exit_subfuncs_s, 1u },
};

/***************************************************************************************************
**                              Session Transition Table                                          **
***************************************************************************************************/
/* FBL's 0x10 policy - a whitelist, anything absent is refused with NRC 0x7E. See
   UDS_session_transition_st in UDS.h for the matching rules.

   Deliberately looser than APP's table (APP/Src/UDS_CFG/UDS_config.c): getting here at all already
   required APP to authenticate the tester before rebooting into FBL, and fbl_main() starts this
   partition pre-unlocked on the strength of that, so re-gating the sessions would only ask the same
   tester to prove itself twice.

   Reaching DEFAULT from anywhere is the tester saying "done programming" and resets the ECU so BM
   re-validates APP and boots it. Wildcarding from_session is what makes the conventional exit
   PROGRAMMING -> EXTENDED -> DEFAULT behave the same as a direct PROGRAMMING -> DEFAULT, and it
   also gives the S3 timeout a route home if the tester simply vanishes mid-session (see
   uds_handle_s3_timeout - PROGRAMMING itself is exempt there, so FBL's own boot delay still owns
   the "no tester ever showed up" case). */
STATIC const UDS_session_transition_st uds_session_table_s[] =
{
    /* from         to                   sec  action */
    { UDS_SES_ANY,  UDS_SES_PROGRAMMING, 0u,  UDS_PENDING_ACTION_NONE },
    { UDS_SES_ANY,  UDS_SES_EXTENDED,    0u,  UDS_PENDING_ACTION_NONE },
    { UDS_SES_ANY,  UDS_SES_DEFAULT,     0u,  UDS_PENDING_ACTION_SOFT_RESET },
};

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
const UDS_service_table_st* UDS_get_service_table( void )
{
    return( uds_service_table_s );
}

u8_t UDS_get_service_table_size( void )
{
    return( (u8_t)( sizeof( uds_service_table_s ) / sizeof( uds_service_table_s[0] ) ) );
}

const UDS_session_transition_st* UDS_get_session_table( void )
{
    return( uds_session_table_s );
}

u8_t UDS_get_session_table_size( void )
{
    return( (u8_t)( sizeof( uds_session_table_s ) / sizeof( uds_session_table_s[0] ) ) );
}

/****************************** END OF FILE *******************************************************/
