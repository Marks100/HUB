/*! \file
*               Author: mstewart
*   \brief      TB callbacks management
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "TB_CBK.h"
#include "HAL_ADC.h"
#include "TIME_MGR.h"
#include "STDC.h"

/* Module Identification for STDC_assert functionality */
#define STDC_MODULE_ID STDC_MOD_TB_CBK

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC off_on_et switch1val = OFF;
STATIC off_on_et switch2val = OFF;

/* RPC Handler Registration Array - Exported for TB configuration */
TB_rpc_handler_entry_st tb_rpc_handlers_s[] =
{
    /* Getter RPCs */
    { "switch1getval",    TB_CBK_rpc_switch1_get_val,   TRUE },
    { "switch2getval",    TB_CBK_rpc_switch2_get_val,   TRUE },
    { "getsentxvalue",    TB_CBK_rpc_get_sen_tx_value,  TRUE },
    { "tbgetposttime",    TB_CBK_rpc_tb_get_post_time,  TRUE },

    /* Setter RPCs */
    { "switch1setval",    TB_CBK_rpc_switch1_set_val,   TRUE },
    { "switch2setval",    TB_CBK_rpc_switch2_set_val,   TRUE },
    { "setsentxvalue",    TB_CBK_rpc_set_sen_tx_value,  TRUE },
    { "tbsetposttime",    TB_CBK_rpc_tb_set_post_time,  TRUE }
};

#define TB_RPC_HANDLER_COUNT  ( sizeof(tb_rpc_handlers_s) / sizeof(tb_rpc_handlers_s[0]) )

/* Telemetry Topics Array - Exported for TB configuration */
TB_telemetry_topic_st tb_telemetry_topics_s[] =
{
    {
        .topic_name      = "heartbeat",
        .callback        = tb_telemetry_heartbeat_callback,
        .interval_ms     = 300000u,
        .last_publish_ms = 0u,
        .enabled         = TRUE
    },
    {
        .topic_name      = "temperature",
        .callback        = tb_telemetry_temperature_callback,
        .interval_ms     = 300000u,
        .last_publish_ms = 0u,
        .enabled         = TRUE
    },
    {
        .topic_name      = "pressure",
        .callback        = tb_telemetry_pressure_callback,
        .interval_ms     = 300000u,
        .last_publish_ms = 0u,
        .enabled         = TRUE
    },
    {
        .topic_name      = "temperature/cave",
        .callback        = tb_telemetry_temperature_cave_callback,
        .interval_ms     = 300000u,
        .last_publish_ms = 0u,
        .enabled         = TRUE
    },
    {
        .topic_name      = "sensors",
        .callback        = tb_telemetry_sensors_callback,
        .interval_ms     = 300000u,
        .last_publish_ms = 0u,
        .enabled         = TRUE
    }
};

#define TB_TELEMETRY_TOPIC_COUNT  ( sizeof(tb_telemetry_topics_s) / sizeof(tb_telemetry_topics_s[0]) )

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

STATIC char* append( char* write_pos, const char* literal )
{
    return( (char*)STDC_strcpy( (u8_t*)write_pos, (const u8_t*)literal ) );
}

STATIC char* append_u32( char* write_pos, u32_t unsigned_value )
{
    return( (char*)STDC_dec_to_ascii( unsigned_value, (u8_t*)write_pos ) );
}

STATIC char* append_s32( char* write_pos, s32_t signed_value )
{
    if( signed_value < 0 )
    {
        *write_pos++ = '-';
        signed_value = -signed_value;
    }
    
    return( append_u32( write_pos, (u32_t)signed_value ) );
}

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

void TB_CBK_init( TB_config_st* config_p )
{
    if( config_p != NULL_P )
    {
        config_p->num_telemetry    = (u8_t)TB_TELEMETRY_TOPIC_COUNT;
        config_p->num_rpc_handlers = (u8_t)TB_RPC_HANDLER_COUNT;
    }
}

TB_rpc_handler_entry_st* TB_CBK_get_rpc_handlers( u8_t* count_p )
{
    if( count_p != NULL_P ) { *count_p = (u8_t)TB_RPC_HANDLER_COUNT; }
    return( tb_rpc_handlers_s );
}

TB_telemetry_topic_st* TB_CBK_get_telemetry_topics( u8_t* count_p )
{
    if( count_p != NULL_P ) { *count_p = (u8_t)TB_TELEMETRY_TOPIC_COUNT; }
    return( tb_telemetry_topics_s );
}

/***************************************************************************************************
**                              RPC Handlers - Getters                                            **
***************************************************************************************************/

pass_fail_et TB_CBK_rpc_switch1_get_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;
    (void)params_p;

    if( ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        char* p = (char*)response_p;
        p = append( p, "{\"result\":" );
        p = append( p, switch1val == ON ? "true}" : "false}" );
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
        result = PASS;
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_switch2_get_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;
    (void)params_p;

    if( ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        char* p = (char*)response_p;
        p = append( p, "{\"result\":" );
        p = append( p, switch2val == ON ? "true}" : "false}" );
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
        result = PASS;
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_get_sen_tx_value( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;
    (void)params_p;

    if( ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        u16_t sen_tx_rate = 30u;  /* TODO: read from NVM */
        char* p = (char*)response_p;
        p = append( p, "{\"result\":" );
        p = append_u32( p, (u32_t)sen_tx_rate );
        p = append( p, "}" );
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
        result = PASS;
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_tb_get_post_time( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;
    (void)params_p;

    if( ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        u16_t post_time_sec = 60u;  /* TODO: read from config */
        char* p = (char*)response_p;
        p = append( p, "{\"result\":" );
        p = append_u32( p, (u32_t)post_time_sec );
        p = append( p, "}" );
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
        result = PASS;
    }

    return( result );
}

/***************************************************************************************************
**                              RPC Handlers - Setters                                            **
***************************************************************************************************/

pass_fail_et TB_CBK_rpc_switch1_set_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;

    if( ( params_p != NULL_P ) && ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        if( STDC_strstr( params_p, (const u8_t*)"\"params\":true" ) != NULL_P )
        {
            switch1val = ON;
            result = PASS;
        }
        else if( STDC_strstr( params_p, (const u8_t*)"\"params\":false" ) != NULL_P )
        {
            switch1val = OFF;
            result = PASS;
        }

        char* p = (char*)response_p;
        if( result == PASS )
        {
            p = append( p, "{\"result\":" );
            p = append( p, switch1val == ON ? "true}" : "false}" );
        }
        else
        {
            p = append( p, "{\"error\":\"Invalid parameter\"}" );
        }
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_switch2_set_val( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;

    if( ( params_p != NULL_P ) && ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        if( STDC_strstr( params_p, (const u8_t*)"\"params\":true" ) != NULL_P )
        {
            switch2val = ON;
            result = PASS;
        }
        else if( STDC_strstr( params_p, (const u8_t*)"\"params\":false" ) != NULL_P )
        {
            switch2val = OFF;
            result = PASS;
        }

        char* p = (char*)response_p;
        if( result == PASS )
        {
            p = append( p, "{\"result\":" );
            p = append( p, switch2val == ON ? "true}" : "false}" );
        }
        else
        {
            p = append( p, "{\"error\":\"Invalid parameter\"}" );
        }
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_set_sen_tx_value( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;

    if( ( params_p != NULL_P ) && ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        const u8_t* value_str = STDC_strstr( params_p, (const u8_t*)"\"params\":" );
        u16_t       new_value = 0u;

        if( value_str != NULL_P )
        {
            value_str += 9u;
            while( ( *value_str >= '0' ) && ( *value_str <= '9' ) )
            {
                new_value = (u16_t)( ( new_value * 10u ) + ( *value_str - '0' ) );
                value_str++;
            }
        }

        char* p = (char*)response_p;
        if( ( value_str != NULL_P ) && ( new_value >= 1u ) && ( new_value <= 300u ) )
        {
            /* TODO: store to NVM and update RF_MGR */
            p = append( p, "{\"result\":" );
            p = append_u32( p, (u32_t)new_value );
            p = append( p, "}" );
            result = PASS;
        }
        else
        {
            p = append( p, "{\"error\":\"Value out of range (1-300)\"}" );
        }
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
    }

    return( result );
}

pass_fail_et TB_CBK_rpc_tb_set_post_time( const u8_t* params_p, u8_t* response_p, u16_t* response_len_p )
{
    pass_fail_et result = FAIL;

    if( ( params_p != NULL_P ) && ( response_p != NULL_P ) && ( response_len_p != NULL_P ) )
    {
        const u8_t* value_str        = STDC_strstr( params_p, (const u8_t*)"\"params\":" );
        u32_t       new_interval_sec = 0u;

        if( value_str != NULL_P )
        {
            value_str += 9u;
            while( ( *value_str >= '0' ) && ( *value_str <= '9' ) )
            {
                new_interval_sec = ( new_interval_sec * 10u ) + ( *value_str - '0' );
                value_str++;
            }
        }

        char* p = (char*)response_p;
        if( ( value_str != NULL_P ) && ( new_interval_sec >= 10u ) && ( new_interval_sec <= 3600u ) )
        {
            u32_t new_interval_ms = new_interval_sec * 1000u;
            for( u8_t i = 0u; i < (u8_t)TB_TELEMETRY_TOPIC_COUNT; i++ )
            {
                tb_telemetry_topics_s[i].interval_ms = new_interval_ms;
            }
            p = append( p, "{\"result\":" );
            p = append_u32( p, new_interval_sec );
            p = append( p, "}" );
            result = PASS;
        }
        else
        {
            p = append( p, "{\"error\":\"Value out of range (10-3600)\"}" );
        }
        *p = '\0';
        *response_len_p = (u16_t)( p - (char*)response_p );
    }

    return( result );
}

/***************************************************************************************************
**                              Telemetry Callbacks                                               **
***************************************************************************************************/

u16_t tb_telemetry_heartbeat_callback( u8_t* buffer_p, u16_t buffer_size )
{
    STATIC u32_t heartbeat_count_s = 0u;
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        u32_t  uptime_s = TIME_get_cumulative_run_time_ms() / 1000u;
        char*  p        = (char*)buffer_p;
        p = append( p, "{\"uptime\":" );     p = append_u32( p, uptime_s );
        p = append( p, ",\"heartbeat\":" );  p = append_u32( p, heartbeat_count_s );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
        heartbeat_count_s++;
    }

    return( json_len );
}

u16_t tb_telemetry_temperature_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        /* T(°C) = ((V25 - Vsense) / Avg_Slope) + 25  [V25=1430mV, slope=4.3mV/°C, VDDA=3300mV] */
        u16_t raw    = HAL_ADC_measure_STM32_temp_raw();
        s32_t vsense = (s32_t)raw * 3300L / 4096L;
        s16_t temp_c = (s16_t)( ( ( 1430L - vsense ) * 10L / 43L ) + 25L );

        char* p = (char*)buffer_p;
        p = append( p, "{\"temperature\":" );
        p = append_s32( p, (s32_t)temp_c );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_pressure_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC u16_t pressure_hpa_s = 1013u;
        pressure_hpa_s += 2u;
        if( pressure_hpa_s > 1020u ) { pressure_hpa_s = 980u; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"pressure\":" );
        p = append_u32( p, (u32_t)pressure_hpa_s );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_temperature_cave_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC s16_t cave_temp_s = 17;
        cave_temp_s++;
        if( cave_temp_s > 20 ) { cave_temp_s = 15; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"temperature_cave\":" );
        p = append_s32( p, (s32_t)cave_temp_s );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_sensors_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC s16_t temp_s     = 22;
        STATIC u16_t pressure_s = 1013u;
        STATIC u16_t humidity_s = 45u;

        temp_s++;
        if( temp_s > 28 ) { temp_s = 20; }
        pressure_s += 2u;
        if( pressure_s > 1020u ) { pressure_s = 1000u; }
        humidity_s += 2u;
        if( humidity_s > 70u ) { humidity_s = 30u; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"temperature\":" );  p = append_s32( p, (s32_t)temp_s );
        p = append( p, ",\"pressure\":" );      p = append_u32( p, (u32_t)pressure_s );
        p = append( p, ",\"humidity\":" );      p = append_u32( p, (u32_t)humidity_s );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_battery_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        /* Stored in millivolts to retain resolution without float */
        STATIC u16_t voltage_mv_s = 12500u;
        voltage_mv_s += 100u;
        if( voltage_mv_s > 13500u ) { voltage_mv_s = 11500u; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"battery_mv\":" );
        p = append_u32( p, (u32_t)voltage_mv_s );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_system_status_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC u8_t can_active = 1u;
        STATIC u8_t errors     = 0u;

        can_active = (can_active == 1u) ? 0u : 1u;
        errors = (errors + 1u) % 3u;

        char* p = (char*)buffer_p;
        p = append( p, "{\"wifi\":" );    p = append_u32( p, (u32_t)WIFI_is_connected() );
        p = append( p, ",\"can\":" );     p = append_u32( p, (u32_t)can_active );
        p = append( p, ",\"errors\":" );  p = append_u32( p, (u32_t)errors );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_memory_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC u32_t heap_used  = 1024u;
        STATIC u32_t stack_used = 512u;

        heap_used += 64u;
        if( heap_used > 2048u ) { heap_used = 512u; }
        stack_used = (stack_used + 32u) % 1024u;

        char* p = (char*)buffer_p;
        p = append( p, "{\"heap_used\":" );   p = append_u32( p, heap_used );
        p = append( p, ",\"stack_used\":" );  p = append_u32( p, stack_used );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_comm_stats_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC u32_t packets_tx  = 0u;
        STATIC u32_t packets_rx  = 0u;
        STATIC u16_t packet_loss = 0u;

        packets_tx  += 10u;
        packets_rx  += 9u;
        packet_loss  = (u16_t)( packets_tx - packets_rx );

        char* p = (char*)buffer_p;
        p = append( p, "{\"tx_packets\":" );   p = append_u32( p, packets_tx );
        p = append( p, ",\"rx_packets\":" );   p = append_u32( p, packets_rx );
        p = append( p, ",\"packet_loss\":" );  p = append_u32( p, (u32_t)packet_loss );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_cpu_load_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC u8_t cpu_load_pct = 25u;
        cpu_load_pct += 5u;
        if( cpu_load_pct > 80u ) { cpu_load_pct = 20u; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"cpu_load\":" );
        p = append_u32( p, (u32_t)cpu_load_pct );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

u16_t tb_telemetry_rf_signal_callback( u8_t* buffer_p, u16_t buffer_size )
{
    u16_t json_len = 0u;

    if( ( NULL_P != buffer_p ) && ( buffer_size > 0u ) )
    {
        STATIC s8_t rssi_dbm       = -50;
        STATIC u8_t signal_quality = 80u;

        rssi_dbm -= 2;
        if( rssi_dbm < -90 ) { rssi_dbm = -40; }
        signal_quality = (u8_t)( 100 + rssi_dbm );
        if( signal_quality > 100u ) { signal_quality = 100u; }

        char* p = (char*)buffer_p;
        p = append( p, "{\"rssi\":" );            p = append_s32( p, (s32_t)rssi_dbm );
        p = append( p, ",\"signal_quality\":" );  p = append_u32( p, (u32_t)signal_quality );
        p = append( p, "}" );
        *p = '\0';
        json_len = (u16_t)( p - (char*)buffer_p );
    }

    return( json_len );
}

/****************************** END OF FILE *******************************************************/
