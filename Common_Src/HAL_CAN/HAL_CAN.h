#ifndef HAL_CAN_H
#define HAL_CAN_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "HAL_config.h"
#include "CANIF_types.h"   /* Generic controller mode/status + ops shape this driver implements */

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* LinkControl (0x87) baud rate parameters, ISO 14229-1's fixed baud rates for CAN */
#define HAL_CAN_BAUD_PARAM_125K  ( 0x11u )
#define HAL_CAN_BAUD_PARAM_250K  ( 0x12u )
#define HAL_CAN_BAUD_PARAM_500K  ( 0x13u )    /* what HAL_CAN_init() runs at unless HAL_CAN_set_baud() says otherwise */

/***************************************************************************************************
**                              Data Types                                                        **
***************************************************************************************************/
/* id_type: 0 = Standard (11-bit), 1 = Extended (29-bit). channel is whatever was given to
   HAL_CAN_set_rx_callback() - the controller doesn't know its own channel number, the board does.
   Same shape as CANIF_rx_indication(), so APP binds that directly. */
typedef void (*HAL_CAN_rx_callback_ft)( u8_t channel, u32_t id, u8_t id_type, const u8_t* data_p, u8_t dlc );

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* HAL_CAN_send_frame/HAL_CAN_set_mode/HAL_CAN_get_status have the shapes of CANIF_channel_cfg_st's
   send_frame/ctrl_set_mode/ctrl_get_status - a CANIF channel table names them directly */
void         HAL_CAN_init( void );
false_true_et HAL_CAN_baud_is_supported( u8_t baud_param );
pass_fail_et HAL_CAN_set_baud( u8_t baud_param );    /* before HAL_CAN_init(): the bit rate it sets up */
void         HAL_CAN_set_rx_callback( u8_t channel, HAL_CAN_rx_callback_ft rx_callback_p );
pass_fail_et HAL_CAN_send_frame( u32_t id, u8_t id_type, const u8_t* data_p, u8_t dlc );
pass_fail_et HAL_CAN_set_mode( CANIF_ctrl_mode_et mode );
pass_fail_et HAL_CAN_get_status( CANIF_ctrl_status_st* status_p );

#endif /* HAL_CAN_H */

/****************************** END OF FILE *******************************************************/
