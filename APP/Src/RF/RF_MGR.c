/*! \file
*               Author: mstewart
*   \brief      RF_MGR module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "RF_MGR.h"
#include "HAL_BRD.h"
#include "MSG_SCHED.h"

/* Module Identification for assert functionality */
#define STDC_MODULE_ID STDC_RF_MGR

extern NRF24_instance_st nrf24_instance_s;

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC RF_MGR_data_st        rf_mgr_data_s;
STATIC RF_MGR_rf_state_et    rf_mgr_state_s;
STATIC RF_MGR_cfg_st         rf_mgr_cfg_s;
STATIC u8_t                  rf_mgr_chan_freq_s[7u] = { 0, 20, 40, 60, 80, 100, 120 };
STATIC RF_MGR_sensor_data_st rf_mgr_sensor_db_s[RF_MGR_MAX_SENSORS];

/* Byte offsets within the sensor RF frame */
#define RF_FRAME_OFFSET_SENSOR_ID       ( 0u )
#define RF_FRAME_OFFSET_SENSOR_TYPE     ( 4u )
#define RF_FRAME_OFFSET_OPER_MODE       ( 5u )
#define RF_FRAME_OFFSET_LOCATION        ( 6u )
#define RF_FRAME_OFFSET_TEMPERATURE     ( 7u )
#define RF_FRAME_OFFSET_HUMIDITY        ( 9u )
#define RF_FRAME_OFFSET_BATT_VOLTAGE    ( 11u )
#define RF_FRAME_OFFSET_WAKEUP_INTERVAL ( 13u )
#define RF_FRAME_OFFSET_RUNTIME         ( 15u )
#define RF_FRAME_OFFSET_BATT_FLAGS      ( 19u )

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Module (re-)initialisation function
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void RF_MGR_init( RF_MGR_cfg_st cfg )
{
    rf_mgr_cfg_s   = cfg;
    rf_mgr_state_s = RF_MGR_APPLY_CFG;
    STDC_memset( &rf_mgr_data_s,      0x00, sizeof( rf_mgr_data_s ) );
    STDC_memset( &rf_mgr_sensor_db_s, 0x00, sizeof( rf_mgr_sensor_db_s ) );
}

/*!
****************************************************************************************************
*
*   \brief         RF Module tick
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void RF_MGR_tick( void )
{
    if( rf_mgr_data_s.frame_pending == TRUE )
    {
        rf_mgr_data_s.frame_pending = FALSE;
        rf_mgr_analyse_received_frame( rf_mgr_data_s.rx_payload );
    }

    rf_mgr_check_comms_lost();

    switch( rf_mgr_state_s )
    {
        case RF_MGR_IDLE:
        {
        }
        break;

        case RF_MGR_APPLY_CFG:
        {
            NRF24_apply_config( &nrf24_instance_s, NRF24_HUB_CONFIG );

            if( NRF24_self_check( &nrf24_instance_s ) != PASS )
            {
                rf_mgr_set_state( RF_MGR_FAULT );
            }
            else if( rf_mgr_cfg_s.mode == RF_MGR_MODE_TX )
            {
                rf_mgr_set_state( RF_MGR_SETUP_TX );
            }
            else
            {
                rf_mgr_set_state( RF_MGR_SETUP_RX );
            }
        }
        break;

        case RF_MGR_SETUP_TX:
        {
            rf_mgr_configure_transmitt_mode( rf_mgr_cfg_s.channel );
            rf_mgr_set_state( RF_MGR_TX );
        }

        /* Intentional fallthrough */
        case RF_MGR_TX:
        {
            /* Setup the payload */
            rf_mgr_setup_tx_payload();
            rf_mgr_send_payload();

            rf_mgr_set_state( RF_MGR_WAIT_FOR_TX_COMPLETE );
        }

        /* Intentional fallthrough */
        case RF_MGR_WAIT_FOR_TX_COMPLETE:
        {
            /* A callback will be triggered when the frame is either sent sucesfully or failed */
        }
        break;

        case RF_MGR_SETUP_RX:
        {
            rf_mgr_configure_receive_mode();
            rf_mgr_set_state( RF_MGR_RX );
        }

        /* Intentional fallthrough */
        case RF_MGR_RX:
        {
        }
        break;

        case RF_MGR_RX_TEST_MODE:
        {
        }
        break;

        case RF_MGR_POWER_DOWN:
        {
            /* Power down the RF chip by setting the low level mode of operation to power down */
            rf_mgr_power_down_chip();

            /* Now go to IDLE mode */
            rf_mgr_set_state( RF_MGR_IDLE );
        }  
        break;

        case RF_MGR_SETUP_CONST_WAVE:
        {
           rf_mgr_configure_transmitt_mode( 0u );

            /* Move onto the NRF24_CONST_WAVE state */
            rf_mgr_set_state( RF_MGR_CONST_WAVE );
        }

        /* Intentional fallthrough */
        case RF_MGR_CONST_WAVE:
        {
        }
        break;

        case RF_MGR_FAULT:
            rf_mgr_set_state( RF_MGR_RESET );
        break;

        case RF_MGR_RESET:
        {
            rf_mgr_set_state( RF_MGR_IDLE );
        }
        break;

        default:
        {
            DBG_MGR_LOG_MSG( "Default Case" );
        }
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Returns the state of the RF MGR
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
RF_MGR_rf_state_et RF_MGR_get_state( void )
{
    return( rf_mgr_state_s );
}

/*!
****************************************************************************************************
*
*   \brief         Callback to signal that the last RF frame has either been sent succesfully or it failed
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void RF_MGR_tx_complete( pass_fail_et status )
{
	if( status != PASS )
	{
		rf_mgr_data_s.tx_timeouts++;
	}

    /* Release the state machine again to run */
    rf_mgr_inc_tx_packet_ctr();
    rf_mgr_set_state( RF_MGR_TX );
}

/*!
****************************************************************************************************
*
*   \brief         Callback to signal that the last RF frame has either been sent succesfully or it failed
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void RF_MGR_get_rx_frame( void )
{
    rf_mgr_data_s.rx_packet_num++;
    rf_mgr_data_s.frame_pending = TRUE;

    NRF24_read_rx_payload( &nrf24_instance_s, rf_mgr_data_s.rx_payload );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Sets the state of the RF MGR
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_set_state( RF_MGR_rf_state_et state )
{
    rf_mgr_state_s = state;
}

/*!
****************************************************************************************************
*
*   \brief         This powers down the rf chip
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_power_down_chip( void )
{
    NRF24_power_down( &nrf24_instance_s );
}

/*!
****************************************************************************************************
*
*   \brief         Configure the RF chip for tranmitter mode
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_configure_transmitt_mode( u8_t channel )
{
    NRF24_configure_transmit_mode( &nrf24_instance_s );

    NRF24_set_channel( &nrf24_instance_s, rf_mgr_chan_freq_s[channel] );
}

/*!
****************************************************************************************************
*
*   \brief         Configure the RF chip for receiver mode
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_configure_receive_mode( void )
{
    NRF24_configure_receive_mode( &nrf24_instance_s );
}

/*!
****************************************************************************************************
*
*   \brief         This sets up the tx payload
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_setup_tx_payload( void )
{
    STDC_memset( rf_mgr_data_s.tx_payload, 0xAA, sizeof( rf_mgr_data_s.tx_payload ) );

    /* RF MGR handles the packet number */
    rf_mgr_data_s.tx_payload[0u] = rf_mgr_data_s.tx_packet_num;

    /* Calculate checksum and place at byte 31 */
    rf_mgr_data_s.tx_payload[31u] = CHKSUM_calc_byte_wise_checksum( rf_mgr_data_s.tx_payload, RF_MGR_MAX_PAYLOAD_SIZE - 1u );
} 

/*!
****************************************************************************************************
*
*   \brief         This sets up the tx payload
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_send_payload( void )
{
    NRF24_send_payload( &nrf24_instance_s, rf_mgr_data_s.tx_payload, RF_MGR_MAX_PAYLOAD_SIZE );
} 

/*!
****************************************************************************************************
*
*   \brief         Increments the tx packet ctr
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_inc_tx_packet_ctr( void )
{
    if( rf_mgr_data_s.tx_packet_num < U32_T_MAX )
    {
        rf_mgr_data_s.tx_packet_num += 1u;
    }
    else
    {
        rf_mgr_data_s.tx_packet_num = 0u;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Analyse the received RF packets
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
void rf_mgr_analyse_received_frame( u8_t* data_p )
{
    pass_fail_et crc_result = PASS;

#ifndef RF_MGR_DISABLE_CRC_CHECK
    crc_result = rf_mgr_analyse_packet_crc( data_p, RF_MGR_MAX_PAYLOAD_SIZE - 1u );
#endif

    if( crc_result == PASS )
    {
        rf_mgr_decode_sensor_frame( data_p );
    }
    else
    {
        DBG_MGR_LOG_MSG( "Wrong CRC" );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Analyse the received RF packets crc
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
pass_fail_et rf_mgr_analyse_packet_crc( u8_t* data_p, u8_t crc_pos )
{
    pass_fail_et status = FAIL;
    u8_t expected_crc;
    u8_t actual_crc;

    /* First check that the CRC is valid */
    expected_crc = data_p[crc_pos];
    actual_crc = CHKSUM_calc_byte_wise_checksum( data_p, RF_MGR_MAX_PAYLOAD_SIZE - 1u );

    if( expected_crc == actual_crc )
    {
        status = PASS;
    }

    return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Returns a pointer to the sensor database array
*
*   \author        MS
*
*   \return        Pointer to RF_MGR_sensor_data_st[RF_MGR_MAX_SENSORS]
*
***************************************************************************************************/
RF_MGR_sensor_data_st* RF_MGR_get_sensor_db( void )
{
    return( rf_mgr_sensor_db_s );
}

/*!
****************************************************************************************************
*
*   \brief         Decodes a validated sensor RF frame into the sensor database
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void rf_mgr_decode_sensor_frame( u8_t* data_p )
{
    u8_t                   i;
    u8_t                   free_slot = RF_MGR_MAX_SENSORS;
    u32_t                  sensor_id = STDC_copy_buffer_msb_first_to_32bit( &data_p[RF_FRAME_OFFSET_SENSOR_ID] );
    RF_MGR_sensor_data_st* entry_p   = NULL_P;

    for( i = 0u; i < RF_MGR_MAX_SENSORS; i++ )
    {
        if( ( rf_mgr_sensor_db_s[i].valid == TRUE ) && ( rf_mgr_sensor_db_s[i].sensor_id == sensor_id ) )
        {
            entry_p = &rf_mgr_sensor_db_s[i];
        }

        if( ( rf_mgr_sensor_db_s[i].valid == FALSE ) && ( free_slot == RF_MGR_MAX_SENSORS ) )
        {
            free_slot = i;
        }
    }

    if( ( entry_p == NULL_P ) && ( free_slot < RF_MGR_MAX_SENSORS ) )
    {
        entry_p = &rf_mgr_sensor_db_s[free_slot];
    }

    if( entry_p != NULL_P )
    {
        entry_p->sensor_id              = sensor_id;
        entry_p->sensor_type            = data_p[RF_FRAME_OFFSET_SENSOR_TYPE];
        entry_p->operating_mode         = data_p[RF_FRAME_OFFSET_OPER_MODE];
        entry_p->sensor_location        = data_p[RF_FRAME_OFFSET_LOCATION];
        entry_p->temperature_centidegC  = (s16_t)STDC_copy_buffer_msb_first_to_16bit( &data_p[RF_FRAME_OFFSET_TEMPERATURE] );
        entry_p->humidity_tenths_pct    = (s16_t)STDC_copy_buffer_msb_first_to_16bit( &data_p[RF_FRAME_OFFSET_HUMIDITY] );
        entry_p->battery_voltage_mv     = STDC_copy_buffer_msb_first_to_16bit( &data_p[RF_FRAME_OFFSET_BATT_VOLTAGE] );
        entry_p->wakeup_interval_sec    = STDC_copy_buffer_msb_first_to_16bit( &data_p[RF_FRAME_OFFSET_WAKEUP_INTERVAL] );
        entry_p->runtime_sec            = STDC_copy_buffer_msb_first_to_32bit( &data_p[RF_FRAME_OFFSET_RUNTIME] );
        entry_p->battery_flags          = data_p[RF_FRAME_OFFSET_BATT_FLAGS];
        entry_p->last_rx_time_ms        = TIME_get_cumulative_run_time_ms();
        entry_p->rx_frame_count         += 1u;
        entry_p->comms_lost             = FALSE;
        entry_p->tb_publish_pending     = TRUE;
        entry_p->valid                  = TRUE;
        MSG_SCHED_send_now( (u8_t)( entry_p - rf_mgr_sensor_db_s ) );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Checks all known sensors for lost communication
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void rf_mgr_check_comms_lost( void )
{
    u8_t i;

    for( i = 0u; i < RF_MGR_MAX_SENSORS; i++ )
    {
        if( rf_mgr_sensor_db_s[i].valid == TRUE )
        {
            if( TIME_has_time_elapsed_ms( rf_mgr_sensor_db_s[i].last_rx_time_ms, RF_MGR_COMMS_LOST_TIMEOUT_SECS * MSECS_PER_SEC ) == TRUE )
            {
                rf_mgr_sensor_db_s[i].comms_lost = TRUE;
            }
        }
    }
}

/****************************** END OF FILE *******************************************************/
