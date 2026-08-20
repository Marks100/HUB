/*! \file
*               Author: mstewart
*   \brief      FBL's UDS service table implementation - see UDS_config.h for scope
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "UDS_config.h"
#include "FBL.h"

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
STATIC UDS_subfunction_table_st security_access_subfuncs_s[] =
{
    { FBL_SECURITY_LEVEL_1_SEED, uds_handle_security_request_seed, UDS_SES_PROGRAMMING, 0u },
    { FBL_SECURITY_LEVEL_1_KEY,  uds_handle_security_send_key,     UDS_SES_PROGRAMMING, 0u },
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
    { UDS_SID_SECURITY_ACCESS,   security_access_subfuncs_s,        2u },
    { UDS_SID_ROUTINE_CONTROL,   routine_control_subfuncs_s,        2u },
    { UDS_SID_REQUEST_DOWNLOAD,  request_download_subfuncs_s,       1u },
    { UDS_SID_TRANSFER_DATA,     transfer_data_subfuncs_s,          1u },
    { UDS_SID_REQUEST_TRANSFER_EXIT, request_transfer_exit_subfuncs_s, 1u },
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
***************************************************************************************************/
STATIC u8_t uds_handle_routine_erase_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    (void)data_p;

    if( FBL_flash_erase_application() == TRUE )
    {
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
    }
    else
    {
        *nrc_p = UDS_RC_GENERAL_PROGRAMMING_FAILURE;
    }

    return( 0u );
}

/*!
****************************************************************************************************
*   \brief         0x31 StartRoutine 0xFF02 - CheckMemory
*   \details       Response: [routineStatusRecord: 4-byte big-endian CRC32 over the APP region].
*                  Lets the tester verify a flashed image without a separate 0x23 ReadMemory pass.
***************************************************************************************************/
STATIC u8_t uds_handle_routine_check_memory( u8_t* data_p, u16_t* len_p, UDS_response_code_et* nrc_p )
{
    const fbl_config_st* cfg_p = FBL_get_config();
    u32_t                crc;

    crc = FBL_crc_calculate( cfg_p->app_start_address, ( cfg_p->app_end_address - cfg_p->app_start_address ) );

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
    const fbl_config_st* cfg_p       = FBL_get_config();
    u8_t                 addr_len_fmt = data_p[1];
    u8_t                 addr_len    = (u8_t)( addr_len_fmt & 0x0Fu );
    u8_t                 size_len    = (u8_t)( ( addr_len_fmt >> 4u ) & 0x0Fu );
    u32_t                address;
    u32_t                length;

    if( ( addr_len == 0u ) || ( addr_len > 4u ) || ( size_len == 0u ) || ( size_len > 4u ) )
    {
        *nrc_p = UDS_RC_REQUEST_OUT_OF_RANGE;
    }
    else
    {
        address = uds_read_big_endian( &data_p[2], addr_len );
        length  = uds_read_big_endian( &data_p[2u + addr_len], size_len );

        if( FBL_download_request( address, length ) == TRUE )
        {
            data_p[0] = 0x40u;  /* lengthFormatIdentifier: 4-byte maxNumberOfBlockLength follows */
            data_p[1] = (u8_t)( cfg_p->max_transfer_block_len >> 24u );
            data_p[2] = (u8_t)( cfg_p->max_transfer_block_len >> 16u );
            data_p[3] = (u8_t)( cfg_p->max_transfer_block_len >> 8u );
            data_p[4] = (u8_t)( cfg_p->max_transfer_block_len );
            *len_p    = 5u;
            *nrc_p    = UDS_RC_POSITIVE_RESPONSE;
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
    (void)data_p;

    if( FBL_download_exit() == TRUE )
    {
        *len_p = 0u;
        *nrc_p = UDS_RC_POSITIVE_RESPONSE;
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
