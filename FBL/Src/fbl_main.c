/*! \file
*               Author: mstewart
*   \brief      FBL (Field Bootloader) entry point and platform wiring for STM32F103C8
*
*   Reset handler -> FBL_init()/FBL_run() (xCOMMON_MODULES/Src/FBL) with every platform
*   dependency supplied here via fbl_config_s, following the exact pattern BM/Src/bm_main.c
*   already established for BM.
*
*   CAN transceiver: TJA1051 on this board (see APP/Src/HAL_config.h's TJA1051_EN_PIN, GPIOB10) -
*   needs its EN pin driven high at init, unlike the source project's UJA1167A which was a full
*   SPI-configured system-basis-chip (dropped entirely - this board has neither the SBC nor the
*   OLED display the source project's MAIN.c also wired up).
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL.h"
#include "CHKSUM.h"
#include "CLK_STM32F1.h"
#include "SHARED_RAM.h"
#include "FLS_STM32F1.h"
#include "SYSTICK.h"
#include "TIME_MGR.h"
#include "HAL_CAN.h"
#include "TJA1051.h"
#include "CANTP.h"
#include "PDUR.h"
#include "UDS.h"
#include "UDS_config.h"
#include "MCU_JUMP.h"
#include "system_stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/* Board pin map - shared with APP, see APP/Src/HAL_config.h */
#define TJA1051_EN_PORT   GPIOB
#define TJA1051_EN_PIN    GPIO_Pin_10

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
extern u32_t __app_start__;
extern u32_t __app_code_end__;
extern u32_t _estack;

extern u32_t _sidata;
extern u32_t _sdata;
extern u32_t _edata;
extern u32_t _sbss;
extern u32_t _ebss;

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void crc_init( void );
STATIC void clk_init( void );
STATIC void fbl_systick_callback( void );
STATIC void systick_init_wrapper( void );
STATIC void can_init_wrapper( void );
STATIC void cantp_init_wrapper( void );
STATIC void pdur_init_wrapper( void );
STATIC void fbl_can_rx_wrapper( u32_t id, u8_t* data_p, u8_t dlc );
STATIC void fbl_cantp_send_wrapper( CANTP_can_msg_format_st* msg_p );
STATIC void fbl_cantp_tx_request_wrapper( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len, u8_t channel );
STATIC void fbl_pdur_tx_uds( u8_t* data_p, u16_t len );
STATIC void fbl_uds_session_notify( UDS_session_et old_session, UDS_session_et new_session );
STATIC void tja1051_en_pin_set( low_high_et state );

/***************************************************************************************************
**                              CRC Configuration                                                **
***************************************************************************************************/
/* Same profile as BM/APP - must match app_crc_injector's default --crc-type crc32. */
STATIC const hw_crc_config_st hw_crc_cfg_s =
{
    .width             = HW_CRC_WIDTH_32BIT,
    .polynomial        = 0x04C11DB7UL,
    .seed              = 0xFFFFFFFFUL,
    .complement_result = TRUE,
};

STATIC void crc_init( void )
{
    CHKSUM_init_hw_crc( &hw_crc_cfg_s );
}

/***************************************************************************************************
**                              Clock Configuration                                              **
***************************************************************************************************/
/* Same profile as BM/APP - see BM/Src/bm_main.c's comment on why this must agree across stages. */
STATIC void clk_init( void )
{
    CLK_STM32F1_init( &hse8_72mhz_s );
}

/***************************************************************************************************
**                              SysTick / 1ms tick                                                **
***************************************************************************************************/
STATIC volatile false_true_et fbl_1ms_tick_flag_s = FALSE;

STATIC void fbl_systick_callback( void )
{
    fbl_1ms_tick_flag_s = TRUE;
    TIME_increment_time();
}

STATIC void systick_init_wrapper( void )
{
    SYSTICK_cfg_st cfg =
    {
        .timeout_ms     = 1u,
        .systick_func_p = fbl_systick_callback
    };

    SYSTICK_init( &cfg, SystemCoreClock );
}

/*! Implements FBL.h's FBL_is_1ms_elapsed() - see FBL_run()'s poll loop. */
false_true_et FBL_is_1ms_elapsed( void )
{
    false_true_et result = fbl_1ms_tick_flag_s;

    if( fbl_1ms_tick_flag_s == TRUE )
    {
        fbl_1ms_tick_flag_s = FALSE;
    }

    return( result );
}

/***************************************************************************************************
**                              CAN Transceiver (TJA1051)                                         **
***************************************************************************************************/
STATIC void tja1051_en_pin_set( low_high_et state )
{
    if( state == HIGH )
    {
        GPIO_SetBits( TJA1051_EN_PORT, TJA1051_EN_PIN );
    }
    else
    {
        GPIO_ResetBits( TJA1051_EN_PORT, TJA1051_EN_PIN );
    }
}

STATIC const TJA1051_func_st tja1051_func_table_s =
{
    .en_pin_set = tja1051_en_pin_set,
    .s_pin_set  = NULL_P,  /* base TJA1051 has no S pin on this board */
};

STATIC const TJA1051_config_st tja1051_config_s =
{
    .initial_mode   = TJA1051_MODE_NORMAL,
    .event_callback = NULL_P,
};

/***************************************************************************************************
**                              CAN / CAN-TP / PDU-R / UDS Wiring                                 **
***************************************************************************************************/
STATIC void fbl_can_rx_wrapper( u32_t id, u8_t* data_p, u8_t dlc )
{
    CANTP_rx_frame_received( id, STANDARD_ID, data_p, (u16_t)dlc, 0u );
}

STATIC void fbl_cantp_send_wrapper( CANTP_can_msg_format_st* msg_p )
{
    if( msg_p != NULL_P )
    {
        (void)HAL_CAN_send_frame( msg_p->Id, msg_p->Data, msg_p->DLC );
    }
}

/* PDUR's lower_layer_tx_func_t shape - routes application TX through CANTP so multi-frame
   responses get segmented, matching PDUR.c's PDUR_tx() which calls this directly per-route. */
STATIC void fbl_cantp_tx_request_wrapper( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len, u8_t channel )
{
    (void)CANTP_tx_request( id, (CANTP_id_type_et)id_type, (CANTP_frame_type_et)frame_type, data_p, len, channel, NULL_P );
}

STATIC void can_init_wrapper( void )
{
    GPIO_InitTypeDef gpio_init;

    /* TJA1051 EN pin - push-pull output, driven via tja1051_en_pin_set() */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
    gpio_init.GPIO_Pin   = TJA1051_EN_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init( TJA1051_EN_PORT, &gpio_init );

    TJA1051_init( &tja1051_func_table_s, &tja1051_config_s );

    HAL_CAN_init( fbl_can_rx_wrapper );
}

STATIC void cantp_init_wrapper( void )
{
    STATIC CANTP_cfg_st fbl_cantp_cfg_s =
    {
        .CANTP_message_rx_func_p = PDUR_rx_indication,
        .CANTP_error_func_p      = NULL_P,
        .channel_tx_funcs        = { fbl_cantp_send_wrapper },
        .tp_buffer               = pdur_buffer_s,
        .tp_ids                  = { FBL_UDS_REQUEST_ID, FBL_CAN_RX_ID },
        .st_min                  = 10u,
        .rx_block_size           = 10u,
        .N_Cr                    = CANTP_DEFAULT_N_CR,
        .N_Bs                    = CANTP_DEFAULT_N_BS,
        .N_Ar                    = CANTP_DEFAULT_N_AR,
        .uds_req_id              = FBL_UDS_REQUEST_ID,
        .uds_resp_id             = FBL_UDS_RESPONSE_ID,
        .num_channels            = 1u,
        .fd_enable               = FALSE,
    };

    CANTP_init( &fbl_cantp_cfg_s );
}

/* Bidirectional routes: functional request (0x700->0x600) and physical request (0x7E0->0x7E8).
   lower_layer_tx_func routes through CANTP (frame_type=TP) so multi-frame UDS responses get
   segmented automatically - see PDUR.c's PDUR_tx(), which calls this per-route. */
STATIC const PDUR_rx_route_st fbl_pdur_routing_table_s[] =
{
    { FBL_UDS_REQUEST_ID, 0xFFFFFFFFu, FBL_UDS_RESPONSE_ID, 0u, 2u, 0u, UDS_rx_indication, fbl_cantp_tx_request_wrapper, NULL_P },
    { FBL_CAN_RX_ID,      0xFFFFFFFFu, FBL_CAN_TX_ID,       0u, 2u, 0u, UDS_rx_indication, fbl_cantp_tx_request_wrapper, NULL_P },
};

STATIC void pdur_init_wrapper( void )
{
    PDUR_init( fbl_pdur_routing_table_s, (u16_t)( sizeof( fbl_pdur_routing_table_s ) / sizeof( PDUR_rx_route_st ) ) );
}

STATIC void fbl_pdur_tx_uds( u8_t* data_p, u16_t len )
{
    PDUR_tx( FBL_UDS_RESPONSE_ID, data_p, len );
}

/*!
****************************************************************************************************
*   \brief         UDS session change notification
*   \details       Tester returning PROGRAMMING->DEFAULT means "exit FBL" - clear the FBL request
*                  flag so BM boots APP after the soft reset UDS schedules for this transition.
***************************************************************************************************/
STATIC void fbl_uds_session_notify( UDS_session_et old_session, UDS_session_et new_session )
{
    if( ( old_session == UDS_SES_PROGRAMMING ) && ( new_session == UDS_SES_DEFAULT ) )
    {
        SHARED_RAM_set_fbl_request( FALSE );
    }
}

STATIC const UDS_func_p_st fbl_uds_func_table_s =
{
    .tp_send_func_p           = fbl_pdur_tx_uds,
    .perform_soft_reset       = MCU_JUMP_software_reset,
    .perform_hard_reset       = MCU_JUMP_software_reset,
    .s3_timeout_notify        = NULL_P,   /* FBL manages its own 30s boot delay instead */
    .session_change_notify    = fbl_uds_session_notify,
    .message_received_notify  = FBL_session_timeout_reset,
    .tester_present_notify    = FBL_boot_delay_reset,
};

/***************************************************************************************************
**                              Field Bootloader Configuration                                    **
***************************************************************************************************/
/* Caller-owned transfer buffer injected into FBL core for sector-staged writes - sized to the
   STM32F103 medium-density page size (1KB), not S32K144's 4KB sector. */
STATIC u8_t fbl_transfer_sector_buffer_s[ FLS_STM32F1_PAGE_SIZE ];

STATIC const fbl_config_st fbl_config_s =
{
    /* Initialisation functions */
    .clk_init        = clk_init,
    .wdg_init        = NULL_P,  /* APP controls the watchdog, same convention as BM */
    .can_init        = can_init_wrapper,
    .cantp_init      = cantp_init_wrapper,
    .pdur_init       = pdur_init_wrapper,
    .crc_init        = crc_init,
    .flash_init      = FLS_STM32F1_init,
    .shared_ram_init = SHARED_RAM_init,
    .systick_init    = systick_init_wrapper,

    /* Runtime function pointers */
    .wdg_kick_func_p           = NULL_P,
    .cantp_tick_func_p         = CANTP_tick,
    .time_get_tick_func_p      = TIME_get_cumulative_run_time_ms,
    .flash_erase_sector_func_p = FLS_STM32F1_erase_sector,
    .flash_write_data_func_p   = FLS_STM32F1_write_data,
    .uds_tx_func_p             = fbl_pdur_tx_uds,
    .crc_calculate_func_p      = CHKSUM_calc_hw_crc32,

    /* Shared RAM interface - same module/contract BM already uses */
    .shared_ram_get_request_func_p = SHARED_RAM_get_fbl_request,
    .shared_ram_set_request_func_p = SHARED_RAM_set_fbl_request,
    .shared_ram_is_valid_func_p    = SHARED_RAM_is_valid,

    /* Configuration values */
    .flash_sector_size          = FLS_STM32F1_PAGE_SIZE,
    .max_transfer_block_len     = FLS_STM32F1_PAGE_SIZE,
    .transfer_sector_buffer_p   = fbl_transfer_sector_buffer_s,
    .transfer_sector_buffer_len = FLS_STM32F1_PAGE_SIZE,
    .app_start_address          = (u32_t)&__app_start__,
    .app_end_address            = (u32_t)&__app_code_end__,
};

/***************************************************************************************************
**                              Reset Handler / Vector Table                                     **
***************************************************************************************************/
void FBL_Reset_Handler( void );

__attribute__((section(".isr_vector"), used))
const u32_t fbl_vector_table[2] =
{
    (u32_t)&_estack,
    (u32_t)FBL_Reset_Handler,
};

void FBL_Reset_Handler( void )
{
    u32_t*             src_p;
    u32_t*             dst_p;
    STATIC const TIME_cfg_st time_cfg_s = { .time_increment_ms = 1u };

    /* Copy .data (initialised globals) from flash to RAM */
    src_p = &_sidata;
    dst_p = &_sdata;
    while( dst_p < &_edata )
    {
        *dst_p = *src_p;
        dst_p++;
        src_p++;
    }

    /* Zero .bss (uninitialised globals) */
    dst_p = &_sbss;
    while( dst_p < &_ebss )
    {
        *dst_p = 0u;
        dst_p++;
    }

    TIME_init( &time_cfg_s );

    /* UDS module init before FBL_init so callbacks are registered before anything can call in */
    UDS_init( &fbl_uds_func_table_s, UDS_get_service_table(), UDS_get_service_table_size(), pdur_buffer_s, PDUR_BUFFER_SIZE );

    /* FBL is always in PROGRAMMING session - being in FBL space at all means we're programming */
    UDS_set_session( UDS_SES_PROGRAMMING );

    FBL_init( &fbl_config_s );
    FBL_run();
}

/****************************** END OF FILE *******************************************************/
