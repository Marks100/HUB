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
#include "FBL_NVM_BLOCKS.h"
#include "CHKSUM.h"
#include "CLK_STM32F1.h"
#include "SHARED_RAM.h"
#include "FLS_STM32F1.h"
#include "NVM_GEN2.h"
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
const TIME_cfg_st time_cfg_s = 
{
    .time_increment_ms = 1u 
};

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
**                              NVM / Fingerprint                                                 **
***************************************************************************************************/
/* Both reserved pages (NVM_BASE_ADDRESS/NVM_TOTAL_SIZE, PROJ_config.h), handed to NVM_GEN2 as its
   two partitions. FBL and APP deliberately point at the SAME two pages and no longer take a page
   each: NVM_GEN2 packs blocks into a shared, append-only log keyed by block ID instead of giving
   each block a page of its own, the two images own separate ID ranges (NVM_GEN2_BLOCK_ID_FBL_*
   here, _APP_* over in APP), and compaction carries any record it has no config for across
   verbatim. So APP compacting can never delete this fingerprint, and FBL compacting can never
   delete APP's blocks. See PROJ_config.h's NVM_BASE_ADDRESS comment and NVM_GEN2/README.md for
   the full argument. */
STATIC const NVM_GEN2_hw_interface_st fbl_nvm_gen2_hw_interface_s =
{
    .init_func          = FLS_STM32F1_init,
    .base_address       = NVM_BASE_ADDRESS,
    .partition_size     = FLS_STM32F1_PAGE_SIZE,
    .total_size         = NVM_TOTAL_SIZE,
    .erase_func         = FLS_STM32F1_erase_sector,
    .write_func         = FLS_STM32F1_write_data,
    .compare_func       = FLS_STM32F1_compare_data,
    .read_func          = FLS_STM32F1_recover_data
};

/* All three blocks FBL registers - IDs, struct shapes, RAM mirrors and NVM_GEN2_block_cfg_st
   configs (fbl_nvm_gen2_fingerprint_block_s / _boot_count_block_s / _download_attempt_count_
   block_s) - live in FBL_NVM_BLOCKS.h/.c, not here. None of that touches hardware, so none of it
   belongs in board wiring; only the NVM_GEN2_hw_interface_st above and the register_block() calls
   in fbl_board_init() below are genuinely this board's concern. APP reads the fingerprint via
   NVM_GEN2_read_block( FBL_FINGERPRINT_BLOCK_ID, ... ) and needs the ID and struct shape (also in
   FBL_NVM_BLOCKS.h) to decode it. */

/* fbl_config_st's fingerprint_write_func_p. Blocks via NVM_GEN2_write_block_now() rather than
   leaving the request for the next periodic tick - the whole point of writing now is durability
   before the tester proceeds to erase/download/reset, not background persistence. Blocks for one
   small write, or for a full compaction if the log happens to be full. */
STATIC void fbl_fingerprint_write( const u8_t* data_p, u8_t len )
{
    fbl_fingerprint_g.len = len;
    STDC_memcpy( fbl_fingerprint_g.data, data_p, len );
    fbl_fingerprint_g.flash_count++;
    fbl_fingerprint_g.last_flash_timestamp_ms         = TIME_get_cumulative_run_time_ms_u32();
    fbl_fingerprint_g.boot_count_at_flash             = fbl_boot_count_g.count;
    fbl_fingerprint_g.download_attempt_count_at_flash = fbl_download_attempt_count_g.count;
    NVM_GEN2_write_block_now( FBL_FINGERPRINT_BLOCK_ID );
}

/* fbl_config_st's download_attempt_notify_func_p - called once per accepted 0x34 RequestDownload,
   see FBL_download_attempt_notify_func_t's comment in FBL.h. Flushes immediately for the same
   reason fbl_fingerprint_write() does: FBL never ticks NVM_GEN2 in the background, so a request
   left merely pending here would never actually reach flash this boot. */
STATIC void fbl_download_attempt_notify( void )
{
    fbl_download_attempt_count_g.count++;
    NVM_GEN2_write_block_now( FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID );
}

/* fbl_config_st's board_init - flash driver bring-up, shared RAM bring-up and NVM bring-up are
   independent of each other (order between them does not matter), but shared RAM must be up
   before reprog_request_clear_func_p runs, so all three are combined into that one early slot. */
STATIC void fbl_board_init( void )
{
    FLS_STM32F1_init();
    SHARED_RAM_init();
    NVM_GEN2_init( &fbl_nvm_gen2_hw_interface_s );

    /* Registration leaves a write pending if no record exists yet for a given block, and FBL
       deliberately never runs NVM_GEN2_tick() periodically to service it - the only thing that
       would achieve is burning a record on empty data at every virgin boot. Everything here that
       needs to reach flash does so via an explicit flush (fbl_fingerprint_write(),
       fbl_download_attempt_notify(), and the boot-count increment below) - an interrupted
       compaction is discarded at mount, never resumed, so there is no other NVM work for FBL to
       do in the background. */
    NVM_GEN2_register_block( FBL_FINGERPRINT_BLOCK_ID, &fbl_nvm_gen2_fingerprint_block_s );
    NVM_GEN2_register_block( FBL_BOOT_COUNT_BLOCK_ID, &fbl_nvm_gen2_boot_count_block_s );
    NVM_GEN2_register_block( FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID, &fbl_nvm_gen2_download_attempt_count_block_s );

    /* Unconditional, once per boot - this is literally what "boot count" means. Flushed
       immediately rather than left pending for the same reason as the two writers above. */
    fbl_boot_count_g.count++;
    NVM_GEN2_write_block_now( FBL_BOOT_COUNT_BLOCK_ID );
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
    display_render( 0u, (u8_t)FBL_BOOT_DELAY_S );
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
   fitting and not. Zero-initialised in .bss, populated at runtime in fbl_comms_init(). */
STATIC CANTP_instance_st fbl_cantp_instance_s;

/* Unpacks CANTP's message struct into HAL_CAN's argument list. */
STATIC void fbl_cantp_send( CANTP_can_msg_format_st* msg_p )
{
    (void)HAL_CAN_send_frame( msg_p->Id, (u8_t)msg_p->id_type, msg_p->Data, msg_p->DLC );
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

/* fbl_config_st's comms_tick_func_p - CAN-TP must pump its queue before UDS_tick() can see
   anything it finished reassembling this cycle, so this owns the order rather than exposing two
   separately-orderable slots. */
STATIC void fbl_comms_tick( void )
{
    CANTP_tick( &fbl_cantp_instance_s );
    UDS_tick();
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

/* Supplies the response CAN ID, which UDS's tp_send_func_p signature has no room for. */
STATIC void fbl_pdur_tx_uds( u8_t* data_p, u16_t len )
{
    PDUR_tx( FBL_UDS_RESPONSE_ID, 0u, data_p, len );
}

/***************************************************************************************************
**                              UDS                                                               **
***************************************************************************************************/
const UDS_func_p_st fbl_uds_func_table_s =
{
    .tp_send_func_p           = fbl_pdur_tx_uds,
    .perform_soft_reset       = MCU_JUMP_software_reset,
    .perform_hard_reset       = MCU_JUMP_software_reset,
    .s3_timeout_notify        = NULL_P,   /* FBL manages its own 30s boot delay instead */
    .session_change_notify    = NULL_P,
    /* fbl_run_auto_boot_timer() (FBL.c) already pauses the countdown itself for the duration of an
       erase or download, so this is only for the genuinely-idle case: a tester that has an active
       session but takes its time between separate commands. Wiring both notifies to the reset
       keeps the countdown fresh on ANY diagnostic traffic in that window, not just an explicit
       TesterPresent ping. */
    .message_received_notify  = FBL_reset_auto_boot_timer,
    .tester_present_notify    = FBL_reset_auto_boot_timer,
};

/* fbl_config_st's comms_init - one slot for the whole stack instead of four, since CAN-TP needs a
   working CAN driver, PDUR needs CAN-TP, and UDS needs PDUR: a strict dependency chain, not four
   independently optional stages, so nothing is lost by not exposing them separately. */
STATIC void fbl_comms_init( void )
{
    GPIO_InitTypeDef gpio_init;

    /* CAN: TJA1051 EN pin - push-pull output, driven via tja1051_en_pin_set() */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
    gpio_init.GPIO_Pin   = TJA1051_EN_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init( TJA1051_EN_PORT, &gpio_init );

    TJA1051_init( &tja1051_func_table_s, &tja1051_config_s );

    HAL_CAN_init();
    HAL_CAN_set_rx_callback( fbl_can_rx );

    /* CAN-TP: compound-literal assignment for readability. Still .bss, not .data:
       fbl_cantp_instance_s's own declaration above has no initializer, so this assignment doesn't
       change where the object itself lives - only how the initial values get written into it. */
    fbl_cantp_instance_s = (CANTP_instance_st)
    {
        .CANTP_message_rx_func_p = fbl_cantp_rx_indication,
        .tx_func_p               = fbl_cantp_send,
        .tp_buffer               = pdur_buffer_s,
        .tp_ids                  = { FBL_UDS_REQUEST_ID, FBL_CAN_RX_ID },
        .st_min                  = 0x03,
        .rx_block_size           = 10u,     /* 0 = unlimited (ISO 15765-2) - CANTP_init() no longer
                                              rewrites this to a default, so it genuinely means one
                                              Flow Control per TransferData PDU instead of one every
                                              N frames. */
        .N_Cr                    = CANTP_DEFAULT_N_CR,
        .N_Bs                    = CANTP_DEFAULT_N_BS,
        .N_Ar                    = CANTP_DEFAULT_N_AR,
        .uds_req_id              = FBL_UDS_REQUEST_ID,
        .uds_resp_id             = FBL_UDS_RESPONSE_ID,
    };

    CANTP_init( &fbl_cantp_instance_s );

    /* PDU Router */
    PDUR_init( fbl_pdur_routing_table_s, (u16_t)( sizeof( fbl_pdur_routing_table_s ) / sizeof( PDUR_rx_route_st ) ) );

    /* UDS */
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
       re-initialises fbl_context_s right after fbl_comms_init() returns, which would wipe anything
       set here). */
    UDS_set_security_level( 1u );
}

/* Placeholder seed/key algorithm - not real challenge/response security, just enough to exercise
   the UDS exchange end to end. STM32F103 (medium-density) has no hardware RNG peripheral, so the
   seed is tick-based rather than truly random; TIME_get_cumulative_run_time_ms() is the same
   public time source already wired as time_get_tick_func_p, not FBL's own private tick counter.
   The XOR mask matches Tool_cfg/CANFLASH/seedkeydll/seedkey.cpp's SECURITY_KEY_XOR_MASK exactly -
   change both together if this is ever replaced with a real customer/OEM algorithm. */
#define FBL_SECURITY_SEED_MULTIPLIER (0x12345678u)
#define FBL_SECURITY_KEY_XOR_MASK    (0xA5A5A5A5u)

STATIC u32_t fbl_security_generate_seed( void )
{
    return( (u32_t)TIME_get_cumulative_run_time_ms() * FBL_SECURITY_SEED_MULTIPLIER );
}

STATIC u32_t fbl_security_calculate_key_placeholder( u32_t seed )
{
    return( seed ^ FBL_SECURITY_KEY_XOR_MASK );
}

/* One row per documented level (FBL_security_level_key_st - see FBL.h). All four point at the same
   placeholder function today, but each is independently repointable: giving level 3 a real,
   different algorithm later means changing this one row, not touching FBL_security_verify_key()
   or any other level. A RequestSeed for a level with no row here (FBL's UDS_config.c accepts any
   odd value) always fails SendKey - see fbl_security_find_level_key_entry() in FBL.c. */
STATIC const FBL_security_level_key_st fbl_security_level_key_table_s[] =
{
    { 0x01u, fbl_security_calculate_key_placeholder },  /* Level 1 */
    { 0x03u, fbl_security_calculate_key_placeholder },  /* Level 2 */
    { 0x05u, fbl_security_calculate_key_placeholder },  /* Level 3 */
    { 0x07u, fbl_security_calculate_key_placeholder },  /* Level 4 */
    { 0x09u, fbl_security_calculate_key_placeholder },  /* Level 5 */
    { 0x0Bu, fbl_security_calculate_key_placeholder },  /* Level 6 */
};

/***************************************************************************************************
**                              Field Bootloader Configuration                                    **
***************************************************************************************************/
/* Caller-owned staging buffer for sector writes - STM32F103 medium-density page size (1KB). */
STATIC u8_t fbl_transfer_sector_buffer_s[ FLS_STM32F1_PAGE_SIZE ];

const fbl_config_st fbl_config_s =
{
    /* Initialisation functions */
    .clk_init        = clk_init,
    .wdg_init        = NULL_P,
    .comms_init      = fbl_comms_init,
    .crc_init        = crc_init,
    .board_init      = fbl_board_init,
    .systick_init    = systick_init,
    .display_init    = display_init,

    /* Runtime function pointers */
    .wdg_kick_func_p           = NULL_P,
    .comms_tick_func_p         = fbl_comms_tick,
    .time_get_tick_func_p      = TIME_get_cumulative_run_time_ms,
    .flash_erase_sector_func_p = FLS_STM32F1_erase_sector,
    .flash_write_data_func_p   = FLS_STM32F1_write_data,
    .crc_calculate_func_p      = CHKSUM_calc_hw_crc32,
    .display_update_func_p     = display_render,
    .security_generate_seed_func_p    = fbl_security_generate_seed,  /* placeholder tick-based seed,
                                                                          see its comment */
    .security_level_key_table_p       = fbl_security_level_key_table_s,
    .security_level_key_table_size    = (u8_t)( sizeof( fbl_security_level_key_table_s ) /
                                                 sizeof( fbl_security_level_key_table_s[0] ) ),

    .reprog_request_clear_func_p    = SHARED_RAM_set_fbl_request,
    .fingerprint_write_func_p       = fbl_fingerprint_write,
    .download_attempt_notify_func_p = fbl_download_attempt_notify,

    /* Configuration values */
    .flash_sector_size          = FLS_STM32F1_PAGE_SIZE,
    .max_transfer_block_len     = FLS_STM32F1_PAGE_SIZE,
    .transfer_sector_buffer_p   = fbl_transfer_sector_buffer_s,
    .transfer_sector_buffer_len = (uint32_t)sizeof( fbl_transfer_sector_buffer_s ),
    .app_header_address         = (u32_t)&__app_header_start__,
    .app_code_end_address       = (u32_t)&__app_code_end__,
};

/****************************** END OF FILE *******************************************************/
