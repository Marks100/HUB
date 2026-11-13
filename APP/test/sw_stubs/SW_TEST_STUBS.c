/*! \file
*               Author: mstewart
*   \brief      Software tests - how every test starts the ECU
*
*   \details    CAN1 reaches its bus through the TJA1051 model, which the firmware has to take out of standby (EN pin,
*               TJA1051_init) before a frame passes either way; CAN2 is the MCP2515 on SPI1 and its chip model. The
*               symbols the linker script would give the APP are here too, as addresses in the virtual flash.
*/

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "HAL_config.h"
#include "vhw.h"
#include "stm32f10x_can.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "TMD_TIME.h"
#include "TMD_SYSTICK.h"
#include "TMD_NVM.h"
#include "TMD_PANEL.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "FLS_STM32F1.h"
#include "TMD_INIT.h"
#include "TMD_CAN.h"
#include "TMD_CANTP.h"
#include "TMD_UDS.h"
#include "TMD_TJA1051.h"
#include "TMD_MCP2515.h"
#include "TMD_STARTUP.h"
#include "TMD_MBA.h"
#include "VPM_config.h"
#include "NVM_GEN2.h"
#include "FBL_NVM_BLOCKS.h"
#include "TMD_SH1106.h"
#include "SH1106.h"
#include "BOOT_HANDOFF.h"
#include "SW_TEST_STUBS.h"
#include "INTEGRATION_STUBS.h"

/***************************************************************************************************
**                                    Exported Globals                                           **
***************************************************************************************************/
extern void app_main( void );                      /* main.c: the ECU's start, it returns */

/* The symbols APP's linker script defines, as the addresses they have there: DATASET0's payload is in the virtual flash, where a
   WriteMemoryByAddress puts it. The assembler defines them, C cannot define an address. */
#define SW_TEST_STR_( x )    #x
#define SW_TEST_STR( x )     SW_TEST_STR_( x )

#if defined( _WIN32 )
    #define SW_TEST_SYMBOL_PREFIX    "_"     /* the host compiler puts an underscore before every C symbol */
#else
    #define SW_TEST_SYMBOL_PREFIX    ""
#endif

u32_t __isr_vector_start = 0u;    /* the start of the vector table, which app_main() hands to the vector table offset register */

__asm__( ".globl " SW_TEST_SYMBOL_PREFIX "__dataset0_code_start__\n"
         ".set "   SW_TEST_SYMBOL_PREFIX "__dataset0_code_start__, " SW_TEST_STR( SW_TEST_DATASET0_PAYLOAD_ADDRESS ) "\n"
         ".globl " SW_TEST_SYMBOL_PREFIX "__dataset0_code_end__\n"
         ".set "   SW_TEST_SYMBOL_PREFIX "__dataset0_code_end__, "   SW_TEST_STR( SW_TEST_DATASET0_END_ADDRESS ) "\n" );

/***************************************************************************************************
**                                  Private Function Prototypes                                   **
***************************************************************************************************/
/* How long the firmware takes to run. Every function it enters costs this long, on average, and the hardware goes on while it runs:
   an interrupt can fall in the middle of a scheduler pass, where its time says it would.
   833 ns is 60 cycles of the STM32F103 at 72 MHz: an estimate for -Os from flash (a call and return are 8 to 12 cycles, the body 15 to
   100 instructions at 1.3 to 1.8 cycles each). Measure it on the board: the time of one scheduler pass (DWT->CYCCNT around
   SysTick_Handler, over the core clock) divided by the function entries one pass makes here (about 230 with nothing going on) -
   and change it here */
#define SW_TEST_NS_PER_ENTRY        ( 833u )

/* The models are ticked at every this many functions the firmware enters; the clock still moves at every one, so the time is the same.
   1 is every function, the finest; more is a faster run, and an interrupt that falls due waits for the next tick */
#define SW_TEST_ENTRIES_PER_TICK    ( 8u )


static const TMD_MBA_cfg_st         sw_test_bootloader_cfg_s =                     /* what the MBA model, the bootloader, knows of the flash and the NVM */
{
    .common_p            = &vpm_cfg_s,
    .page_size           = (u16_t)FLS_STM32F1_PAGE_SIZE,
    .note_applied_func_p = vpm_config_note_applied
};

static void        sw_test_tick_the_models( void );
static void        sw_test_ecu_start( void );
static void        sw_test_power_on( false_true_et blank_nvm );

const TMD_INIT_cfg_st sw_test_init_cfg_g =
{
    .power_on_func_p    = sw_test_power_on,
    .ecu_main_func_p    = sw_test_ecu_start
};

/***************************************************************************************************
**                                  Public Functions                                              **
***************************************************************************************************/

/* None */

/***************************************************************************************************
**                                  Private Functions                                             **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Power on what the ECU runs on: the MCU's hardware and the models of everything around it
*
*   \author        mstewart
*
*   \param[in]     blank_nvm    TRUE: a new ECU, its flash blank. FALSE: a restart, which keeps whatever is in its flash
*
*   \return        none
*
*   \note          Three steps, in this order: the hardware (the firmware's RAM and the virtual STM32), every model (the clock, the bus
*                  models, then what attaches to them: CAN1, bxCAN TXD and RXD through the TJA1051 whose EN pin is PB10, and CAN2, the
*                  MCP2515 on SPI1 selected by its pin), and then the wiring of the models to the virtual STM32. The ECU's own start,
*                  BM, the FBL and the application, is sw_test_ecu_start().
*
*****************************************************************************************************/
static void sw_test_power_on( false_true_et blank_nvm )
{
    /* The model's memory is the firmware's NVM reservation: the model decides which programs and erases of it fail */
    static const TMD_NVM_config_st nvm_cfg =
    {
        .memory_size  = NVM_TOTAL_SIZE,
        .sector_size  = FLS_STM32F1_PAGE_SIZE,
        .page_size    = 2u,
        .erased_value = 0xFFu,
        .corrupt_func_p = FLASH_SIM_tear_last_program    /* the virtual flash is the STM32's: a program the power cut left unfinished */
    };

    static const TMD_CANTP_config_st tester_cfg =
    {
        .request_id  = SW_TEST_ECU_UDS_REQUEST_ID,
        .response_id = SW_TEST_ECU_UDS_RESPONSE_ID
    };

    static const SPI_SIM_slave_st mcp2515_slave =
    {
        .is_selected = TMD_MCP2515_is_selected,
        .exchange    = TMD_MCP2515_spi_byte
    };

    /*************************************** 1. The hardware ***************************************/
    TMD_STARTUP_init_ram();    /* the firmware's RAM as the startup code leaves it, before anything of the firmware runs */
    vhw_power_on( blank_nvm );

    /********************************* 2. The models, each started alone ********************************/
    /* The pins are calls into the virtual STM32 (GPIOA is its smart register), so this cannot be static const */
    const TMD_PANEL_config_st panel_cfg =
    {
        .button            = { { PANEL_SELECT_BTN_PORT,  PANEL_SELECT_BTN_PIN },
                               { PANEL_CONFIRM_BTN_PORT, PANEL_CONFIRM_BTN_PIN },
                               { PANEL_BACK_BTN_PORT,    PANEL_BACK_BTN_PIN } },
        .knob_timer_p      = TIM4,
        .counts_per_detent = 4u      /* TIM4 decodes x4, hmi_sh1106_cfg_s.enc_counts_per_detent */
    };

    /* The inits clear what was registered with the model before, so nothing is wired yet */
    TMD_TIME_init( SW_TEST_NS_PER_ENTRY, SW_TEST_ENTRIES_PER_TICK, sw_test_tick_the_models );    /* every model acts as the clock moves */
    TMD_SYSTICK_init( vhw_systick_read_regs, &SystemCoreClock, vhw_systick_set_pending );    /* the clock's address: the firmware sets it up after this */
    TMD_CAN_init();
    TMD_TJA1051_reset();
    TMD_MCP2515_reset();
    TMD_NVM_init( &nvm_cfg );
    TMD_ERX_init();
    TMD_CANTP_init( &tester_cfg );       /* the tester on CAN1, at the ECU's own UDS server: a test points it at another node with TMD_CANTP_set_ids() */
    TMD_UDS_init();
    TMD_UDS_set_timeout( SW_TEST_TESTER_TIMEOUT_MS );
    TMD_MBA_init( &sw_test_bootloader_cfg_s );            /* the tester's writes are made by the bootloader, which the library does not run: the model is it */
    TMD_SH1106_init();                                    /* the OLED of the panel */
    TMD_PANEL_init( &panel_cfg );                         /* the buttons and the knob of the panel, released */

    /******************************** 3. The models wired to the virtual STM32 ********************************/
    TMD_TJA1051_attach( TEST_BUS_TESTER, CAN_SIM_rxd );
    CAN_SIM_attach( TMD_TJA1051_attached_txd );
    GPIO_SIM_set_listener( TJA1051_EN_PORT, TJA1051_EN_PIN, TMD_TJA1051_attached_enable_pin_set );

    TMD_MCP2515_attach( TEST_BUS_NODES );
    SPI_SIM_attach( SPI1, &mcp2515_slave );
    GPIO_SIM_set_listener( MCP2515_CS_PORT, MCP2515_CS_PIN, TMD_MCP2515_cs_set );

    TMD_ERX_set_can_instance( TEST_BUS_NODES );                  /* the ERXs are on the nodes' bus */
    GPIO_SIM_set_listener( ERX_PORT_0_PWR_PORT, ERX_PORT_0_PWR_PIN, TMD_ERX_port_a_supply_pin_set );    /* and on the supply the HUB switches */
    GPIO_SIM_set_listener( ERX_PORT_1_PWR_PORT, ERX_PORT_1_PWR_PIN, TMD_ERX_port_b_supply_pin_set );

    TMD_NVM_set_address_base( NVM_BASE_ADDRESS );

    TMD_SH1106_attach( I2C1, SH1106_I2C_ADDR_DEFAULT );

    /* The board's pull-ups hold a released button HIGH, which a virtual pin cannot do by itself: a power on clears it to LOW */
    GPIO_SIM_set_input( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN, HIGH );
}
/*!
****************************************************************************************************
*
*   \brief         Every model acts on the clock where it has got to: the scheduler's period, the bus, the ERXs (the UDS nodes answer what
*                  the scheduler's passes sent, and the heartbeats go out), the NVM and the tester, then the virtual STM32 catches up
*
*   \author        mstewart
*
*   \return        none
*
*   \note          They look at the clock and do what is due, so they can run as often as it moves: TMD_TIME runs this at every function
*                  the firmware enters, and at every millisecond stop. The virtual STM32 follows the same clock in its own sync (the
*                  timers, the watchdogs, the RTC); SysTick only sets its pending bit, and its handler, the scheduler's pass, runs from
*                  the interrupt poll like any other, by its priority.
*
*****************************************************************************************************/
static void sw_test_tick_the_models( void )
{
    TMD_SYSTICK_tick();   /* the scheduler's period: SysTick goes pending */
    TMD_CAN_tick();       /* the frames that have crossed the bus by now reach their end */
    TMD_ERX_tick();
    TMD_CANTP_tick();
    TMD_UDS_tick();

    vhw_sync();           /* the hardware catches up with what the models did, and the interrupts they asserted run: here, at the top of whichever function the firmware has just entered */
}

/*!
****************************************************************************************************
*
*   \brief         The ECU's own start, as it happens on the board: BM, then the FBL if the application asked for it, then the application
*
*   \author        mstewart
*
*   \return        none
*
*   \note          BM finds DATASET0 valid when its flash is not blank, and says so in the handoff RAM, which a restart keeps. The FBL is
*                  the MBA model, and it runs only for what the application hands over for, a write it kept (0x3D
*                  WriteMemoryByAddress): the application asked for it by the request in the handoff RAM, the FBL starts its NVM as its
*                  own start does and registers the block it keeps its note in (which the application only reads), makes the write,
*                  and clears the request and the failed-start count as its start does. The request is gone whatever happened: a power
*                  cut in the middle of it loses the handoff RAM. The firmware's RAM is wiped when the FBL is done, as the reset to the
*                  application does. A pin the application drove reaches its listener on the next sync of the virtual STM32: without
*                  this, the ERX supply it switched on at start-up would reach the ERXs only when the first millisecond goes by.
*
*****************************************************************************************************/
static void sw_test_ecu_start( void )
{
    /*************************************** BM ***************************************/
    BOOT_HANDOFF_init();

    /*************************************** FBL, when the application asked for it ***************************************/
    if( BOOT_HANDOFF_get_fbl_request() == TRUE )
    {
        NVM_GEN2_init( &nvm_gen2_hw_interface_s );
        NVM_GEN2_register_block( FBL_VPM_APPLIED_BLOCK_ID, &fbl_nvm_gen2_vpm_applied_block_s );

        if( TMD_MBA_is_write_pending() == TRUE )
        {
            (void)TMD_MBA_start_server();
            BOOT_HANDOFF_set_fbl_request( FALSE );
            BOOT_HANDOFF_set_boot_failure_count( 0u );    /* the application handed over at once: not a failed start */
        }

        TMD_STARTUP_init_ram();    /* the reset to the application */
    }

    /*************************************** BM again, then the application ***************************************/
    BOOT_HANDOFF_set_dataset_valid_mask( ( STDC_memcompare_value( (void*)SW_TEST_DATASET0_PAYLOAD_ADDRESS, 0xFFu,
                                                                  SW_TEST_DATASET0_PAYLOAD_SIZE ) == TRUE ) ? 0u : 1u );    /* bit 0: DATASET0 */

    app_main();
}

/****************************** END OF FILE *******************************************************/
