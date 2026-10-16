#ifndef SENSOR_NET_H
#define SENSOR_NET_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define SENSOR_NET_MAX_SENSORS              ( 12u )

/* Sensor frames are variable length. A frame is decoded from SENSOR_NET_BASE_FRAME_LEN bytes, a field
   after that is only read if the frame is long enough to hold it, and bytes past the last known field
   are ignored - so older and newer sensors both work. See SENSOR_NET.c. */
#define SENSOR_NET_BASE_FRAME_LEN           ( 20u )     /* Sensor id up to and including the battery flags */
#define SENSOR_NET_VERSION_FRAME_LEN        ( 25u )     /* Adds the SW version, HW version and manufacture date */

#define SENSOR_NET_COMMS_LOST_TIMEOUT_SECS  ( 30u * 60u )

/* Battery state flag bit positions within SENSOR_NET_sensor_data_st.battery_flags */
#define SENSOR_NET_BAT_FLAG_LOW_BIT         ( 0u )
#define SENSOR_NET_BAT_FLAG_CRITICAL_BIT    ( 1u )
#define SENSOR_NET_BAT_FLAG_LATCHED_LOW_BIT ( 2u )

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef struct
{
    u32_t         sensor_id;
    u8_t          sensor_type;
    u8_t          operating_mode;
    u8_t          sensor_location;
    s16_t         temperature_centidegC;
    s16_t         humidity_tenths_pct;
    u16_t         battery_voltage_mv;
    u16_t         wakeup_interval_sec;
    u32_t         runtime_sec;
    u8_t          battery_flags;
    u8_t          sw_major;                 /*!< Valid only if version_valid is TRUE, else 0       */
    u8_t          sw_minor;
    u8_t          sw_patch;
    u8_t          hw_version;
    u8_t          mfg_year;                 /*!< Years since 2000                                  */
    u8_t          mfg_month;
    u8_t          mfg_day;
    false_true_et version_valid;            /*!< The last frame was long enough to carry the version fields */
    u8_t          last_frame_len;           /*!< Length of the last frame received from this sensor */
    u64_t         last_rx_time_ms;
    u32_t         rx_frame_count;
    false_true_et comms_lost;
    false_true_et tb_publish_pending;
    false_true_et valid;
} SENSOR_NET_sensor_data_st;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void                       SENSOR_NET_init( void );
void                       SENSOR_NET_tick( void );
SENSOR_NET_sensor_data_st* SENSOR_NET_get_sensor_db( void );

#endif /* SENSOR_NET_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
