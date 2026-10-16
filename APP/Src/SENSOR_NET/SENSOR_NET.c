/*! \file
*               Author: mstewart
*   \brief      SENSOR_NET - wireless sensor frames decoded into a sensor database
*
*   \details    The application half of the sensor radio link. RF_MGR (common module) owns the radio
*               and hands over raw frames of whatever length the sensor sent, this module knows what
*               is inside them: it decodes a frame into the slot for that sensor, flags a sensor whose
*               frames stop arriving, and tells MSG_SCHED a new reading is ready to send.
*
*               Frame layout (big-endian), as sent by the sensor firmware's RF_MGR:
*
*                 offset  size  field
*                 0       4     sensor id
*                 4       1     sensor type
*                 5       1     operating mode
*                 6       1     location
*                 7       2     temperature (centidegC, s16)
*                 9       2     humidity (tenths of %, s16)
*                 11      2     battery voltage (mV)
*                 13      2     wakeup period (s)
*                 15      4     runtime counter
*                 19      1     battery flags
*                 20      1     SW major [7:4] | SW minor [3:0]       optional, with 21 to 24
*                 21      1     SW patch [7:4] | HW version [3:0]
*                 22      3     manufacture date (YY since 2000, MM, DD)
*
*               Length handling: the frame length is whatever the sensor sent, nothing here assumes a
*               fixed size. A frame shorter than SENSOR_NET_BASE_FRAME_LEN is dropped, it cannot hold
*               a reading. The version fields are decoded only when the frame reaches
*               SENSOR_NET_VERSION_FRAME_LEN, otherwise they are cleared and version_valid is FALSE.
*               Bytes after the last field known here are ignored, so a newer sensor that adds
*               fields still works. A field is only ever read from inside the received length.
*
*               SENSOR_NET_tick() must run after RF_MGR_tick() in the schedule, so a frame that
*               arrived this tick is decoded on the same tick.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "SENSOR_NET.h"
#include "RF_MGR.h"
#include "DBG_MGR.h"
#include "TIME_MGR.h"
#include "MSG_SCHED.h"

/* Module Identification for assert functionality */
#define STDC_MODULE_ID STDC_RF_MGR

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
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
#define RF_FRAME_OFFSET_VERSION         ( 20u )
#define RF_FRAME_OFFSET_MFG_DATE        ( 22u )

#define RF_FRAME_VERSION_NIBBLE_MASK    ( 0x0Fu )
#define RF_FRAME_VERSION_UPPER_SHIFT    ( 4u )

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void sensor_net_decode_sensor_frame( u8_t* data_p, u8_t len );
STATIC void sensor_net_decode_version_fields( SENSOR_NET_sensor_data_st* entry_p, u8_t* data_p, u8_t len );
STATIC void sensor_net_check_comms_lost( void );

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC SENSOR_NET_sensor_data_st sensor_net_db_s[SENSOR_NET_MAX_SENSORS];

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
*   \note          Clears the sensor database.
*
***************************************************************************************************/
void SENSOR_NET_init( void )
{
    STDC_memset( &sensor_net_db_s, 0x00, sizeof( sensor_net_db_s ) );
}

/*!
****************************************************************************************************
*
*   \brief         Sensor network tick
*
*   \author        MS
*
*   \return        none
*
*   \note          Collects the frame RF_MGR holds, if there is a new one, and decodes it whatever its
*                  length. Then marks any sensor that has not been heard from for
*                  SENSOR_NET_COMMS_LOST_TIMEOUT_SECS.
*
***************************************************************************************************/
void SENSOR_NET_tick( void )
{
    u8_t frame[RF_MGR_MAX_FRAME_SIZE];
    u8_t len = 0u;

    if( RF_MGR_receive_frame( frame, &len ) == TRUE )
    {
        sensor_net_decode_sensor_frame( frame, len );
    }

    sensor_net_check_comms_lost();
}

/*!
****************************************************************************************************
*
*   \brief         Returns a pointer to the sensor database array
*
*   \author        MS
*
*   \return        Pointer to SENSOR_NET_sensor_data_st[SENSOR_NET_MAX_SENSORS]
*
***************************************************************************************************/
SENSOR_NET_sensor_data_st* SENSOR_NET_get_sensor_db( void )
{
    return( sensor_net_db_s );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Decodes a received sensor RF frame into the sensor database
*
*   \author        MS
*
*   \param[in]     data_p - The received frame
*   \param[in]     len    - Its length in bytes, whatever the sensor sent
*
*   \return        none
*
*   \note          A frame shorter than SENSOR_NET_BASE_FRAME_LEN is dropped. A known sensor id
*                  updates its slot, an unknown one takes the first free slot, and a frame from a new
*                  sensor is dropped if the database is full.
*
***************************************************************************************************/
STATIC void sensor_net_decode_sensor_frame( u8_t* data_p, u8_t len )
{
    u8_t                       i;
    u8_t                       free_slot = SENSOR_NET_MAX_SENSORS;
    u32_t                      sensor_id;
    SENSOR_NET_sensor_data_st* entry_p   = NULL_P;

    if( len < SENSOR_NET_BASE_FRAME_LEN )
    {
        DBG_MGR_LOG_MSG( "Short sensor frame" );
    }
    else
    {
        sensor_id = STDC_copy_buffer_msb_first_to_32bit( &data_p[RF_FRAME_OFFSET_SENSOR_ID] );

        for( i = 0u; i < SENSOR_NET_MAX_SENSORS; i++ )
        {
            if( ( sensor_net_db_s[i].valid == TRUE ) && ( sensor_net_db_s[i].sensor_id == sensor_id ) )
            {
                entry_p = &sensor_net_db_s[i];
            }

            if( ( sensor_net_db_s[i].valid == FALSE ) && ( free_slot == SENSOR_NET_MAX_SENSORS ) )
            {
                free_slot = i;
            }
        }

        if( ( entry_p == NULL_P ) && ( free_slot < SENSOR_NET_MAX_SENSORS ) )
        {
            entry_p = &sensor_net_db_s[free_slot];
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
            entry_p->last_frame_len         = len;
            entry_p->last_rx_time_ms        = TIME_get_cumulative_run_time_ms_64();
            entry_p->rx_frame_count         += 1u;
            entry_p->comms_lost             = FALSE;
            entry_p->tb_publish_pending     = TRUE;
            entry_p->valid                  = TRUE;

            sensor_net_decode_version_fields( entry_p, data_p, len );

            MSG_SCHED_send_now( (u8_t)( entry_p - sensor_net_db_s ) );
        }
    }
}

/*!
****************************************************************************************************
*
*   \brief         Decodes the optional version and manufacture date fields, if the frame has them
*
*   \author        MS
*
*   \param[in,out] entry_p - The sensor's database slot
*   \param[in]     data_p  - The received frame
*   \param[in]     len     - Its length in bytes
*
*   \return        none
*
*   \note          The fields are only read when the frame reaches SENSOR_NET_VERSION_FRAME_LEN.
*                  Otherwise they are cleared, so a sensor that drops back to a shorter frame does not
*                  keep showing an old version.
*
***************************************************************************************************/
STATIC void sensor_net_decode_version_fields( SENSOR_NET_sensor_data_st* entry_p, u8_t* data_p, u8_t len )
{
    if( len >= SENSOR_NET_VERSION_FRAME_LEN )
    {
        u8_t major_minor = data_p[RF_FRAME_OFFSET_VERSION];
        u8_t patch_hw    = data_p[RF_FRAME_OFFSET_VERSION + 1u];

        entry_p->sw_major      = (u8_t)( major_minor >> RF_FRAME_VERSION_UPPER_SHIFT );
        entry_p->sw_minor      = (u8_t)( major_minor & RF_FRAME_VERSION_NIBBLE_MASK );
        entry_p->sw_patch      = (u8_t)( patch_hw >> RF_FRAME_VERSION_UPPER_SHIFT );
        entry_p->hw_version    = (u8_t)( patch_hw & RF_FRAME_VERSION_NIBBLE_MASK );
        entry_p->mfg_year      = data_p[RF_FRAME_OFFSET_MFG_DATE];
        entry_p->mfg_month     = data_p[RF_FRAME_OFFSET_MFG_DATE + 1u];
        entry_p->mfg_day       = data_p[RF_FRAME_OFFSET_MFG_DATE + 2u];
        entry_p->version_valid = TRUE;
    }
    else
    {
        entry_p->sw_major      = 0u;
        entry_p->sw_minor      = 0u;
        entry_p->sw_patch      = 0u;
        entry_p->hw_version    = 0u;
        entry_p->mfg_year      = 0u;
        entry_p->mfg_month     = 0u;
        entry_p->mfg_day       = 0u;
        entry_p->version_valid = FALSE;
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
STATIC void sensor_net_check_comms_lost( void )
{
    u8_t i;

    for( i = 0u; i < SENSOR_NET_MAX_SENSORS; i++ )
    {
        if( sensor_net_db_s[i].valid == TRUE )
        {
            if( TIME_has_time_elapsed_ms_64( sensor_net_db_s[i].last_rx_time_ms, SENSOR_NET_COMMS_LOST_TIMEOUT_SECS * MSECS_PER_SEC ) == TRUE )
            {
                sensor_net_db_s[i].comms_lost = TRUE;
            }
        }
    }
}

/****************************** END OF FILE *******************************************************/
