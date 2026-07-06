#ifndef RF_MGR_H
#define RF_MGR_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "MODE_MGR.h"
#include "CHKSUM.h"
#include "NRF24.h"
#include "CHKSUM.h"
#include "DBG_MGR.h"
#include "TIME_MGR.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define RF_MGR_TICK_RATE                ( MODE_MGR_TICK_RATE_MSECS )
#define RF_MGR_MAX_PAYLOAD_SIZE         ( NRF_MAX_PAYLOAD_SIZE )
#define RF_MGR_TX_TIMEOUT_MS            ( 30u/RF_MGR_TICK_RATE )
#define RF_MGR_RX_TIMEOUT_MS            ( 30u/RF_MGR_TICK_RATE )

#define RF_MGR_MAX_SENSORS              ( 12u )

#define RF_MGR_COMMS_LOST_TIMEOUT_SECS  ( 30u * 60u )

/* Uncomment to bypass the CRC check on received frames (debug only) */
#define RF_MGR_DISABLE_CRC_CHECK

/* Battery state flag bit positions within RF_MGR_sensor_data_st.battery_flags */
#define RF_MGR_BAT_FLAG_LOW_BIT         ( 0u )
#define RF_MGR_BAT_FLAG_CRITICAL_BIT    ( 1u )
#define RF_MGR_BAT_FLAG_LATCHED_LOW_BIT ( 2u )

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef enum
{
    RF_TX_IDLE = 0u,
    RF_TX_IN_PROGRESS,
    RF_MGR_RETRY_LIMIT_REACHED,
    RF_MGR_TX_SUCCESS,
} RF_MGR_tx_status_et;

typedef enum
{
	RF_MGR_MISSING_SENSOR = 0u
} RF_MGR_generic_dtc_et;

typedef enum
{
    RF_MGR_MODE_TX = 0u,
    RF_MGR_MODE_RX,
} RF_MGR_mode_et;

typedef struct
{
    RF_MGR_mode_et mode;
    u8_t           channel;
} RF_MGR_cfg_st;

typedef enum
{
    RF_MGR_IDLE = 0u,
    RF_MGR_APPLY_CFG,
    RF_MGR_SETUP_TX,
    RF_MGR_TX,
    RF_MGR_WAIT_FOR_TX_COMPLETE,
    RF_MGR_TX_TEST_MODE,
    RF_MGR_SETUP_RX,
    RF_MGR_RX,
    RF_MGR_RX_TEST_MODE,
    RF_MGR_POWER_DOWN,
    RF_MGR_SETUP_CONST_WAVE,
    RF_MGR_CONST_WAVE,
    RF_MGR_FAULT,
    RF_MGR_RESET
} RF_MGR_rf_state_et;

typedef struct
{
    u8_t   rx_payload[NRF_MAX_PAYLOAD_SIZE];
    u8_t   tx_payload[NRF_MAX_PAYLOAD_SIZE];
    u32_t  rx_packet_num;
    u32_t  tx_packet_num;
    u32_t  tx_timeouts;
    false_true_et frame_pending;
} RF_MGR_data_st;

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
    u64_t         last_rx_time_ms;
    u32_t         rx_frame_count;
    false_true_et comms_lost;
    false_true_et tb_publish_pending;
    false_true_et valid;
} RF_MGR_sensor_data_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void                   RF_MGR_init( const RF_MGR_cfg_st* cfg_p );
void                   RF_MGR_tick( void );
RF_MGR_rf_state_et     RF_MGR_get_state( void );
void                   RF_MGR_tx_complete( pass_fail_et state );
void                   RF_MGR_get_rx_frame( void );
RF_MGR_sensor_data_st* RF_MGR_get_sensor_db( void );

void                   rf_mgr_set_state( RF_MGR_rf_state_et state );
void                   rf_mgr_send_payload( void );
void                   rf_mgr_analyse_received_frame( u8_t* data_p );
void                   rf_mgr_decode_sensor_frame( u8_t* data_p );
void                   rf_mgr_configure_transmitt_mode( u8_t channel );
void                   rf_mgr_configure_receive_mode( void );
void                   rf_mgr_power_down_chip( void );
void                   rf_mgr_setup_tx_payload( void );
pass_fail_et           rf_mgr_analyse_packet_crc( u8_t* data_p, u8_t crc_pos );
void                   rf_mgr_inc_tx_packet_ctr( void );
void                   rf_mgr_setup_ack_payload( u8_t* buffer, u8_t len );
void                   rf_mgr_check_comms_lost( void );

#endif /* RF_MGR_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
