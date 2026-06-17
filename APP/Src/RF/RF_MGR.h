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

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define RF_MGR_TICK_RATE                ( MODE_MGR_TICK_RATE_MSECS )
#define RF_MGR_MAX_PAYLOAD_SIZE         ( NRF_MAX_PAYLOAD_SIZE )
#define RF_MGR_TX_TIMEOUT_MS            ( 30u/RF_MGR_TICK_RATE )
#define RF_MGR_RX_TIMEOUT_MS            ( 30u/RF_MGR_TICK_RATE )

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
    RF_MGR_IDLE = 0u,
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

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void               RF_MGR_init( RF_MGR_rf_state_et state, u8_t channel );
void               RF_MGR_tick( void );
RF_MGR_rf_state_et RF_MGR_get_state( void );
void               RF_MGR_tx_complete( pass_fail_et state );
void               RF_MGR_get_rx_frame( void );

void               rf_mgr_set_state( RF_MGR_rf_state_et state );
void               rf_mgr_send_payload( void );
void               rf_mgr_analyse_received_frame( u8_t* data_p );
void               rf_mgr_configure_transmitt_mode( u8_t channel );
void               rf_mgr_configure_receive_mode( void );
void               rf_mgr_power_down_chip( void );
void               rf_mgr_setup_tx_payload( void );
pass_fail_et       rf_mgr_analyse_packet_crc( u8_t* data_p, u8_t crc_pos );
void               rf_mgr_inc_tx_packet_ctr( void );
void               rf_mgr_setup_ack_payload( u8_t* buffer, u8_t len );

#endif /* RF_MGR_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
