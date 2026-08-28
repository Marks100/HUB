/*! \file
*               Author: mstewart
*   \brief      APP's UDS service table implementation - see UDS_config.h for scope
*
*   \note       Seed/key handling is a placeholder, not a real challenge/response algorithm: the
*               seed is a fixed dummy value and SendKey accepts any key unconditionally. Good
*               enough to exercise the EXTENDED -> SecurityAccess -> PROGRAMMING gate end to end;
*               swap in a real algorithm (matching whatever the tester side implements) before
*               this is exposed on a bus anyone untrusted can reach.
*
*   \note       RoutineControl's Force Boot Mode routine (0x31 0x01 0xF518) reaches the same FBL
*               entry as EXTENDED -> SecurityAccess -> PROGRAMMING, deliberately without requiring
*               that gate first - the generic SLP3 flash procedure tries it immediately after
*               entering EXTENDED, before any SecurityAccess exchange. This means any node on the
*               bus can force a reset into FBL (the DoS the session-table gate above was written to
*               prevent - see its comment), but not force an actual flash: FBL re-runs its own
*               independent SecurityAccess before honouring 0x34 RequestDownload regardless of how
*               it was entered. Accepted trade-off, not an oversight - see FBL/Src/UDS_CFG/
*               UDS_config.c's session table comment for the FBL-side half of this.
***************************************************************************************************/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "SHARED_RAM.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_security_send_key( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_routine_force_boot_mode( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* Level 1 and level 2 share the one stub algorithm - see the constants' comment in UDS_config.h for
   why both are listed. Every row is EXTENDED-only: SecurityAccess is what authorises the jump into
   the bootloader, so it must not be reachable from DEFAULT. */
STATIC UDS_subfunction_table_st security_access_subfuncs_s[] =
{
    { APP_SECURITY_LEVEL_1_SEED, uds_handle_security_request_seed, UDS_SES_EXTENDED, 0u },
    { APP_SECURITY_LEVEL_1_KEY,  uds_handle_security_send_key,     UDS_SES_EXTENDED, 0u },
    { APP_SECURITY_LEVEL_2_SEED, uds_handle_security_request_seed, UDS_SES_EXTENDED, 0u },
    { APP_SECURITY_LEVEL_2_KEY,  uds_handle_security_send_key,     UDS_SES_EXTENDED, 0u },
};

/* required_sec_level = 0 is deliberate here, not the usual "not gated yet" placeholder - see this
   file's header note on why Force Boot Mode intentionally skips the SecurityAccess gate. */
STATIC UDS_subfunction_table_st routine_control_subfuncs_s[] =
{
    { APP_ROUTINE_ID_FORCE_BOOT_MODE, uds_handle_routine_force_boot_mode, UDS_SES_EXTENDED, 0u },
};

/***************************************************************************************************
**                              Service Table                                                     **
***************************************************************************************************/
STATIC UDS_service_table_st uds_service_table_s[] =
{
    { UDS_SID_SECURITY_ACCESS, security_access_subfuncs_s,
      (u8_t)( sizeof( security_access_subfuncs_s ) / sizeof( security_access_subfuncs_s[0] ) ) },
    { UDS_SID_ROUTINE_CONTROL, routine_control_subfuncs_s,
      (u8_t)( sizeof( routine_control_subfuncs_s ) / sizeof( routine_control_subfuncs_s[0] ) ) },
};

/***************************************************************************************************
**                              Session Transition Table                                          **
***************************************************************************************************/
/* APP's 0x10 policy - a whitelist, anything absent is refused with NRC 0x7E. See
   UDS_session_transition_st in UDS.h for the matching rules.

   Entering PROGRAMMING is the one guarded transition, because it is what reboots the ECU into the
   flash-capable bootloader: it is reachable only from EXTENDED and only once SecurityAccess has
   unlocked level 1. That is the conventional DEFAULT -> EXTENDED -> unlock -> PROGRAMMING tester
   sequence - RoutineControl's Force Boot Mode (see routine_control_subfuncs_s below) is the one
   deliberate exception that reaches PROGRAMMING without it, see this file's header note.

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
STATIC u8_t uds_handle_security_request_seed( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
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
STATIC u8_t uds_handle_security_send_key( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    UDS_set_security_level( APP_SECURITY_LEVEL_1 );
    *len_p = 0u;
    *nrc_p = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x31 StartRoutine 0xF518 - Force Boot Mode
*   \details       Sets the shared-RAM FBL request flag and schedules the same delayed reset
*                  0x10 0x02 (Programming session) would - see this file's header note for why this
*                  routine intentionally reaches that outcome without going through SecurityAccess
*                  first. UDS_DELAYED_ACTION_TIMEOUT_MS still elapses first so this positive
*                  response reaches the tester before the reset fires.
***************************************************************************************************/
STATIC u8_t uds_handle_routine_force_boot_mode( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    SHARED_RAM_set_fbl_request( TRUE );
    UDS_schedule_pending_action( UDS_PENDING_ACTION_PROGRAMMING_SESSION );

    *len_p = 0u;
    *nrc_p = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/****************************** END OF FILE *******************************************************/
