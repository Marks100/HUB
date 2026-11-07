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
#include "SW_TEST_STUBS.h"
#include "INTEGRATION_STUBS.h"

/***************************************************************************************************
**                                    Exported Globals                                           **
***************************************************************************************************/
extern void app_main( void );                      /* main.c: the ECU's start, it returns */

/* The ends of DATASET0's payload, which the APP linker script places in flash and TYRE_DATA.c reads. One byte each
   here: an empty dataset, so TYRE_DATA_init() sees no valid data. */
const u8_t __dataset0_code_start__ = 0u;
const u8_t __dataset0_code_end__   = 0u;

/* The start of the vector table, which app_main() hands to the vector table offset register */
u32_t __isr_vector_start = 0u;

/***************************************************************************************************
**                                  Private Function Prototypes                                   **
***************************************************************************************************/
static void        sw_test_models_init( void );
static void        sw_test_models_tick( void );
static void        sw_test_ecu_start( void );

const TMD_INIT_cfg_st sw_test_init_cfg_g =
{
    .power_on_func_p    = vhw_power_on,
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
    TMD_TIME_init( sw_test_models_tick );    /* every simulated millisecond ticks the models */
    TMD_SYSTICK_init( vhw_systick_read_regs, &SystemCoreClock );    /* the clock's address: the firmware sets it up after this */
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
    TMD_CAN_register_sync_hook( vhw_sync );    /* a frame the firmware just requested is on the bus when a test looks */

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
}

/*!
****************************************************************************************************
*
*   \brief         One simulated millisecond of the models: SysTick, which runs the firmware's scheduler as the real one
*                  does, then the ERXs (the UDS nodes answer what the scheduler's passes sent, and the heartbeats go out), the
*                  NVM, and the tester
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void sw_test_models_tick( void )
{
    TMD_SYSTICK_tick();
    TMD_ERX_UDS_tick();
    TMD_ERX_tick();
    TMD_NVM_tick();
    TMD_CANTP_tick();
    TMD_UDS_tick();
}

/*!
****************************************************************************************************
*
*   \brief         The ECU's start: app_main(), and then the virtual STM32 carries what it did to the pins to their models
*
*   \author        mstewart
*
*   \return        none
*
*   \note          A pin the firmware drove reaches its listener on the next sync of the virtual STM32. Without this, the
*                  ERX supply the firmware switched on at start-up would reach the ERXs only when the first millisecond goes by.
*
*****************************************************************************************************/
static void sw_test_ecu_start( void )
{
    app_main();
    vhw_sync();
}

/****************************** END OF FILE *******************************************************/
