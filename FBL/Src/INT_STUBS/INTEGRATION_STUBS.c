/*! \file
*               Author: mstewart
*   \brief      FBL (Field Bootloader) platform wiring for STM32F103C8
*
*   Everything here exists to bind generic modules to this board. Most functions are one-liners
*   that supply something the module's function-pointer signature has no room for: a config struct
*   (clk_init, crc_init), an instance pointer (fbl_cantp_*), a fixed CAN ID (fbl_pdur_tx_uds), or a
*   type the two layers spell differently (fbl_cantp_rx_indication). They are adapters, not
*   indirection - deleting one means changing a module's contract for every project that uses it.
*
*   Definitions are ordered so each is complete before it is referenced, which is why this file
*   carries no forward-declaration block. Keep it that way when adding to it.
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "INTEGRATION_STUBS.h"
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
#include "UDS_config.h"
#include "HAL_I2C.h"
#include "SH1106.h"
#include "printf.h"
#include "MCU_JUMP.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
extern u32_t __app_header_start__;
extern u32_t __app_code_end__;

/***************************************************************************************************
**                              Timekeeping                                                       **
***************************************************************************************************/
/* Passed to TIME_init(), called directly from fbl_main() rather than through fbl_config_st, since
   it has to run before FBL_init() (which is what actually consumes fbl_config_s). */
const TIME_cfg_st time_cfg_s = { .time_increment_ms = 1u };

/***************************************************************************************************
**                              Clock / CRC                                                       **
***************************************************************************************************/
/* Same CRC profile as BM/APP - must match app_crc_injector's --crc-type. */
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

STATIC void clk_init( void )
{
    CLK_STM32F1_init( &hse8_72mhz_s );
}

/***************************************************************************************************
**                              OLED Progress Display (SH1106 via I2C1)                           **
***************************************************************************************************/
/* Low-level SH1106.c only, no HMI_SH1106.c/GFX.c - storage is caller-owned per SH1106.h. */
STATIC SH1106_instance_st fbl_display_instance_s;
STATIC u8_t               fbl_display_framebuffer_s[SH1106_FRAMEBUFFER_SIZE];
STATIC bool_et            fbl_display_page_dirty_s[SH1106_NUM_PAGES];

/*! Implements FBL.h's display_update_func_p - called on download start/each block/exit and once a
 *  second while idle. Redraws the whole screen so a shrinking number leaves no stale digit. */
STATIC void display_render( u8_t percent, u8_t seconds_remaining )
{
    char line[SH1106_MAX_CHARS_PER_LINE + 1u];

    SH1106_buffer_clear( &fbl_display_instance_s );
    SH1106_buffer_write_string( &fbl_display_instance_s, 1u, 0u, "FIELD BOOTLOADER", FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Progress:  %3u%%", (unsigned int)percent );
    SH1106_buffer_write_string( &fbl_display_instance_s, 3u, 0u, line, FALSE );

    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "Auto-boot: %2us", (unsigned int)seconds_remaining );
    SH1106_buffer_write_string( &fbl_display_instance_s, 5u, 0u, line, FALSE );
}

STATIC void display_init( void )
{
    STATIC const sh1106_config_st display_cfg_s =
    {
        .i2c_write_func_p  = HAL_I2C_write_registers,   /* Signatures already match - no adapter */
        .i2c_address       = SH1106_I2C_ADDR_DEFAULT,
        .contrast          = SH1106_DEFAULT_CONTRAST,
        /* Same panel APP drives, mounted rotated - must match INTEGRATION_STUBS.c's config. */
        .flip_horizontal   = TRUE,
        .flip_vertical     = TRUE,
    };

    HAL_I2C1_init();
    SH1106_init( &fbl_display_instance_s, fbl_display_framebuffer_s, fbl_display_page_dirty_s, &display_cfg_s );

    /* First frame through the same path every later update uses - matches what FBL_init() sets. */
    display_render( 0u, (u8_t)( FBL_BOOT_DELAY_MS / 1000u ) );
}

/***************************************************************************************************
**                              SysTick / 1ms tick                                                **
***************************************************************************************************/
STATIC void fbl_systick_callback( void )
{
    TIME_increment_time();

    SH1106_tick( &fbl_display_instance_s );

    FBL_tick();
}

/* SystemCoreClock is only known at runtime, so the config cannot be a static initialiser. */
STATIC void systick_init( void )
{
    SYSTICK_cfg_st cfg =
    {
        .timeout_ms     = 1u,
        .systick_func_p = fbl_systick_callback
    };

    SYSTICK_init( &cfg, SystemCoreClock );
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
**                              CAN / CAN-TP / PDU-R                                              **
***************************************************************************************************/
/* Deliberately NOT a designated initializer - see the identical comment on app_cantp_instance_s in
   APP/Src/INT_STUBS/INTEGRATION_STUBS.c: CANTP_instance_st embeds ~1.7KB of near-all-zero RX/TX
   queue and TP session arrays, and any non-zero designated field forces the compiler to store the
   WHOLE struct as flash-resident .data. On FBL's flash budget that is the difference between
   fitting and not. Zero-initialised in .bss, populated at runtime in cantp_init(). */
STATIC CANTP_instance_st fbl_cantp_instance_s;

/* Unpacks CANTP's message struct into HAL_CAN's argument list. */
STATIC void fbl_cantp_send( CANTP_can_msg_format_st* msg_p )
{
    if( msg_p != NULL_P )
    {
        (void)HAL_CAN_send_frame( msg_p->Id, (u8_t)msg_p->id_type, msg_p->Data, msg_p->DLC );
    }
}

/* CANTP_message_rx_func_p carries CANTP_id_type_et, but PDUR stays CANTP-agnostic and takes a plain
   u8_t - different types, so a direct function-pointer assignment would not compile. */
STATIC void fbl_cantp_rx_indication( u32_t id, CANTP_id_type_et id_type, u8_t* data_p, u16_t len )
{
    PDUR_rx_indication( id, (u8_t)id_type, data_p, len );
}

/* The three below supply &fbl_cantp_instance_s, which the callback signatures have no room for. */
STATIC void fbl_can_rx( u32_t id, u8_t id_type, u8_t* data_p, u8_t dlc )
{
    CANTP_rx_frame_received( &fbl_cantp_instance_s, id, (CANTP_id_type_et)id_type, data_p, (u16_t)dlc );
}

/* PDUR's lower_layer_tx_func_t shape - supplies &fbl_cantp_instance_s, which it has no room for. */
STATIC void fbl_cantp_tx_request( u32_t id, u8_t id_type, u8_t frame_type, u8_t* data_p, u16_t len )
{
    (void)CANTP_tx_request( &fbl_cantp_instance_s, id, (CANTP_id_type_et)id_type, (CANTP_frame_type_et)frame_type, data_p, len, NULL_P );
}

STATIC void fbl_cantp_tick( void )
{
    CANTP_tick( &fbl_cantp_instance_s );
}

STATIC void can_init( void )
{
    GPIO_InitTypeDef gpio_init;

    /* TJA1051 EN pin - push-pull output, driven via tja1051_en_pin_set() */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
    gpio_init.GPIO_Pin   = TJA1051_EN_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init( TJA1051_EN_PORT, &gpio_init );

    TJA1051_init( &tja1051_func_table_s, &tja1051_config_s );

    HAL_CAN_init();
    HAL_CAN_set_rx_callback( fbl_can_rx );
}

STATIC void cantp_init( void )
{
    /* Compound-literal assignment rather than field-by-field: costs ~32 bytes more than individual
       assignments at -Og (measured - GCC doesn't lower this to the same store sequence), which is
       real but accepted here for readability. Still .bss, not .data: fbl_cantp_instance_s's own
       declaration above has no initializer, so this assignment doesn't change where the object
       itself lives - only how the initial values get written into it. */
    fbl_cantp_instance_s = (CANTP_instance_st)
    {
        .CANTP_message_rx_func_p = fbl_cantp_rx_indication,
        .tx_func_p               = fbl_cantp_send,
        .tp_buffer               = pdur_buffer_s,
        .tp_ids                  = { FBL_UDS_REQUEST_ID, FBL_CAN_RX_ID },
        .st_min                  = 10u,
        .rx_block_size           = 10u,
        .N_Cr                    = CANTP_DEFAULT_N_CR,
        .N_Bs                    = CANTP_DEFAULT_N_BS,
        .N_Ar                    = CANTP_DEFAULT_N_AR,
        .uds_req_id              = FBL_UDS_REQUEST_ID,
        .uds_resp_id             = FBL_UDS_RESPONSE_ID,
    };

    CANTP_init( &fbl_cantp_instance_s );
}

/* Bidirectional routes: functional (0x700->0x600) and physical (0x7E0->0x7E8). TX goes via CANTP so
   multi-frame UDS responses get segmented - see PDUR_tx(), which calls this per-route. tx_frame_type
   must be TP (CANTP_frame_type_et), not a bare frame-length category - CANTP_tx_request() only
   branches on `frame_type == TP`, anything else (including the old `2u` here) falls through to its
   NORMAL/raw path and skips ISO-TP PCI framing entirely. */
STATIC const PDUR_rx_route_st fbl_pdur_routing_table_s[] =
{
    { FBL_UDS_REQUEST_ID, 0xFFFFFFFFu, FBL_UDS_RESPONSE_ID, 0u, TP, UDS_rx_indication, fbl_cantp_tx_request },
    { FBL_CAN_RX_ID,      0xFFFFFFFFu, FBL_CAN_TX_ID,       0u, TP, UDS_rx_indication, fbl_cantp_tx_request },
};

STATIC void pdur_init( void )
{
    PDUR_init( fbl_pdur_routing_table_s, (u16_t)( sizeof( fbl_pdur_routing_table_s ) / sizeof( PDUR_rx_route_st ) ) );
}

/* Supplies the response CAN ID, which UDS's tp_send_func_p signature has no room for. */
STATIC void fbl_pdur_tx_uds( u8_t* data_p, u16_t len )
{
    PDUR_tx( FBL_UDS_RESPONSE_ID, 0u, data_p, len );
}

/***************************************************************************************************
**                              UDS                                                               **
***************************************************************************************************/
/*! Any transition to DEFAULT means "exit FBL" - clear the request flag so BM boots APP after the
 *  soft reset that transition schedules. Keyed on new_session alone because being in FBL space at
 *  all means we are programming, even if the tester hopped via EXTENDED - which is why the session
 *  table in UDS_CFG/UDS_config.c wildcards from_session on its ANY -> DEFAULT entry. The two must
 *  agree: that table decides when the reset happens, this decides which image BM boots after it. */
STATIC void fbl_uds_session_notify( UDS_session_et old_session, UDS_session_et new_session )
{
    if( new_session == UDS_SES_DEFAULT )
    {
        SHARED_RAM_set_fbl_request( FALSE );
    }

    /* Tester left PROGRAMMING for EXTENDED or DEFAULT - cleans up FBL's internal download/security
       state immediately instead of leaving it dangling until either the 30s programming_timeout
       self-heal or an eventual MCU reset reinitialises everything from scratch. A self-transition
       (old==new==PROGRAMMING) does not match, so a healthy in-progress download is untouched. */
    if( ( old_session == UDS_SES_PROGRAMMING ) && ( new_session != UDS_SES_PROGRAMMING ) )
    {
        FBL_download_abort();
    }
}

const UDS_func_p_st fbl_uds_func_table_s =
{
    .tp_send_func_p           = fbl_pdur_tx_uds,
    .perform_soft_reset       = MCU_JUMP_software_reset,
    .perform_hard_reset       = MCU_JUMP_software_reset,
    .s3_timeout_notify        = NULL_P,   /* FBL manages its own 30s boot delay instead */
    .session_change_notify    = fbl_uds_session_notify,
    /* fbl_tick_timers() decrements app_boot_delay_ms unconditionally every tick regardless of
       download.state - fbl_run_state_machine() only skips *acting* on it while a download is
       active, it does not pause the countdown itself. A real transfer (~100 TransferData calls)
       comfortably outlasts the 30s delay, so without this the timer sits at 0 for the rest of the
       download, and the instant state flips to FBL_STATE_COMPLETE after a successful 0x37,
       fbl_run_boot_timer() resets the MCU immediately - before CANTP's TX queue ever gets a tick
       to transmit the queued positive response. Wiring both notifies to the reset keeps the
       countdown fresh on ANY diagnostic traffic, not just an explicit TesterPresent ping. */
    .message_received_notify  = FBL_boot_delay_reset,
    .tester_present_notify    = FBL_boot_delay_reset,
};

/* fbl_config_st's uds_init - called from inside FBL_init(), right after pdur_init (see FBL.c). */
STATIC void uds_init( void )
{
    UDS_init( &fbl_uds_func_table_s,
              UDS_get_service_table(), UDS_get_service_table_size(),
              UDS_get_session_table(), UDS_get_session_table_size(),
              pdur_buffer_s, PDUR_BUFFER_SIZE );

    /* FBL is always in PROGRAMMING session - being in FBL space at all means we're programming */
    UDS_set_session( UDS_SES_PROGRAMMING );

    /* APP already required security to be unlocked (in EXTENDED session) before it would honour
       the request that got us here - see uds_handle_session_control(). Trust that and start
       unlocked so the tester need not authenticate twice before RequestDownload/TransferData
       work. This only covers the generic required_sec_level check in the service tables (e.g.
       ROUTINE_ID_ERASE_MEMORY) - FBL_download_request()'s own separate security check is granted
       in FBL_init() itself, see its comment (this runs too early: FBL_init() clears and
       re-initialises fbl_context_s right after uds_init() returns, which would wipe anything set
       here). */
    UDS_set_security_level( 1u );
}

/***************************************************************************************************
**                              Field Bootloader Configuration                                    **
***************************************************************************************************/
/* Caller-owned staging buffer for sector writes - STM32F103 medium-density page size (1KB). */
STATIC u8_t fbl_transfer_sector_buffer_s[ FLS_STM32F1_PAGE_SIZE ];

const fbl_config_st fbl_config_s =
{
    /* Initialisation functions */
    .clk_init        = clk_init,
    .wdg_init        = NULL_P,  /* APP controls the watchdog, same convention as BM */
    .can_init        = can_init,
    .cantp_init      = cantp_init,
    .pdur_init       = pdur_init,
    .uds_init        = uds_init,
    .crc_init        = crc_init,
    .flash_init      = FLS_STM32F1_init,
    .shared_ram_init = SHARED_RAM_init,
    .systick_init    = systick_init,
    .display_init    = display_init,

    /* Runtime function pointers */
    .wdg_kick_func_p           = NULL_P,
    .cantp_tick_func_p         = fbl_cantp_tick,
    .time_get_tick_func_p      = TIME_get_cumulative_run_time_ms,
    .flash_erase_sector_func_p = FLS_STM32F1_erase_sector,
    .flash_write_data_func_p   = FLS_STM32F1_write_data,
    .uds_tx_func_p             = fbl_pdur_tx_uds,
    .crc_calculate_func_p      = CHKSUM_calc_hw_crc32,
    .display_update_func_p     = display_render,
    .erase_complete_func_p     = UDS_erase_complete_notify,   /* answers the deferred 0x31 */
    .security_generate_seed_func_p = NULL_P,  /* no HSM/RNG wired on this target - falls back to
                                                  FBL_security_generate_seed()'s tick-based seed */
    .security_calculate_key_func_p = NULL_P,  /* no customer-specific algorithm wired on this
                                                  target - falls back to FBL_security_verify_key()'s
                                                  XOR-mask algorithm */

    /* Shared RAM interface - same module/contract BM already uses */
    .shared_ram_get_request_func_p = SHARED_RAM_get_fbl_request,
    .shared_ram_set_request_func_p = SHARED_RAM_set_fbl_request,
    .shared_ram_is_valid_func_p    = SHARED_RAM_is_valid,

    /* Configuration values */
    .flash_sector_size          = FLS_STM32F1_PAGE_SIZE,
    .max_transfer_block_len     = FLS_STM32F1_PAGE_SIZE,
    .transfer_sector_buffer_p   = fbl_transfer_sector_buffer_s,
    .transfer_sector_buffer_len = FLS_STM32F1_PAGE_SIZE,
    .app_header_address         = (u32_t)&__app_header_start__,
    .app_code_end_address       = (u32_t)&__app_code_end__,
};

/****************************** END OF FILE *******************************************************/
