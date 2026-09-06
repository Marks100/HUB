/*! \file
*               Author: mstewart
*   \brief      FBL's UDS service table - see UDS_config.h for scope
*
*   Most handlers in the tables below are FBL's own ready-made FBL_uds_handle_*() (see FBL.h) - this
*   file owns the genuinely project-specific pieces: which subfunction/routine/DID values are
*   exposed, what session and security level each requires, and the RID/DID constants themselves
*   (UDS_config.h). See FBL.h's "UDS Service Handlers" section comment for why those handler bodies
*   live there instead of being duplicated per platform.
*
*   Two exceptions, both local to this file (fbl_uds_handle_*(), lowercase) instead of FBL.c
*   wrappers, because what they do is project-specific rather than generic bootloader behaviour -
*   both mirror the equivalent handler in APP/Src/UDS_CFG/UDS_config.c exactly:
*     - SecurityAccess's RequestSeed/SendKey pair - the seed/key algorithm itself is project-
*       specific (mirrors uds_handle_security_request_seed()/uds_handle_security_send_key()).
*       FBL.c still owns the reusable lockout/failed-attempt tracking behind SendKey - see
*       FBL_security_report_key_result() in FBL.h. The RequestSeed/SendKey handshake itself (was a
*       seed actually requested first) is enforced by UDS.c itself now, before this file's SendKey
*       handler is even called - generic across every project, not FBL-specific at all - see
*       UDS_security_seed_was_requested()'s comment in UDS.h.
*     - WriteDataByIdentifier's fingerprint handler - persistence is project-specific (NVM_GEN2,
*       Common_Src/FBL_NVM_BLOCKS), mirroring uds_handle_write_key() touching app_key_1/2_blk_g
*       directly.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "FBL.h"
#include "MCU_JUMP.h"
#include "TIME_MGR.h"
#include "FBL_NVM_BLOCKS.h"
#include "NVM_GEN2.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Placeholder seed/key algorithm - not real challenge/response security, just enough to exercise
   the UDS exchange end to end. STM32F103 (medium-density) has no hardware RNG peripheral, so the
   seed is tick-based rather than truly random. The XOR mask matches Tool_cfg/CANFLASH/seedkeydll/
   seedkey.cpp's SECURITY_KEY_XOR_MASK exactly - change both together if this is ever replaced with
   a real customer/OEM algorithm. Six RequestSeed subfunction values are accepted below (0x01/0x03/
   0x05/0x07/0x09/0x0B), all sharing this one mask - see fbl_uds_handle_send_key()'s comment. */
#define FBL_SECURITY_SEED_MULTIPLIER (0x12345678ul)
#define FBL_SECURITY_KEY_XOR_MASK    (0xA5A5A5A5ul)

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u8_t fbl_uds_handle_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t fbl_uds_handle_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t fbl_uds_handle_write_fingerprint( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* SecurityAccess (0x27) is a row in uds_service_table_s below like every other service, but
   uds_process_security_access() (UDS.c) still dispatches it specially - picking between exactly
   these two entries by subFunction parity, never by searching for a matching value - since ISO
   14229-1's odd=requestSeed/even=sendKey convention is baked into the engine itself (see
   UDS_security_access_entry_st's comment in UDS.h). FBL has no concept of graded UDS levels
   anyway - every one of the six levels fbl_uds_handle_send_key() accepts returns the same level
   1u for UDS.c to grant (see that struct's comment on what the return value means), so both
   handlers ignore which specific odd/even value actually arrived (see the subfunc parameter each
   is handed).

   required_session is UDS_SES_ANY rather than UDS_SES_PROGRAMMING: a tester normally unlocks
   security against APP (in EXTENDED) before ever entering FBL, so FBL's own copy of this table
   only gets exercised when the ECU is unexpectedly already sitting in FBL (e.g. stuck there from
   a previous failed flash attempt) and the generic flash sequence blindly sends its standard
   EXTENDED-session handshake. The handlers themselves don't care what session they're called
   from, so gating on session here only serves to reject that recovery case for no benefit. */
/* Seed last handed out by fbl_uds_handle_request_seed() - exists so the comparison has somewhere
   to read from, the way APP's uds_last_seed_s does (APP/Src/UDS_CFG/UDS_config.c). */
STATIC u32_t fbl_last_seed_s = 0u;

STATIC const UDS_security_access_entry_st security_access_cfg_s[UDS_SECURITY_ACCESS_ENTRY_COUNT] =
{
    { fbl_uds_handle_request_seed, UDS_SES_ANY, 0u },  /* RequestSeed */
    { fbl_uds_handle_send_key,     UDS_SES_ANY, 0u },  /* SendKey     */
};

/* TesterPresent (0x3E) is also a row below - see UDS_tester_present_cfg_st's comment in UDS.h.
   FBL_reset_auto_boot_timer() is the TesterPresent-specific half of what used to be wired to both
   .message_received_notify and .tester_present_notify in fbl_uds_func_table_s
   (FBL/Src/INT_STUBS/INTEGRATION_STUBS.c) - that field now only covers the "any SID" half. */
STATIC const UDS_tester_present_cfg_st tester_present_cfg_s =
{
    FBL_reset_auto_boot_timer
};

/* ECU Reset (0x11) is also a row below - see UDS_ecu_reset_cfg_st's comment in UDS.h. Same reset
   for both halves, matching the old .perform_soft_reset/.perform_hard_reset in fbl_uds_func_table_s
   (FBL/Src/INT_STUBS/INTEGRATION_STUBS.c) this replaced. */
STATIC const UDS_ecu_reset_cfg_st ecu_reset_cfg_s =
{
    MCU_JUMP_software_reset,
    MCU_JUMP_software_reset
};

STATIC UDS_subfunction_table_st routine_control_subfuncs_s[] =
{
    { ROUTINE_ID_ERASE_MEMORY, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_erase_memory, UDS_SES_PROGRAMMING, 1u },
    { ROUTINE_ID_CHECK_MEMORY, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_check_memory, UDS_SES_PROGRAMMING, 0u },
    { ROUTINE_ID_STAY_IN_BOOT, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_stay_in_boot, UDS_SES_PROGRAMMING, 0u },
    { ROUTINE_ID_CHECK_PROGRAMMING_DEPENDENCIES, UDS_SUBFUNC_MASK_EXACT,
      FBL_uds_handle_check_programming_dependencies, UDS_SES_PROGRAMMING, 0u },
    { ROUTINE_ID_CHECK_PROGRAMMING_PRECONDITIONS, UDS_SUBFUNC_MASK_EXACT,
      FBL_uds_handle_check_programming_preconditions, UDS_SES_PROGRAMMING, 0u },
};

STATIC UDS_subfunction_table_st request_download_subfuncs_s[] =
{
    { 0x0000u, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_request_download, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st transfer_data_subfuncs_s[] =
{
    { 0x0000u, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_transfer_data, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st request_transfer_exit_subfuncs_s[] =
{
    { 0x0000u, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_request_transfer_exit, UDS_SES_PROGRAMMING, 1u },
};

/* Security level 1 - writing the fingerprint is a write to the ECU, same gate EraseMemory uses.
   fbl_uds_handle_write_fingerprint() is local to this file, not a FBL.c wrapper - persistence is
   project-specific (NVM_GEN2, Common_Src/FBL_NVM_BLOCKS), same reasoning as SecurityAccess's
   handlers above, and mirrors APP's own uds_handle_write_key() (APP/Src/UDS_CFG/UDS_config.c)
   touching app_key_1/2_blk_g directly. */
STATIC UDS_subfunction_table_st write_data_by_identifier_subfuncs_s[] =
{
    { DID_APPLICATION_SOFTWARE_FINGERPRINT, UDS_SUBFUNC_MASK_EXACT, fbl_uds_handle_write_fingerprint, UDS_SES_PROGRAMMING, 1u },
};

/* UDS_SES_ANY/no security - an identification read, same class of DID as
   activeDiagnosticSession (0xF186), readable in any session without unlocking anything first. */
STATIC UDS_subfunction_table_st read_data_by_identifier_subfuncs_s[] =
{
    { DID_BOOT_SOFTWARE_IDENTIFICATION, UDS_SUBFUNC_MASK_EXACT, FBL_uds_handle_read_boot_sw_id, UDS_SES_ANY, 0u },
};

/* FBL's 0x10 policy - a whitelist, anything absent is refused with NRC 0x7E. See
   UDS_session_transition_st in UDS.h for the matching rules. Declared ahead of uds_service_table_s
   below since that table's 0x10 row names this array.

   Deliberately looser than APP's table (APP/Src/UDS_CFG/UDS_config.c): getting here at all already
   required APP to authenticate the tester before rebooting into FBL, and fbl_main() starts this
   partition pre-unlocked on the strength of that, so re-gating the sessions would only ask the same
   tester to prove itself twice.

   Reaching DEFAULT is the tester saying "done programming" and resets the ECU so BM re-validates
   APP and boots it. The row is keyed only on to_session, so this fires the same way whether the
   tester got here via PROGRAMMING -> EXTENDED -> DEFAULT or a direct PROGRAMMING -> DEFAULT, and it
   also gives the S3 timeout a route home if the tester simply vanishes mid-session (see
   uds_handle_s3_timeout - PROGRAMMING itself is exempt there, so FBL's own boot delay still owns
   the "no tester ever showed up" case). */
/* No row needs an on_transition callback - FBL has nothing project-specific to do on any
   transition itself (see UDS_session_transition_st's comment in UDS.h for what that field is). */
STATIC const UDS_session_transition_st uds_session_table_s[] =
{
    /* to               sec  action                    on_transition */
    { UDS_SES_PROGRAMMING, 0u,  UDS_PENDING_ACTION_NONE,       NULL_P },
    { UDS_SES_EXTENDED,    0u,  UDS_PENDING_ACTION_NONE,       NULL_P },
    { UDS_SES_DEFAULT,     0u,  UDS_PENDING_ACTION_SOFT_RESET, NULL_P },
};

/***************************************************************************************************
**                              Service Table                                                     **
***************************************************************************************************/
/* Every SID this project answers, SessionControl (0x10), SecurityAccess (0x27), TesterPresent
   (0x3E) and ECU Reset (0x11) included - see UDS_service_table_st's comment in UDS.h for why
   table_p's shape depends on sid (session transitions for 0x10, the seed/key pair for 0x27, the
   notify callback for 0x3E, the two reset callbacks for 0x11, the usual subfunction/DID/RID table
   for everything else). See APP/Src/UDS_CFG/UDS_config.c's copy of this same note. */
STATIC UDS_service_table_st uds_service_table_s[] =
{
    { UDS_SID_DIAGNOSTIC_SESSION_CONTROL, uds_session_table_s,                 UDS_TABLE_ROWS( uds_session_table_s ) },
    { UDS_SID_SECURITY_ACCESS,            security_access_cfg_s,               UDS_SECURITY_ACCESS_ENTRY_COUNT },
    { UDS_SID_TESTER_PRESENT,             &tester_present_cfg_s,               1u },
    { UDS_SID_ECU_RESET,                  &ecu_reset_cfg_s,                    1u },
    { UDS_SID_ROUTINE_CONTROL,            routine_control_subfuncs_s,          UDS_TABLE_ROWS( routine_control_subfuncs_s ) },
    { UDS_SID_REQUEST_DOWNLOAD,           request_download_subfuncs_s,         UDS_TABLE_ROWS( request_download_subfuncs_s ) },
    { UDS_SID_TRANSFER_DATA,              transfer_data_subfuncs_s,            UDS_TABLE_ROWS( transfer_data_subfuncs_s ) },
    { UDS_SID_REQUEST_TRANSFER_EXIT,      request_transfer_exit_subfuncs_s,    UDS_TABLE_ROWS( request_transfer_exit_subfuncs_s ) },
    { UDS_SID_WRITE_DATA_BY_IDENTIFIER,   write_data_by_identifier_subfuncs_s, UDS_TABLE_ROWS( write_data_by_identifier_subfuncs_s ) },
    { UDS_SID_READ_DATA_BY_IDENTIFIER,    read_data_by_identifier_subfuncs_s,  UDS_TABLE_ROWS( read_data_by_identifier_subfuncs_s ) },
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
    return( UDS_TABLE_ROWS( uds_service_table_s ) );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*   \brief         0x27 0x01/0x03/0x05/0x07/0x09/0x0B - SecurityAccess RequestSeed, shared across
*                  every level
*   \details       Hands back a fresh tick-based seed every call (see file header note on why this
*                  isn't real randomness) - two RequestSeeds in a row don't see the same bytes.
***************************************************************************************************/
STATIC u8_t fbl_uds_handle_request_seed( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)subfunc;

    if( FBL_security_is_locked_out() == TRUE )
    {
        *nrc_p = UDS_RC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
    }
    else
    {
        /* UDS.c arms UDS_security_seed_was_requested() itself once this returns
           UDS_RC_POSITIVE_RESPONSE (see uds_process_security_access()'s comment in UDS.c) - nothing
           to record here. */
        fbl_last_seed_s = (u32_t)TIME_get_cumulative_run_time_ms() * FBL_SECURITY_SEED_MULTIPLIER;

        STDC_copy_32bit_to_buffer_msb_first( data_p, fbl_last_seed_s );
        *len_p = 4u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x27 0x02/0x04/0x06/0x08/0x0A/0x0C - SecurityAccess SendKey, shared across
*                  every level
*   \details       subfunc itself picks which of the six levels this is (0x01u/../0x0Bu below,
*                  matching the odd RequestSeed values FBL's own UDS_config.c exposes) - all six
*                  share fbl_last_seed_s ^ FBL_SECURITY_KEY_XOR_MASK, so an unrecognised subfunc
*                  and a wrong key take the same path (is_key_correct stays FALSE). On success,
*                  returns level 1 unconditionally so UDS.c grants it (see UDS_security_access_
*                  entry_st's comment in UDS.h) - FBL has no per-level UDS gating the way APP's
*                  UDS_config.c does. Attempt/lockout bookkeeping is only reported to FBL.c when a
*                  real check actually happened (a locked-out call doesn't count against the limit)
*                  - see FBL_security_report_key_result()'s comment. No seed-was-requested check
*                  needed here - uds_process_security_access() (UDS.c) never calls this handler at
*                  all without one, answering NRC 0x24 itself instead.
***************************************************************************************************/
STATIC u8_t fbl_uds_handle_send_key( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u32_t received_key = STDC_copy_buffer_msb_first_to_32bit( data_p );
    false_true_et key_correct = FALSE;
    u8_t          granted_level = 0u;

    if( ( subfunc == 0x02u ) || ( subfunc == 0x04u ) || ( subfunc == 0x06u ) ||
        ( subfunc == 0x08u ) || ( subfunc == 0x0Au ) || ( subfunc == 0x0Cu ) )
    {
        key_correct = ( received_key == ( fbl_last_seed_s ^ FBL_SECURITY_KEY_XOR_MASK ) );
    }

    FBL_security_report_key_result( key_correct );

    if( key_correct == TRUE )
    {
        granted_level = 1u;
        *len_p        = 0u;
        *nrc_p        = UDS_RC_POSITIVE_RESPONSE;
    }
    else if( FBL_security_is_locked_out() == TRUE )
    {
        *nrc_p = UDS_RC_EXCEEDED_NUMBER_OF_ATTEMPTS;
    }
    else
    {
        *nrc_p = UDS_RC_INVALID_KEY;
    }

    return( granted_level );
}

/*!
****************************************************************************************************
*   \brief         0x2E WriteDataByIdentifier - applicationSoftwareFingerprint handler
*                  (DID 0xF184)
*   \details       Response is just the echoed DID (data_p[0..1] untouched) - no payload beyond
*                  that, per WriteDataByIdentifier's format. *len_p on entry is however many bytes
*                  of fingerprint data the tester actually sent; anything over
*                  FBL_FINGERPRINT_MAX_LEN is rejected rather than silently truncated. Flushed
*                  synchronously (NVM_GEN2_flush_block(), not left for a periodic tick FBL doesn't
*                  run) so the write is durable on flash before the positive response goes out -
*                  same reasoning as APP's uds_handle_write_key() (APP/Src/UDS_CFG/UDS_config.c).
***************************************************************************************************/
STATIC u8_t fbl_uds_handle_write_fingerprint( u16_t subfunc, u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u16_t fingerprint_len = *len_p;

    (void)subfunc;

    if( fingerprint_len > FBL_FINGERPRINT_MAX_LEN )
    {
        *nrc_p = UDS_RC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT;
    }
    else
    {
        fbl_fingerprint_g.len = (u8_t)fingerprint_len;
        STDC_memcpy( fbl_fingerprint_g.data, data_p, (u8_t)fingerprint_len );
        fbl_fingerprint_g.flash_count++;
        fbl_fingerprint_g.last_flash_timestamp_ms         = TIME_get_cumulative_run_time_ms_u32();
        fbl_fingerprint_g.boot_count_at_flash             = fbl_boot_count_g.count;
        fbl_fingerprint_g.download_attempt_count_at_flash = fbl_download_attempt_count_g.count;
        NVM_GEN2_flush_block( FBL_FINGERPRINT_BLOCK_ID );

        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }

    return( 0u );
}

/****************************** END OF FILE *******************************************************/
