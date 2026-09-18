/*! \file
*               Author: mstewart
*   \brief      APP's UDS service table implementation - see UDS_config.h for scope
*
*   \note       Seed/key handling is still not a real challenge/response algorithm, just further
*               along than a pure no-op: RequestSeed hands out a fresh RNG value per call (not a
*               fixed constant - see uds_handle_security_request_seed()), and SendKey now computes
*               and compares the expected key (see uds_handle_security_send_key() and the
*               APP_SECURITY_KEY_SECRET_LEVEL_* defines) - but a mismatch is not enforced, only
*               recorded (uds_key_1/2/3_check_ok_s), so every key is still accepted unconditionally
*               for now. Good enough to exercise the EXTENDED -> SecurityAccess -> PROGRAMMING/key-
*               write gates end to end while the real derivation is validated; swap in a real
*               algorithm (matching whatever the tester side implements) AND start enforcing the
*               comparison before this is exposed on a bus anyone untrusted can reach.
***************************************************************************************************/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "FAULT_MGR.h"
#include "MSG_SCHED.h"
#include "APP_NVM_BLOCKS.h"
#include "NVM_GEN2.h"
#include "RNG.h"
#include "VER.h"
#include "printf.h"
#include "SHARED_RAM.h"
#include "MCU_JUMP.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Placeholder per-level "secret" the expected key is derived from below - XOR-with-seed is not a
   real key-derivation function, just enough to make uds_handle_security_send_key()'s check
   meaningful (right key vs wrong key actually differ) instead of a no-op. Private to this file -
   nothing outside SecurityAccess needs to know these. Swap for a real algorithm (and matching
   secrets on the tester side) before this is exposed on an untrusted bus. */
#define APP_SECURITY_KEY_SECRET_LEVEL_1  ( 0xA5A5A5A5ul )
#define APP_SECURITY_KEY_SECRET_LEVEL_2  ( 0x5A5A5A5Aul )
#define APP_SECURITY_KEY_SECRET_LEVEL_3  ( 0x3C3C3C3Cul )

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_security_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_check_programming_preconditions( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_communication_control( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_control_dtc_setting( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_write_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_read_app_sw_id( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC void uds_handle_programming_session_notify( void );

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* SecurityAccess (0x27) is a row in uds_service_table_s below like every other service, but its
   dispatch is still special: uds_process_security_access() (UDS.c) picks between exactly these two
   entries by subFunction parity (odd/even), never by searching for a matching subfunction value -
   ISO 14229-1's odd=requestSeed/even=sendKey convention is baked into the engine itself (see
   UDS_security_access_entry_st's comment in UDS.h), so this file only ever supplies its own two
   handler functions, EXTENDED-only, no security level needed to reach either (SecurityAccess is
   what grants a level in the first place). */
/* Seed last handed out by uds_handle_security_request_seed(), and whether each level's most
   recent SendKey matched the key derived from it. Not used to gate anything yet (see
   uds_handle_security_send_key()'s comment) - exists so the comparison has somewhere to leave its
   result for a debugger to watch, the way app_fbl_fingerprint_g exists for FBL's fingerprint
   (APP_NVM_BLOCKS.c's comment), rather than the check being computed and immediately discarded. */
STATIC u32_t          uds_last_seed_s          = 0u;
STATIC false_true_et  uds_key_1_check_ok_s     = FALSE;
STATIC false_true_et  uds_key_2_check_ok_s     = FALSE;
STATIC false_true_et  uds_key_3_check_ok_s     = FALSE;

STATIC const UDS_security_access_entry_st security_access_cfg_s[UDS_SECURITY_ACCESS_ENTRY_COUNT] =
{
    { uds_handle_security_request_seed, UDS_SES_EXTENDED, 0u },  /* RequestSeed, any level */
    { uds_handle_security_send_key,     UDS_SES_EXTENDED, 0u },  /* SendKey, level from subfunc */
};

/* ECU Reset (0x11) is also a row below - see UDS_ecu_reset_cfg_st's comment in UDS.h. Same reset
   for both halves, matching the old .perform_soft_reset/.perform_hard_reset in app_uds_func_table_s
   (APP/Src/INT_STUBS/INTEGRATION_STUBS.c) this replaced. */
STATIC const UDS_ecu_reset_cfg_st ecu_reset_cfg_s =
{
    MCU_JUMP_software_reset,
    MCU_JUMP_software_reset
};

/* TesterPresent (0x3E) is also a row below - see UDS_tester_present_cfg_st's comment in UDS.h.
   NULL_P: APP has no project-specific action to take when a TesterPresent ping arrives (unlike
   FBL's tester_present_cfg_s, FBL/Src/UDS_CFG/UDS_config.c, which resets its boot-delay timer) -
   the row still exists so this SID is registered/dispatched the same uniform way as every other
   internally-handled one, rather than silently falling back to whatever UDS.c does when a project
   supplies no row at all for it. */
STATIC const UDS_tester_present_cfg_st tester_present_cfg_s =
{
    NULL_P
};

/* Callable from EXTENDED with no security unlock - SLP3 §2.2.7.2 runs this during the
   Pre-Programming Step, before SecurityAccess. See uds_handle_check_programming_preconditions()
   for what "no real gate yet" means here. */
STATIC UDS_subfunction_table_st routine_control_subfuncs_s[] =
{
    { ROUTINE_ID_CHECK_PROGRAMMING_PRECONDITIONS, UDS_SUBFUNC_MASK_EXACT,
      uds_handle_check_programming_preconditions, UDS_SES_EXTENDED, 0u },
};

/* UDS_SUBFUNC_MASK_ANY matches every incoming Control Type - the handler itself validates
   $00/$01/$03 via the subfunc parameter (see uds_handle_communication_control()'s comment on why
   one masked row, not one exact row per value, matches this project's SecurityAccess precedent
   above). UDS_SUBFUNC_IGNORED documents that the paired value is never actually compared
   against anything under that mask - see its comment in UDS.h. */
STATIC UDS_subfunction_table_st communication_control_subfuncs_s[] =
{
    { UDS_SUBFUNC_IGNORED, UDS_SUBFUNC_MASK_ANY, uds_handle_communication_control, UDS_SES_EXTENDED, 0u },
};

/* Same masked-family approach as communication_control_subfuncs_s above - $01/$02 validated by
   the handler via subfunc. */
STATIC UDS_subfunction_table_st control_dtc_setting_subfuncs_s[] =
{
    { UDS_SUBFUNC_IGNORED, UDS_SUBFUNC_MASK_ANY, uds_handle_control_dtc_setting, UDS_SES_EXTENDED, 0u },
};

/* Each key gated by its own SecurityAccess level (2 for key 1, 3 for key 2 - see
   APP_SECURITY_LEVEL_2/_3's comment in UDS_config.h for the >= hierarchy this implies), not both
   on level 1: writing key material is a write to the ECU, same class of gate FBL's fingerprint
   write uses (FBL/Src/UDS_CFG/UDS_config.c), just split per key instead of shared. EXTENDED rather
   than PROGRAMMING: unlike FBL's fingerprint (written mid-flash-sequence), these are written while
   APP is running normally, and SecurityAccess itself is only reachable from EXTENDED (see
   security_access_cfg_s above). */
STATIC UDS_subfunction_table_st write_data_by_identifier_subfuncs_s[] =
{
    { DID_APP_KEY_1, UDS_SUBFUNC_MASK_EXACT, uds_handle_write_key, UDS_SES_EXTENDED, APP_SECURITY_LEVEL_2 },
    { DID_APP_KEY_2, UDS_SUBFUNC_MASK_EXACT, uds_handle_write_key, UDS_SES_EXTENDED, APP_SECURITY_LEVEL_3 },
};

/* UDS_SES_ANY/no security - an identification read, same class of DID as
   activeDiagnosticSession (0xF186), readable in any session without unlocking anything first -
   matches FBL's read_data_by_identifier_subfuncs_s (FBL/Src/UDS_CFG/UDS_config.c) for the sibling
   DID (bootSoftwareIdentification there, applicationSoftwareIdentification here). */
STATIC UDS_subfunction_table_st read_data_by_identifier_subfuncs_s[] =
{
    { DID_APP_SOFTWARE_IDENTIFICATION, UDS_SUBFUNC_MASK_EXACT, uds_handle_read_app_sw_id, UDS_SES_ANY, 0u },
};

/* APP's 0x10 policy - a whitelist, anything absent is refused with NRC 0x7E. See
   UDS_session_transition_st in UDS.h for the matching rules. Declared ahead of uds_service_table_s
   below since that table's 0x10 row names this array.

   Entering PROGRAMMING is the one guarded transition, because it is what reboots the ECU into the
   flash-capable bootloader: it requires SecurityAccess level 1, and level 1 is only ever unlocked
   from EXTENDED (security_access_cfg_s above) and cleared the moment the session drops to DEFAULT
   (UDS.c), so in practice a tester still has to go DEFAULT -> EXTENDED -> unlock -> PROGRAMMING -
   just enforced by the security check below rather than a separate from-session gate.

   Nothing here schedules a reset back to APP on reaching DEFAULT - APP never sits in PROGRAMMING
   long enough to leave it (the transition into PROGRAMMING resets straight into FBL), so that rule
   belongs in FBL's table, not this one.

   The PROGRAMMING row is the only one with an on_transition callback: entering PROGRAMMING means a
   tester wants FBL entry, so uds_handle_programming_session_notify() (below) sets the shared-RAM
   FBL-request flag before the reset that transition schedules - see that function's own comment.
   The other two rows have nothing project-specific to do on their own account, hence NULL_P. */
STATIC const UDS_session_transition_st uds_session_table_s[] =
{
    /* to                   sec                    action                              on_transition */
    { UDS_SES_DEFAULT,     0u,                    UDS_PENDING_ACTION_NONE,                NULL_P },
    { UDS_SES_EXTENDED,    0u,                    UDS_PENDING_ACTION_NONE,                NULL_P },
    { UDS_SES_PROGRAMMING, APP_SECURITY_LEVEL_1,  UDS_PENDING_ACTION_PROGRAMMING_SESSION, uds_handle_programming_session_notify },
};

/***************************************************************************************************
**                              Service Table                                                     **
***************************************************************************************************/
/* Every SID this project answers, SessionControl (0x10), SecurityAccess (0x27), TesterPresent
   (0x3E) and ECU Reset (0x11) included - see UDS_service_table_st's comment in UDS.h for why
   table_p's shape depends on sid (session transitions for 0x10, the seed/key pair for 0x27, the
   notify callback for 0x3E, the two reset callbacks for 0x11, the usual subfunction/DID table for
   everything else). See FBL/Src/UDS_CFG/UDS_config.c's copy of this same note. */
STATIC UDS_service_table_st uds_service_table_s[] =
{
    { UDS_SID_DIAGNOSTIC_SESSION_CONTROL, uds_session_table_s,                 UDS_TABLE_ROWS( uds_session_table_s ) },
    { UDS_SID_SECURITY_ACCESS,            security_access_cfg_s,               UDS_SECURITY_ACCESS_ENTRY_COUNT },
    { UDS_SID_ECU_RESET,                  &ecu_reset_cfg_s,                    1u },
    { UDS_SID_TESTER_PRESENT,             &tester_present_cfg_s,               1u },
    { UDS_SID_READ_DATA_BY_IDENTIFIER,    read_data_by_identifier_subfuncs_s,  UDS_TABLE_ROWS( read_data_by_identifier_subfuncs_s ) },
    { UDS_SID_ROUTINE_CONTROL,            routine_control_subfuncs_s,          UDS_TABLE_ROWS( routine_control_subfuncs_s ) },
    { UDS_SID_COMMUNICATION_CONTROL,      communication_control_subfuncs_s,    UDS_TABLE_ROWS( communication_control_subfuncs_s ) },
    { UDS_SID_CONTROL_DTC_SETTING,        control_dtc_setting_subfuncs_s,      UDS_TABLE_ROWS( control_dtc_setting_subfuncs_s ) },
    { UDS_SID_WRITE_DATA_BY_IDENTIFIER,   write_data_by_identifier_subfuncs_s, UDS_TABLE_ROWS( write_data_by_identifier_subfuncs_s ) },
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

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*   \brief         0x27 0x01/0x03/0x05 - SecurityAccess RequestSeed, shared across all three levels
*   \details       Placeholder still: SendKey accepts any key back regardless of what seed was
*                  handed out (see file header note and the three uds_handle_security_send_key_
*                  level*() below), so this isn't a real challenge/response yet. What it does do is
*                  hand back a fresh RNG_gen_u32() value every call rather than a fixed constant, so
*                  two RequestSeeds in a row (or two different devices) don't see the same bytes.
*                  RNG.h itself is not a CSPRNG (Xorshift128+, not crypto-secure) - fine for now
*                  since SendKey doesn't actually check the seed against anything yet; swap both
*                  together (a real seed source AND a real key check) before this is exposed on an
*                  untrusted bus. subfunc is available if a real algorithm ever needs to vary the
*                  seed by level. RNG is seeded once at boot from a hardware-unique value - see
*                  RNG_init() in main.c.
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    uds_last_seed_s = RNG_gen_u32();

    (void)subfunc;

    STDC_copy_32bit_to_buffer_msb_first( data_p, uds_last_seed_s );
    *len_p = 4u;
    *nrc_p    = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x27 0x02/0x04/0x06 - SecurityAccess SendKey, shared across all three levels
*   \details       One handler switching on subfunc (the actual subFunction that fired - UDS.c's
*                  uds_process_security_access() always passes the real incoming value here, never
*                  just the row index) instead of a dedicated function per level: the mechanism is
*                  identical per level (derive the expected key from uds_last_seed_s and that
*                  level's APP_SECURITY_KEY_SECRET_LEVEL_*, compare, record the result), only the
*                  secret/level/result-variable differ. The level to grant is returned, not set
*                  directly - uds_process_security_access() (UDS.c) calls UDS_set_security_level()
*                  itself on a nonzero return, see UDS_security_access_entry_st's comment in UDS.h.
*
*                  The key is genuinely checked now, but a mismatch is NOT enforced - the level is
*                  granted either way, same as before this check existed. Intentionally a soft
*                  rollout step: it lets the comparison (and whatever the real tester side sends) be
*                  observed/validated via uds_key_1/2/3_check_ok_s without risking locking out a
*                  working flash/diagnostic sequence if the derivation is wrong. Once verified,
*                  enforce it by returning UDS_RC_INVALID_KEY (and 0u) instead of granting on a
*                  mismatch.
***************************************************************************************************/
STATIC u8_t uds_handle_security_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u32_t received_key = STDC_copy_buffer_msb_first_to_32bit( data_p );
    u32_t expected_key;
    u8_t  level;

    switch( subfunc )
    {
        case 0x02u:  /* Level 1 - gates PROGRAMMING */
            level                = APP_SECURITY_LEVEL_1;
            expected_key         = uds_last_seed_s ^ APP_SECURITY_KEY_SECRET_LEVEL_1;
            uds_key_1_check_ok_s = ( received_key == expected_key ) ? TRUE : FALSE;
            break;

        case 0x04u:  /* Level 2 - gates writing app_key_1_blk_g */
            level                = APP_SECURITY_LEVEL_2;
            expected_key         = uds_last_seed_s ^ APP_SECURITY_KEY_SECRET_LEVEL_2;
            uds_key_2_check_ok_s = ( received_key == expected_key ) ? TRUE : FALSE;
            break;

        case 0x06u:  /* Level 3 - gates writing app_key_2_blk_g */
            level                = APP_SECURITY_LEVEL_3;
            expected_key         = uds_last_seed_s ^ APP_SECURITY_KEY_SECRET_LEVEL_3;
            uds_key_3_check_ok_s = ( received_key == expected_key ) ? TRUE : FALSE;
            break;

        default:
            /* uds_process_security_access() (UDS.c) only ever calls this handler for an even
               subfunc (SendKey family) - the specific value could still be something other than
               0x02/0x04/0x06 if a future level's row exists without a matching case here, so this
               stays as a defensive fail-closed rather than granting an undefined level. */
            *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
            return( 0u );
    }

    *len_p = 0u;
    *nrc_p = UDS_RC_POSITIVE_RESPONSE;

    return( level );
}

/*!
****************************************************************************************************
*   \brief         0x31 0x01 0x0203 - RoutineControl StartRoutine, CheckProgrammingPreconditions
*   \details       SLP3 §2.2.7.2: lets a tester ask, before ever touching SecurityAccess or
*                  PROGRAMMING session, whether this ECU is in a state that allows reprogramming
*                  (spec's own example is an engine ECU refusing while the engine runs). This
*                  product has no such interlock defined yet - see the routine status record note
*                  below - so it always reports every precondition fulfilled. Wire a real check in
*                  here (e.g. an active-fault or in-motion query) once this product defines one;
*                  the request/response plumbing and table wiring do not need to change to do that.
***************************************************************************************************/
STATIC u8_t uds_handle_check_programming_preconditions( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)subfunc;

    /* Routine Status Record: $00 - no unfulfilled preconditions, matching this spec's $00-means-
       success convention used everywhere else (Erase Memory, Check Programming Dependencies, ...) */
    data_p[0] = 0x00u;
    *len_p    = 1u;
    *nrc_p    = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x28 - CommunicationControl
*   \details       SLP3 §2.2.5: gates non-diagnostic bus traffic during the Pre/Post-Programming
*                  Steps. This product's only non-diagnostic TX is MSG_SCHED's cyclic sensor
*                  telemetry (see CAN_SENSOR_BASE_ID in INT_STUBS/INTEGRATION_STUBS.c) - there is
*                  no separate non-diagnostic RX path to gate, so $01 (enable RX, disable TX) and
*                  $03 (disable RX and TX) are treated the
*                  same: suspend MSG_SCHED. $00 resumes it. CommunicationType (data_p[0], always
*                  $01 Normal Communication for this product) is not otherwise checked. subfunc
*                  arrives already stripped of suppressPosRspMsgIndicationBit - see
*                  uds_process_rx_message()'s comment on suppress_response in UDS.c.
***************************************************************************************************/
STATIC u8_t uds_handle_communication_control( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    if( ( subfunc == 0x00u ) || ( subfunc == 0x01u ) || ( subfunc == 0x03u ) )
    {
        MSG_SCHED_set_tx_enabled( ( subfunc == 0x00u ) ? TRUE : FALSE );
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else
    {
        *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x85 - ControlDTCSetting
*   \details       SLP3 §2.2.12: suspends/resumes new DTC storage for the duration of the flash
*                  sequence, so a reset/erase on this ECU or the bus quieting down doesn't get
*                  latched as a permanent fault. The optional 3-byte $FFFFFF option record
*                  (deactivate all DTCs) is not checked - this product has one FAULT_MGR instance
*                  covering every fault ID, so "off" already means "all of them". subfunc arrives
*                  already stripped of suppressPosRspMsgIndicationBit - see
*                  uds_process_rx_message()'s comment on suppress_response in UDS.c.
***************************************************************************************************/
STATIC u8_t uds_handle_control_dtc_setting( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    if( ( subfunc == 0x01u ) || ( subfunc == 0x02u ) )
    {
        FAULT_MGR_set_dtc_setting( ( subfunc == 0x01u ) ? TRUE : FALSE );
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else
    {
        *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x2E WriteDataByIdentifier - app_key_1_blk_g/app_key_2_blk_g handler (DID 0xF900/0xF901)
*   \details       One handler for both keys: write_data_by_identifier_subfuncs_s below wires both
*                  DID_APP_KEY_1 and DID_APP_KEY_2 rows to it, and subfunc is the matched DID itself
*                  (uds_process_subfunc_id() in UDS.c), enough to pick the target key/block without a
*                  dedicated function per key. Response is just the echoed DID (data_p[0..1]
*                  untouched, same convention fbl_uds_handle_write_fingerprint()
*                  (FBL/Src/UDS_CFG/UDS_config.c) uses) - no payload beyond that, per spec's
*                  WriteDataByIdentifier format. *len_p on entry is however many bytes of key data
*                  the tester actually sent; anything other than exactly APP_KEY_LEN is rejected
*                  rather than silently padded/truncated, since a short or long key is never valid.
*                  Flushed synchronously (NVM_GEN2_flush_block(), not a request left for the next
*                  tick) so the write is durable on flash before the positive response goes out -
*                  same reasoning as fbl_uds_handle_write_fingerprint()'s comment.
***************************************************************************************************/
STATIC u8_t uds_handle_write_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    if( *len_p != APP_KEY_LEN )
    {
        *nrc_p = UDS_RC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
    else if( subfunc == DID_APP_KEY_1 )
    {
        STDC_memcpy( app_key_1_blk_g.key, data_p, APP_KEY_LEN );
        NVM_GEN2_flush_block( APP_KEY_1_BLOCK_ID );
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else if( subfunc == DID_APP_KEY_2 )
    {
        STDC_memcpy( app_key_2_blk_g.key, data_p, APP_KEY_LEN );
        NVM_GEN2_flush_block( APP_KEY_2_BLOCK_ID );
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else
    {
        /* write_data_by_identifier_subfuncs_s only ever wires DID_APP_KEY_1/_2 to this handler, so
           this is unreachable in practice - defensive fail-closed rather than assuming which key. */
        *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x22 ReadDataByIdentifier - applicationSoftwareIdentification (DID 0xF181)
*   \details       ISO 14229-1 Table C.1's standard applicationSoftwareIdentification DID (ASCII) -
*                  reports APP's own version (see VER.h/autoversion.h, generated at build time by
*                  sVersion from APP/project.yml) as "major.minor.patch-release_type", same format
*                  and same no-security/any-session convention as FBL_uds_handle_read_boot_sw_id()
*                  (FBL/Src/FBL.c) for the sibling DID_BOOT_SOFTWARE_IDENTIFICATION.
***************************************************************************************************/
STATIC u8_t uds_handle_read_app_sw_id( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u8_t  sw_version[SW_VERSION_NUM_SIZE];
    s16_t written;

    (void)subfunc;

    VER_get_sw_version_num( sw_version );

    written = PRINTF_snprintf( data_p, (u16_t)APP_SW_ID_MAX_LEN, "%u.%u.%u-%s",
                                sw_version[0], sw_version[1], sw_version[2], VER_get_sw_release_type() );

    *len_p = ( written > 0 ) ? (u16_t)written : 0u;
    *nrc_p = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         PROGRAMMING on_transition callback (uds_session_table_s above)
*   \details       Entering PROGRAMMING means a tester wants FBL entry - set the FBL request flag so
*                  BM boots FBL after the soft reset that transition schedules (see UDS.c's
*                  uds_apply_session_change()/uds_invoke_pending_action(); this callback runs before
*                  the reset is scheduled, which is the whole reason it exists).
*
*                  Whether the transition is allowed at all is not decided here - the row this is
*                  attached to already gates it on security level, and UDS.c only ever calls a row's
*                  on_transition for that exact row's to_session (never any other), so this needs no
*                  argument to know which transition fired.
***************************************************************************************************/
STATIC void uds_handle_programming_session_notify( void )
{
    SHARED_RAM_set_fbl_request( TRUE );
}

/****************************** END OF FILE *******************************************************/
