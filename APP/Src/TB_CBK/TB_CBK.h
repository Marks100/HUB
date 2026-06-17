#ifndef TB_CBK_H
#define TB_CBK_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "HAL_BRD.h"
#include "TB.h"
#include "WIFI.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Exported Variables                                                **
***************************************************************************************************/
/* RPC Handler array - exported for TB configuration initialization */
extern TB_rpc_handler_entry_st tb_rpc_handlers_s[];

/* Telemetry Topics array - exported for TB configuration initialization */
extern TB_telemetry_topic_st tb_telemetry_topics_s[];

/***************************************************************************************************
**                              Public Function Prototypes                                        **
***************************************************************************************************/
/* Initialization */
void TB_CBK_init( TB_config_st* config_p );

/* RPC Callbacks - Getters (TB_rpc_handler_t signature) */
pass_fail_et TB_CBK_rpc_switch1_get_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_switch2_get_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_get_sen_tx_value( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_tb_get_post_time( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );

/* RPC Callbacks - Setters (TB_rpc_handler_t signature) */
pass_fail_et TB_CBK_rpc_switch1_set_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_switch2_set_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_set_sen_tx_value( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );
pass_fail_et TB_CBK_rpc_tb_set_post_time( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p );

/* Get RPC handler array */
TB_rpc_handler_entry_st* TB_CBK_get_rpc_handlers( u8_t* count_p );

/* Get telemetry topics array */
TB_telemetry_topic_st* TB_CBK_get_telemetry_topics( u8_t* count_p );

/* Telemetry Callbacks (TB_telemetry_callback_t signature) */
u16_t tb_telemetry_heartbeat_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_temperature_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_pressure_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_temperature_cave_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_sensors_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_battery_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_system_status_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_memory_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_comm_stats_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_cpu_load_callback( u8_t* buffer_p, u16_t buffer_size );
u16_t tb_telemetry_rf_signal_callback( u8_t* buffer_p, u16_t buffer_size );

#endif /* TB_CBK_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
