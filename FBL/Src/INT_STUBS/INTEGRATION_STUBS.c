/*! \file
*               Author: mstewart
*   \brief      FBL (Field Bootloader) platform wiring for STM32F103C8
*
*   Everything here exists to bind generic modules to this board. Most functions are one-liners
*   that supply something the module's function-pointer signature has no room for: a config struct
*   (clk_init, crc_init). They are adapters, not indirection - deleting one means changing a module's contract for every project
*   that uses it. Where a module's function-pointer type already matches another layer's signature
*   exactly (PDUR_tx, PDUR_rx_indication_physical, PDUR_tx_confirmation_physical, and the whole CAN
*   stack through CANIF), it's bound directly below with no adapter at all.
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
#include "CANIF.h"
#include "PDUR.h"
#include "UDS_config.h"
#include "HAL_I2C.h"
#include "SH1106.h"
#include "VER.h"
#include "printf.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/***************************************************************************************************
**                              External Symbols from Linker                                     **
***************************************************************************************************/
extern u32_t __app_header_start__;
extern u32_t __app_code_end__;
extern u32_t __dataset0_header_start__;
extern u32_t __dataset0_code_end__;
extern u32_t __dataset1_header_start__;
extern u32_t __dataset1_code_end__;

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

/* All four blocks FBL registers - IDs, struct shapes, RAM mirrors and NVM_GEN2_block_cfg_st
   configs (fbl_nvm_gen2_fingerprint_block_s / _boot_count_block_s / _download_attempt_count_
   block_s / _dataset_download_count_block_s) - live in FBL_NVM_BLOCKS.h/.c, not here. None of
   that touches hardware, so none of it belongs in board wiring; only the NVM_GEN2_hw_interface_st
   above and the register_block() calls in fbl_board_init() below are genuinely this board's
   concern. APP reads the fingerprint via NVM_GEN2_read_block( FBL_FINGERPRINT_BLOCK_ID, ... ) and
   needs the ID and struct shape (also in FBL_NVM_BLOCKS.h) to decode it. */

/* fbl_config_st's download_attempt_notify_func_p - called once per accepted 0x34 RequestDownload,
   see FBL_download_attempt_notify_func_t's comment in FBL.h. Flushes immediately for the same
   reason fbl_uds_handle_write_fingerprint() (FBL/Src/UDS_CFG/UDS_config.c) does: FBL never ticks
   NVM_GEN2 in the background, so a request left merely pending here would never actually reach
   flash this boot. */
STATIC void fbl_download_attempt_notify( void )
{
    fbl_download_attempt_count_g.count++;
    NVM_GEN2_flush_block( FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID );
}

/* fbl_config_st's dataset_download_notify_func_p - called once per accepted 0x34 RequestDownload
   that targets a dataset region, alongside (not instead of) fbl_download_attempt_notify() above.
   index is always < FBL_DATASET_DOWNLOAD_COUNT_MAX in practice (FBL.c only ever passes a valid
   dataset_table_p index), but bounds-checked anyway since this writes flash - never trust an index
   into a fixed-size array without checking, even one this codebase itself computed. */
STATIC void fbl_dataset_download_notify( u8_t index )
{
    if( index < FBL_DATASET_DOWNLOAD_COUNT_MAX )
    {
        fbl_dataset_download_count_g.count[index]++;
        NVM_GEN2_flush_block( FBL_DATASET_DOWNLOAD_COUNT_BLOCK_ID );
    }
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
       needs to reach flash does so via an explicit flush (fbl_uds_handle_write_fingerprint() in
       FBL/Src/UDS_CFG/UDS_config.c, fbl_download_attempt_notify(), and the boot-count increment
       below) - an interrupted compaction is discarded at mount, never resumed, so there is no
       other NVM work for FBL to do in the background. */
    NVM_GEN2_register_block( FBL_FINGERPRINT_BLOCK_ID, &fbl_nvm_gen2_fingerprint_block_s );
    NVM_GEN2_register_block( FBL_BOOT_COUNT_BLOCK_ID, &fbl_nvm_gen2_boot_count_block_s );
    NVM_GEN2_register_block( FBL_DOWNLOAD_ATTEMPT_COUNT_BLOCK_ID, &fbl_nvm_gen2_download_attempt_count_block_s );
    NVM_GEN2_register_block( FBL_DATASET_DOWNLOAD_COUNT_BLOCK_ID, &fbl_nvm_gen2_dataset_download_count_block_s );

    /* Unconditional, once per boot - this is literally what "boot count" means. Flushed
       immediately rather than left pending for the same reason as the two writers above. */
    fbl_boot_count_g.count++;
    NVM_GEN2_flush_block( FBL_BOOT_COUNT_BLOCK_ID );
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
    char  line[SH1106_MAX_CHARS_PER_LINE + 1u];
    u8_t  sw_version[SW_VERSION_NUM_SIZE];

    SH1106_buffer_clear( &fbl_display_instance_s );

    /* VER_init() (called from FBL_init(), before board_init()/display_init() run - see FBL.c)
       has already populated this from autoversion.h, same source as the 0xF180 boot-software-ID
       UDS response (FBL_uds_handle_read_boot_sw_id()). */
    VER_get_sw_version_num( sw_version );
    (void)PRINTF_snprintf( (u8_t*)line, (u16_t)sizeof( line ), "BOOTLOADER v%u.%u.%u",
                            (unsigned int)sw_version[0], (unsigned int)sw_version[1], (unsigned int)sw_version[2] );
    SH1106_buffer_write_string( &fbl_display_instance_s, 1u, 0u, line, FALSE );

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

STATIC TJA1051_instance_st fbl_tja1051_instance_s;

/***************************************************************************************************
**                              CAN / CAN-TP / PDU-R                                              **
***************************************************************************************************/
/* Deliberately NOT a designated initializer - see the identical comment on app_cantp_instance_s in
   APP/Src/INT_STUBS/INTEGRATION_STUBS.c: CANTP_instance_st embeds ~1.7KB of near-all-zero RX/TX
   queue and TP session arrays, and any non-zero designated field forces the compiler to store the
   WHOLE struct as flash-resident .data. On FBL's flash budget that is the difference between
   fitting and not. Zero-initialised in .bss, populated at runtime in fbl_comms_init(). */
STATIC CANTP_instance_st fbl_cantp_instance_s;

/* The one message buffer CANTP reassembles requests into and UDS builds responses in. Sized for
   FBL's largest request, a full TransferData: SID + blockSequenceCounter + max_transfer_block_len
   of data - enough whether the tester reads maxNumberOfBlockLength as covering those two header
   bytes (ISO 14229-1) or not. Nothing FBL answers is longer. */
#define FBL_UDS_BUFFER_SIZE     ( FLS_STM32F1_PAGE_SIZE + 2u )
STATIC u8_t fbl_uds_buffer_s[ FBL_UDS_BUFFER_SIZE ];

/* No adapter functions between the CAN layers - same wiring as APP, every signature lines up:
   - HAL_CAN RX ISR -> CANIF_rx_indication() (HAL_CAN_set_rx_callback() in fbl_comms_init()), CANIF ->
     fbl_cantp_instance_s.
   - CANTP -> PDUR: rx_indication_func_p is PDUR_rx_indication_physical(), TP completion goes through
     PDUR_tx_confirmation_physical().
   - PDUR -> CANTP: both routes point at CANIF_pdur_tp_tx (always ISO-TP framed - UDS request/response).
   - CANTP -> driver: can_tx_func_p is CANIF_transmit.
   FBL has one bus, so every channel involved is 0 (the CANTP instance's and every route's default). */

/* The only IDs FBL receives on - the two UDS request IDs. CANIF_rx_indication() drops every other ID
   before it reaches CANTP. Without it, foreign frames share CANTP's 10-deep RX queue with a 10-frame
   TransferData block (rx_block_size below) - one of them mid-block costs a Consecutive Frame and
   fails the download. */
STATIC const u32_t fbl_can_rx_ids_s[] = { FBL_UDS_REQUEST_ID, FBL_CAN_RX_ID };

/* ctrl_set_mode/ctrl_get_status/trcv_set_mode deliberately left NULL_P: FBL never changes controller
   or transceiver mode at runtime (TJA1051_init() puts the transceiver in NORMAL once), and naming
   them would link HAL_CAN_set_mode/get_status, their SPL helpers and TJA1051_canif_set_mode - ~400 B
   of flash for nothing. */
STATIC const CANIF_channel_cfg_st fbl_canif_channels_s[] =
{
    { .send_frame = HAL_CAN_send_frame,
      .rx_ids_p   = fbl_can_rx_ids_s,
      .num_rx_ids = (u8_t)( sizeof( fbl_can_rx_ids_s ) / sizeof( fbl_can_rx_ids_s[0] ) ),
      .cantp_p    = &fbl_cantp_instance_s },
};

/* fbl_config_st's comms_tick_func_p - CAN-TP must pump its queue before UDS_tick() can see
   anything it finished reassembling this cycle, so this owns the order rather than exposing two
   separately-orderable slots. */
STATIC void fbl_comms_tick( void )
{
    CANIF_tick();
    UDS_tick();
}

/* FBL's own logical PDU IDs - these, not FBL_UDS_REQUEST_ID/FBL_CAN_RX_ID etc., are what
   fbl_pdur_routing_table_s's array position means and what PDUR_tx() dispatches on. Values
   double as array indices (designated-index initializers below pin each route to its enum value
   explicitly, so reordering this enum without reordering the table - or vice versa - is a compile
   error from a duplicate/out-of-range index, not a silent mismatch). */
typedef enum
{
    FBL_PDU_UDS_FUNCTIONAL = 0u,  /* rx: FBL_UDS_REQUEST_ID (0x700) / tx: FBL_UDS_RESPONSE_ID (0x600) */
    FBL_PDU_UDS_PHYSICAL,         /* rx: FBL_CAN_RX_ID (0x7E0) / tx: FBL_CAN_TX_ID (0x7E8) */
    FBL_PDU_NUM_ROUTES
} fbl_pdu_id_et;

/* Bidirectional routes: functional (0x700->0x600) and physical (0x7E0->0x7E8). TX goes via CANTP
   (CANIF_pdur_tp_tx, always TP-framed) so multi-frame UDS responses get segmented. UDS_tx_confirmation tells UDS each response has actually gone out - it holds the
   shared buffer and any scheduled reset/APP jump until then. */
STATIC const PDUR_route_st fbl_pdur_routing_table_s[] =
{
    [FBL_PDU_UDS_FUNCTIONAL] = { .rx_id = FBL_UDS_REQUEST_ID, .tx_id = FBL_UDS_RESPONSE_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = CANIF_pdur_tp_tx,
      .txConfirmation = UDS_tx_confirmation },
    [FBL_PDU_UDS_PHYSICAL] = { .rx_id = FBL_CAN_RX_ID, .tx_id = FBL_CAN_TX_ID,
      .upperLayerRxIndication = UDS_rx_indication, .lower_layer_tx_func = CANIF_pdur_tp_tx,
      .txConfirmation = UDS_tx_confirmation },
};

/* UDS_init_cfg_st.is_functional_route_fp - see there */
STATIC false_true_et fbl_uds_is_functional_route( UDS_route_id_t route_id )
{
    return( ( route_id == (UDS_route_id_t)FBL_PDU_UDS_FUNCTIONAL ) ? TRUE : FALSE );
}

/* UDS_init_cfg_st.tp_send_func_p is assigned PDUR_tx directly below - PDUR_pdu_id_t and
   UDS_route_id_t are both u16_t and PDUR_tx's pass_fail_et return now matches the typedef exactly
   (see UDS_init_cfg_st's comment in UDS.h), so no adapter is needed. route_id is whatever
   UDS_rx_indication() was called with for the request this response answers (UDS.c just stores and
   returns it, see UDS_ctrl_st.req_route_id), so a request received via FBL_PDU_UDS_PHYSICAL gets
   its reply sent via FBL_PDU_UDS_PHYSICAL too, not a single fixed route. */

/***************************************************************************************************
**                              UDS                                                               **
***************************************************************************************************/
/* .perform_soft_reset/.perform_hard_reset and .tester_present_notify used to live in a
   UDS_func_p_st here - both moved into UDS_config.c service table rows (ecu_reset_cfg_s /
   tester_present_cfg_s, SID 0x11 / 0x3E - see UDS_ecu_reset_cfg_st/UDS_tester_present_cfg_st in
   UDS.h). FBL manages its own 30s boot delay rather than using UDS.c's S3 timeout (there is no
   s3_timeout_notify hook any more). tp_send_func_p/message_received_notify (PDUR_tx /
   FBL_reset_auto_boot_timer - the latter covers the "any SID" half of keeping FBL's boot-delay
   timer fresh; TesterPresent's own half is tester_present_cfg_s above) are now assigned directly
   in fbl_uds_init_cfg_s below instead of a separate wrapper object - see UDS_init_cfg_st's comment
   in UDS.h for why that wrapper went away. */

/* fbl_config_st's comms_init - one slot for the whole stack instead of four, since CAN-TP needs a
   working CAN driver, PDUR needs CAN-TP, and UDS needs PDUR: a strict dependency chain, not four
   independently optional stages, so nothing is lost by not exposing them separately. */
STATIC void fbl_comms_init( void )
{
    GPIO_InitTypeDef gpio_init;
    pass_fail_et     canif_ok;
    pass_fail_et     pdur_ok;

    /* CAN: TJA1051 EN pin - push-pull output, driven via tja1051_en_pin_set() */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
    gpio_init.GPIO_Pin   = TJA1051_EN_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init( TJA1051_EN_PORT, &gpio_init );

    TJA1051_init( &fbl_tja1051_instance_s, &tja1051_func_table_s, &tja1051_config_s );

    HAL_CAN_init();

    /* CAN-TP: compound-literal assignment for readability. Still .bss, not .data:
       fbl_cantp_instance_s's own declaration above has no initializer, so this assignment doesn't
       change where the object itself lives - only how the initial values get written into it. */
    fbl_cantp_instance_s = (CANTP_instance_st)
    {
        .rx_indication_func_p    = PDUR_rx_indication_physical,
        .can_tx_func_p           = CANIF_transmit,
        .rx_buffer_p             = fbl_uds_buffer_s,
        .rx_buffer_size          = FBL_UDS_BUFFER_SIZE,
        .tp_ids                  = { { FBL_UDS_REQUEST_ID, FBL_UDS_RESPONSE_ID },
                                     { FBL_CAN_RX_ID,       FBL_CAN_TX_ID       } },
        .st_min                  = 0x01u,
        .rx_block_size           = 10u,     /* 0 = unlimited (ISO 15765-2) - CANTP_init() no longer
                                              rewrites this to a default, so it genuinely means one
                                              Flow Control per TransferData PDU instead of one every
                                              N frames. */
        .N_Cr                    = CANTP_DEFAULT_N_CR,
        .N_Bs                    = CANTP_DEFAULT_N_BS,
    };

    CANTP_init( &fbl_cantp_instance_s );

    /* FAIL is a wiring mistake in fbl_canif_channels_s */
    canif_ok = CANIF_init( fbl_canif_channels_s, (u8_t)( sizeof( fbl_canif_channels_s ) / sizeof( fbl_canif_channels_s[0] ) ) );
    STDC_assert( canif_ok == PASS );

    /* Only once CANTP's queues are initialised - a frame delivered mid-init could be lost */
    HAL_CAN_set_rx_callback( 0u, CANIF_rx_indication );

    /* PDU Router */
    /* FAIL means a route with neither direction configured - a table typo, caught at boot */
    pdur_ok = PDUR_init( fbl_pdur_routing_table_s, (u16_t)( sizeof( fbl_pdur_routing_table_s ) / sizeof( PDUR_route_st ) ) );
    STDC_assert( pdur_ok == PASS );

    /* UDS - UDS_get_service_table() supplies every SID FBL answers, including the SessionControl
       (0x10) and SecurityAccess (0x27) rows UDS.c still dispatches specially (see
       UDS_service_table_st's comment in UDS.h) */
    /* FBL is always in PROGRAMMING session - being in FBL space at all means we're programming.
       Likewise starts unlocked (.initial_security_level): APP already required security to be
       unlocked (in EXTENDED session) before it would honour the request that got us here - see
       uds_handle_session_control(). Trust that so the tester need not authenticate twice before
       RequestDownload/TransferData work - this is the ONLY "starts unlocked" flag FBL has; there
       is no separate FBL-side unlock to keep in sync with it (see FBL_download_request()'s
       comment, FBL.c). */
    const UDS_init_cfg_st fbl_uds_init_cfg_s =
    {
        .tp_send_func_p          = PDUR_tx,
        .message_received_notify = FBL_reset_auto_boot_timer,
        .service_table_p         = UDS_get_service_table(),
        .service_table_size      = UDS_get_service_table_size(),
        .buffer_p                = fbl_uds_buffer_s,
        .buffer_size             = FBL_UDS_BUFFER_SIZE,
        .initial_session         = UDS_SES_PROGRAMMING,
        .initial_security_level  = 1u,
        .tick_period_ms          = FBL_TIMER_TICK_RATE_MS,
        .is_functional_route_fp  = fbl_uds_is_functional_route,
        .security_max_attempts   = FBL_SECURITY_MAX_FAILED_ATTEMPTS,
        .security_lockout_ms     = FBL_SECURITY_LOCKOUT_TIME_MS
    };
    UDS_init( &fbl_uds_init_cfg_s );
}

/***************************************************************************************************
**                              Field Bootloader Configuration                                    **
***************************************************************************************************/
/* Caller-owned staging buffer for sector writes - STM32F103 medium-density page size (1KB). */
STATIC u8_t fbl_transfer_sector_buffer_s[ FLS_STM32F1_PAGE_SIZE ];

/* Two independently downloadable/erasable 1KB data blocks - see fbl_is_region_valid() in FBL.c.
   Add/remove entries here (and the matching extern symbols above, defined in this project's own
   linker script) to change how many datasets this platform has - FBL.c itself has no hardcoded
   count. Must match bm_dataset_table_s in BM/Src/bm_main.c entry-for-entry. */
STATIC const FBL_dataset_region_st fbl_dataset_table_s[] =
{
    { .header_address = (u32_t)&__dataset0_header_start__, .code_end_address = (u32_t)&__dataset0_code_end__ },
    { .header_address = (u32_t)&__dataset1_header_start__, .code_end_address = (u32_t)&__dataset1_code_end__ },
};

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
    .flash_erase_sector_func_p = FLS_STM32F1_erase_sector,
    .flash_write_data_func_p   = FLS_STM32F1_write_data,
    .crc_calculate_func_p      = CHKSUM_calc_hw_crc32,
    .display_update_func_p     = display_render,

    .reprog_request_clear_func_p    = SHARED_RAM_set_fbl_request,
    .download_attempt_notify_func_p = fbl_download_attempt_notify,
    .dataset_download_notify_func_p = fbl_dataset_download_notify,

    /* Configuration values */
    .flash_sector_size          = FLS_STM32F1_PAGE_SIZE,
    .max_transfer_block_len     = FLS_STM32F1_PAGE_SIZE,
    .transfer_sector_buffer_p   = fbl_transfer_sector_buffer_s,
    .transfer_sector_buffer_len = (uint32_t)sizeof( fbl_transfer_sector_buffer_s ),
    .app_header_address         = (u32_t)&__app_header_start__,
    .app_code_end_address       = (u32_t)&__app_code_end__,
    .dataset_table_p            = fbl_dataset_table_s,
    .dataset_table_size         = (u8_t)( sizeof( fbl_dataset_table_s ) / sizeof( fbl_dataset_table_s[0] ) ),
};

/****************************** END OF FILE *******************************************************/
