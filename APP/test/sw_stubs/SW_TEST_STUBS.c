/*! \file
*               Author: mstewart
*   \brief      Software tests - how every test starts the ECU
*
*   \details    CAN1 reaches its bus through the TJA1051 model, which the firmware has to take out of standby (EN pin,
*               TJA1051_init) before a frame passes either way; CAN2 is the MCP2515 on SPI1 and its chip model. The
*               symbols the linker script would give the APP are here too, as an empty dataset.
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
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "FLS_STM32F1.h"
#include "TMD_INIT.h"
#include "TMD_CAN.h"
#include "TMD_CANTP.h"
#include "TMD_UDS.h"
#include "TMD_TJA1051.h"
#include "TMD_MCP2515.h"
#include "TMD_MBA.h"
#include "TMD_STARTUP.h"
#include "TMD_SH1106.h"
#include "SH1106.h"
#include "BOOT_HANDOFF.h"
#include "SW_TEST_STUBS.h"
#include "INTEGRATION_STUBS.h"

/***************************************************************************************************
**                                    Exported Globals                                           **
***************************************************************************************************/
extern void app_main( void );                      /* main.c: the ECU's start, it returns */

/* DATASET0's payload as BM leaves it in flash (the layout of TYRE_DATA.h, format 2): two makes, three models, four year
   ranges. The linker model puts it where the firmware reads it. */
static const u8_t sw_test_dataset0_c[] =
{
    'T', 'P', 2u, 2u, 3u, 0u, 4u, 0u,                                   /* header: magic, format, 2 makes, 3 models, 4 year ranges */
    2u, 1u,                                                             /* models per make: Audi 2, BMW 1 */
    2u, 1u, 1u,                                                         /* year ranges per model: A3 2, A4 1, M3 1 */
    'A', 'u', 'd', 'i', 0, 'B', 'M', 'W', 0,                            /* make names */
    'A', '3', 0, 'A', '4', 0, 'M', '3', 0,                              /* model names */
    103u, 112u, 23u, 22u,   113u, 120u, 24u, 23u,                       /* A3 2003 - 2012 and 2013 - 2020: front and rear in 0.1 bar */
    108u, 115u, 22u, 21u,                                               /* A4 2008 - 2015 */
    107u, 113u, 25u, 24u                                                /* M3 2007 - 2013 */
};

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

static void        sw_test_models_init( void );
static void        sw_test_tick_the_models( void );
static void        sw_test_ecu_start( void );
static void        sw_test_power_on( false_true_et blank_nvm );

const TMD_INIT_cfg_st sw_test_init_cfg_g =
{
    .power_on_func_p    = sw_test_power_on,
    .models_init_func_p = sw_test_models_init,
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
*   \brief         Initialise every model, in the order the ECU is powered on: the clock, the bus models, then what attaches
*                  to them: CAN1 (bxCAN TXD and RXD through the TJA1051, whose EN pin is PB10) and CAN2 (the MCP2515 on
*                  SPI1, selected by its pin). The interrupt handlers are bound by vhw_reset().
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void sw_test_models_init( void )
{
    /* The model's memory is the firmware's NVM reservation: the model decides which programs and erases of it fail */
    static const TMD_NVM_config_st nvm_cfg =
    {
        .memory_size  = NVM_TOTAL_SIZE,
        .sector_size  = FLS_STM32F1_PAGE_SIZE,
        .page_size    = 2u,
        .erased_value = 0xFFu
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

    /* Every model first: the inits clear what was registered with the model before, so nothing is wired yet */
    TMD_TIME_init( SW_TEST_NS_PER_ENTRY, sw_test_tick_the_models );    /* every model acts as the clock moves */
    TMD_SYSTICK_init( vhw_systick_read_regs, &SystemCoreClock, vhw_systick_set_pending );    /* the clock's address: the firmware sets it up after this */
    TMD_CAN_init();
    TMD_TJA1051_reset();
    TMD_MCP2515_reset();
    TMD_NVM_init( &nvm_cfg );
    TMD_ERX_UDS_init();                  /* no ERX on the buses: a test adds the ones it needs */
    TMD_ERX_init();
    TMD_CANTP_init( &tester_cfg );       /* the tester on CAN1, at the ECU's own UDS server: a test points it at another node with TMD_CANTP_set_ids() */
    TMD_UDS_init();
    TMD_UDS_set_timeout( SW_TEST_TESTER_TIMEOUT_MS );

    /* Then the wiring between the models and the virtual STM32 */

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
    FLASH_SIM_set_fault_funcs( TMD_NVM_is_write_faulted, TMD_NVM_is_erase_faulted );

    TMD_SH1106_init();                                          /* the OLED of the panel, on I2C1 */
    TMD_SH1106_attach( I2C1, SH1106_I2C_ADDR_DEFAULT );
    
    /* The board's pull-ups hold a released button HIGH, which a virtual pin cannot do by itself: a power on clears it to LOW */
    GPIO_SIM_set_input( ONBOARD_BTN_PORT,       ONBOARD_BTN_PIN,       HIGH );
    GPIO_SIM_set_input( PANEL_SELECT_BTN_PORT,  PANEL_SELECT_BTN_PIN,  HIGH );
    GPIO_SIM_set_input( PANEL_CONFIRM_BTN_PORT, PANEL_CONFIRM_BTN_PIN, HIGH );
    GPIO_SIM_set_input( PANEL_BACK_BTN_PORT,    PANEL_BACK_BTN_PIN,    HIGH );
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
    TMD_ERX_UDS_tick();
    TMD_ERX_tick();
    TMD_NVM_tick();
    TMD_CANTP_tick();
    TMD_UDS_tick();

    vhw_sync();           /* the hardware catches up with what the models did, and the interrupts they asserted run: here, at the top of whichever function the firmware has just entered */
}

/*!
****************************************************************************************************
*
*   \brief         The ECU's start: what BM leaves for it, app_main(), and then the virtual STM32 carries what it did to the pins
*                  to their models
*
*   \author        mstewart
*
*   \return        none
*
*   \note          BM finds DATASET0 valid when there is a payload in it, and says so in the handoff RAM, which a restart keeps.
*                  A pin the firmware drove reaches its listener on the next sync of the virtual STM32. Without this, the
*                  ERX supply the firmware switched on at start-up would reach the ERXs only when the first millisecond goes by.
*
*****************************************************************************************************/
static void sw_test_ecu_start( void )
{
    BOOT_HANDOFF_init();
    BOOT_HANDOFF_set_dataset_valid_mask( ( TMD_MBA_is_dataset0_present() == TRUE ) ? 1u : 0u );    /* bit 0: DATASET0 */

    app_main();
}

/*!
****************************************************************************************************
*
*   \brief         Power the ECU on: the firmware's RAM, the virtual STM32, and DATASET0 with the tyre table the factory flashes
*
*   \author        mstewart
*
*   \param[in]     blank_nvm    TRUE: a new ECU. FALSE: a restart, which keeps whatever is in its flash
*
*   \return        none
*
*****************************************************************************************************/
static void sw_test_power_on( false_true_et blank_nvm )
{
    TMD_STARTUP_init_ram();    /* the firmware's RAM as the startup code leaves it, before anything of the firmware runs */
    vhw_power_on( blank_nvm );
    TMD_MBA_power_on( blank_nvm, sw_test_dataset0_c, (u16_t)sizeof( sw_test_dataset0_c ) );
}

/****************************** END OF FILE *******************************************************/
