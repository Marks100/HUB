/*! \file
*               Author: mstewart
*   \brief      APP's UDS service table implementation - see UDS_config.h for scope
*
*   \note       Seed/key handling is a placeholder, not a real challenge/response algorithm: the
*               seed is a fixed dummy value and SendKey accepts any key unconditionally. Good
*               enough to exercise the EXTENDED -> SecurityAccess -> PROGRAMMING gate end to end;
*               swap in a real algorithm (matching whatever the tester side implements) before
*               this is exposed on a bus anyone untrusted can reach.
***************************************************************************************************/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_security_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* ISO 14229-1's standard SecurityAccess convention: odd = requestSeed, even = sendKey. APP has no
   concept of graded levels (see the stub note above - one placeholder algorithm behind everything),
   so rather than enumerate every level's seed/key pair as its own row, match on the low bit alone
   and let the handler take whatever value actually arrived (see the subfunc parameter): any odd
   value is a seed request, any even value is a key send, full stop. Every row is EXTENDED-only:
   SecurityAccess is what authorises the jump into the bootloader, so it must not be reachable from
   DEFAULT. */
STATIC UDS_subfunction_table_st security_access_subfuncs_s[] =
{
    { 0x01u, 0x01u, uds_handle_security_request_seed, UDS_SES_EXTENDED, 0u },  /* odd  - RequestSeed */
    { 0x02u, 0x01u, uds_handle_security_send_key,     UDS_SES_EXTENDED, 0u },  /* even - SendKey     */
};

/***************************************************************************************************
**                              Service Table                                                     **
***************************************************************************************************/
STATIC UDS_service_table_st uds_service_table_s[] =
{
    { UDS_SID_SECURITY_ACCESS, security_access_subfuncs_s,
      (u8_t)( sizeof( security_access_subfuncs_s ) / sizeof( security_access_subfuncs_s[0] ) ) },
};

/***************************************************************************************************
**                              Session Transition Table                                          **
***************************************************************************************************/
/* APP's 0x10 policy - a whitelist, anything absent is refused with NRC 0x7E. See
   UDS_session_transition_st in UDS.h for the matching rules.

   Entering PROGRAMMING is the one guarded transition, because it is what reboots the ECU into the
   flash-capable bootloader: it is reachable only from EXTENDED and only once SecurityAccess has
   unlocked level 1, so an unauthenticated node on the bus cannot force a reset into FBL (which
   would also be a denial-of-service on APP, independent of whether it could then flash anything).
   That is the conventional DEFAULT -> EXTENDED -> unlock -> PROGRAMMING tester sequence.

   Nothing here schedules a reset back to APP on reaching DEFAULT - APP never sits in PROGRAMMING
   long enough to leave it (the transition into PROGRAMMING resets straight into FBL), so that rule
   belongs in FBL's table, not this one. */
STATIC const UDS_session_transition_st uds_session_table_s[] =
{
    /* from              to                   sec  action */
    { UDS_SES_ANY,       UDS_SES_DEFAULT,     0u,  UDS_PENDING_ACTION_NONE },
    { UDS_SES_ANY,       UDS_SES_EXTENDED,    0u,  UDS_PENDING_ACTION_NONE },
    { UDS_SES_EXTENDED,  UDS_SES_PROGRAMMING, APP_SECURITY_LEVEL_1, UDS_PENDING_ACTION_PROGRAMMING_SESSION },
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

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*   \brief         0x27 0x01 - SecurityAccess RequestSeed
*   \details       Placeholder: always returns a fixed all-zero seed - see file header note.
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)subfunc;

    data_p[0] = 0x00u;
    data_p[1] = 0x00u;
    data_p[2] = 0x00u;
    data_p[3] = 0x00u;
    *len_p    = 4u;
    *nrc_p    = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x27 0x02 - SecurityAccess SendKey
*   \details       Placeholder: any key is accepted, unconditionally granting security level 1 -
*                  see file header note. Real verification (matching whatever RequestSeed actually
*                  hands out) belongs here before this is exposed on an untrusted bus.
***************************************************************************************************/
STATIC u8_t uds_handle_security_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)subfunc;
    (void)data_p;

    UDS_set_security_level( APP_SECURITY_LEVEL_1 );
    *len_p = 0u;
    *nrc_p = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/****************************** END OF FILE *******************************************************/
