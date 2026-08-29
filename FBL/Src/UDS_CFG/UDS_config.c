/*! \file
*               Author: mstewart
*   \brief      FBL's UDS service table implementation - see UDS_config.h for scope
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "FBL.h"
#include "HEADER.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_security_send_key( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_routine_erase_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_routine_check_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_request_download( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_transfer_data( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u8_t uds_handle_request_transfer_exit( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p );
STATIC u32_t uds_read_big_endian( const u8_t* data_p, u8_t num_bytes );

/***************************************************************************************************
**                              Sub-function Tables                                               **
***************************************************************************************************/
/* All four levels share the one placeholder algorithm - see FBL.h's comment on why this is
   "four doors to one room, not graded access", not a real level system. */
STATIC UDS_subfunction_table_st security_access_subfuncs_s[] =
{
    { FBL_SECURITY_LEVEL_1_SEED, uds_handle_security_request_seed, UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_1_KEY,  uds_handle_security_send_key,     UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_2_SEED, uds_handle_security_request_seed, UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_2_KEY,  uds_handle_security_send_key,     UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_3_SEED, uds_handle_security_request_seed, UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_3_KEY,  uds_handle_security_send_key,     UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_4_SEED, uds_handle_security_request_seed, UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_4_KEY,  uds_handle_security_send_key,     UDS_SES_PROGRAMMING, 0u },
};

STATIC UDS_subfunction_table_st routine_control_subfuncs_s[] =
{
    { ROUTINE_ID_ERASE_MEMORY, uds_handle_routine_erase_memory, UDS_SES_PROGRAMMING, 1u },
    { ROUTINE_ID_CHECK_MEMORY, uds_handle_routine_check_memory, UDS_SES_PROGRAMMING, 0u },
};

STATIC UDS_subfunction_table_st request_download_subfuncs_s[] =
{
    { 0x0000u, uds_handle_request_download, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st transfer_data_subfuncs_s[] =
{
    { 0x0000u, uds_handle_transfer_data, UDS_SES_PROGRAMMING, 1u },
};

STATIC UDS_subfunction_table_st request_transfer_exit_subfuncs_s[] =
{
    { 0x0000u, uds_handle_request_transfer_exit, UDS_SES_PROGRAMMING, 1u },
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

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*   \brief         0x27 0x01 - SecurityAccess RequestSeed
*   \details       Writes a 4-byte big-endian seed into the response buffer.
***************************************************************************************************/
STATIC u8_t uds_handle_security_request_seed( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u32_t seed;

    (void)len_p;

    if( FBL_security_is_locked_out() == TRUE )
    {
        *nrc_p = UDS_RC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
    }
    else
    {
        FBL_security_generate_seed( &seed );

        data_p[0] = (u8_t)( seed >> 24u );
        data_p[1] = (u8_t)( seed >> 16u );
        data_p[2] = (u8_t)( seed >> 8u );
        data_p[3] = (u8_t)( seed );
        *len_p    = 4u;
        *nrc_p    = UDS_RC_POSITIVE_RESPONSE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x27 0x02 - SecurityAccess SendKey
*   \details       On success, grants UDS security level 1 so 0x34/0x36/0x37/EraseMemory unlock.
***************************************************************************************************/
STATIC u8_t uds_handle_security_send_key( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u32_t key = uds_read_big_endian( data_p, 4u );

    if( FBL_security_verify_key( key ) == TRUE )
    {
        UDS_set_security_level( 1u );
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else if( FBL_security_is_locked_out() == TRUE )
    {
        *nrc_p = UDS_RC_EXCEEDED_NUMBER_OF_ATTEMPTS;
    }
    else
    {
        *nrc_p = UDS_RC_INVALID_KEY;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x31 StartRoutine 0xFF00 - EraseMemory
*   \details       Erases the entire APP flash region. Must precede 0x34 RequestDownload.
*
*                  Answered asynchronously: erasing ~99 pages takes seconds, so this starts the run
*                  and defers the response rather than blocking the tick loop for the duration.
*                  FBL_tick() erases one page per tick and calls uds_erase_complete() below when it
*                  finishes, which sends the real answer. UDS emits 0x78 ResponsePending meanwhile
*                  and discards anything the tester sends until then - see UDS_defer_response().
***************************************************************************************************/
STATIC u8_t uds_handle_routine_erase_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    if( FBL_flash_erase_begin() == TRUE )
    {
        UDS_defer_response();
    }
    else
    {
        *len_p = 0u;
        *nrc_p = UDS_RC_GENERAL_PROGRAMMING_FAILURE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         End of the erase run started above - wired to fbl_config_st.erase_complete_func_p
*   \details       Sends the 0x31 answer that uds_handle_routine_erase_memory() deferred. The
*                  payload is the routine echo the tester expects back (routineControlType then the
*                  2-byte routine identifier), supplied explicitly because the interim 0x78 frames
*                  have long since overwritten the request bytes in the shared buffer.
***************************************************************************************************/
void UDS_erase_complete_notify( false_true_et success )
{
    STATIC const u8_t erase_echo_s[3] =
    {
        0x01u,                                      /* routineControlType: startRoutine   */
        (u8_t)( ROUTINE_ID_ERASE_MEMORY >> 8u ),
        (u8_t)( ROUTINE_ID_ERASE_MEMORY & 0xFFu ),
    };

    if( success == TRUE )
    {
        UDS_send_deferred_response( erase_echo_s, (u16_t)sizeof( erase_echo_s ), UDS_RC_POSITIVE_RESPONSE );
    }
    else
    {
        UDS_send_deferred_response( NULL_P, 0u, UDS_RC_GENERAL_PROGRAMMING_FAILURE );
    }
}

/*!
****************************************************************************************************
*   \brief         0x31 StartRoutine 0xFF02 - CheckMemory
*   \details       Response: [routineStatusRecord: 4-byte big-endian CRC32 over the APP code
*                  region]. Lets the tester verify a flashed image without a separate 0x23
*                  ReadMemory pass. Region is [app_code_start, app_code_end] - header excluded -
*                  to match exactly what BM_app_calculate_crc32() validates against on the next
*                  reset (the header holds the CRC value itself, so including it would make this
*                  unsatisfiable by construction); app_code_start is always app_header_address +
*                  sizeof(APP_header_st), see HEADER.h.
***************************************************************************************************/
STATIC u8_t uds_handle_routine_check_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    const fbl_config_st* cfg_p       = FBL_get_config();
    u32_t                code_start  = cfg_p->app_header_address + (u32_t)sizeof( APP_header_st );
    u32_t                crc;

    crc = FBL_crc_calculate( code_start,
                              FBL_region_length( code_start, cfg_p->app_code_end_address ) );

    data_p[0] = (u8_t)( crc >> 24u );
    data_p[1] = (u8_t)( crc >> 16u );
    data_p[2] = (u8_t)( crc >> 8u );
    data_p[3] = (u8_t)( crc );
    *len_p    = 4u;
    *nrc_p    = UDS_RC_POSITIVE_RESPONSE;

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x34 RequestDownload
*   \details       Request: [dataFormatIdentifier(1)][addrLenFmtId(1)][address(N)][size(M)]
*                  Response: [lengthFormatIdentifier(1)][maxNumberOfBlockLength(4)]
***************************************************************************************************/
STATIC u8_t uds_handle_request_download( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    const fbl_config_st* cfg_p        = FBL_get_config();
    u8_t                 addr_len_fmt = data_p[1];
    u8_t                 addr_len     = (u8_t)( addr_len_fmt & 0x0Fu );
    u8_t                 size_len     = (u8_t)( ( addr_len_fmt >> 4u ) & 0x0Fu );
    u32_t                address;
    u32_t                length;
    false_true_et        security_denied;

    if( ( addr_len == 0u ) || ( addr_len > 4u ) || ( size_len == 0u ) || ( size_len > 4u ) )
    {
        *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
    }
    else
    {
        address = uds_read_big_endian( &data_p[2], addr_len );
        length  = uds_read_big_endian( &data_p[2u + addr_len], size_len );

        if( FBL_download_request( address, length, &security_denied ) == TRUE )
        {
            data_p[0] = 0x40u;  /* lengthFormatIdentifier: 4-byte maxNumberOfBlockLength follows */
            data_p[1] = (u8_t)( cfg_p->max_transfer_block_len >> 24u );
            data_p[2] = (u8_t)( cfg_p->max_transfer_block_len >> 16u );
            data_p[3] = (u8_t)( cfg_p->max_transfer_block_len >> 8u );
            data_p[4] = (u8_t)( cfg_p->max_transfer_block_len );
            *len_p    = 5u;
            *nrc_p    = UDS_RC_POSITIVE_RESPONSE;
        }
        else if( security_denied == TRUE )
        {
            *nrc_p = UDS_RC_SECURITY_ACCESS_DENIED;
        }
        else
        {
            *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
        }
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x36 TransferData
*   \details       Request: [blockSequenceCounter(1)][data...]. Response echoes the sequence
*                  counter byte, which is already sitting at data_p[0] and left untouched.
***************************************************************************************************/
STATIC u8_t uds_handle_transfer_data( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    u8_t                    sequence     = data_p[0];
    u16_t                   payload_len  = (u16_t)( *len_p - 1u );
    false_true_et           ok;
    FBL_transfer_status_et  status;

    ok = FBL_download_transfer_data( sequence, &data_p[1], payload_len );

    if( ok == TRUE )
    {
        *len_p = 1u;  /* echo blockSequenceCounter only, already in place at data_p[0] */
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else
    {
        status = FBL_download_get_last_transfer_status();

        switch( status )
        {
            case FBL_TRANSFER_STATUS_SEQUENCE:
                *nrc_p = UDS_RC_WRONG_BLOCK_SEQUENCE_COUNTER;
            break;

            case FBL_TRANSFER_STATUS_RANGE:
                *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
            break;

            case FBL_TRANSFER_STATUS_NOT_ACTIVE:
                *nrc_p = UDS_RC_REQUEST_SEQUENCE_ERROR;
            break;

            default:
                *nrc_p = UDS_RC_GENERAL_PROGRAMMING_FAILURE;
            break;
        }
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x37 RequestTransferExit
*   \details       No request data, no response data beyond the positive response SID.
***************************************************************************************************/
STATIC u8_t uds_handle_request_transfer_exit( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    false_true_et flush_failed;

    (void)data_p;

    if( FBL_download_exit( &flush_failed ) == TRUE )
    {
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else if( flush_failed == TRUE )
    {
        *nrc_p = UDS_RC_GENERAL_PROGRAMMING_FAILURE;
    }
    else
    {
        *nrc_p = UDS_RC_REQUEST_SEQUENCE_ERROR;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         Parse a big-endian unsigned value of 1-4 bytes
***************************************************************************************************/
STATIC u32_t uds_read_big_endian( const u8_t* data_p, u8_t num_bytes )
{
    u32_t value = 0u;
    u8_t  i;

    for( i = 0u; i < num_bytes; i++ )
    {
        value = ( value << 8u ) | data_p[i];
    }

    return( value );
}

/****************************** END OF FILE *******************************************************/
