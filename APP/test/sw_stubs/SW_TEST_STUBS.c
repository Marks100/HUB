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
static void        sw_test_virgin_init( void );
static void        sw_test_ecu_start( void );
static u8_t        sw_test_find_erx( u8_t iso_position );
static u8_t        sw_test_find_free_slot( u8_t port );
static u8_t        sw_test_port_of( u8_t iso_position );

/* The ISO position of the ERX in each slot of the model, SW_TEST_FREE_SLOT while the slot has none */
static u8_t sw_test_erx_position_s[TMD_ERX_MAX_INSTANCES];

const TMD_INIT_cfg_st sw_test_init_cfg_g =
{
    .power_on_func_p    = vhw_reset,
    .models_init_func_p = sw_test_models_init,
    .virgin_init_func_p = sw_test_virgin_init,
    .ecu_main_func_p    = sw_test_ecu_start
};

/***************************************************************************************************
**                                  Public Functions                                              **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Plug an ERX in at an ISO position, onto the next free slot of the model
*
*   \author        mstewart
*
*   \param[in]     iso_position - One of APP_ISO_POSITIONS.h: its heartbeat ID is 0x400 plus it, its UDS response ID 0x600
*                                 plus it and its UDS request ID 0x700 plus it
*
*   \return        none
*
*   \note          The one place a test sets an ERX's position, and the only one that knows which slot of the model holds it.
*                  It never powers the ERX: the HUB does, by driving its port's supply pin HIGH (ERX_PORT_0_PWR_PIN /
*                  ERX_PORT_1_PWR_PIN, which it does at start-up for the ports the init has an ERX on), and the ERX starts when
*                  the model sees that. An ERX goes on the port that app_autocfg_init_s puts its position on (port 0 for a
*                  position it does not list), in the next free slot of that port. At most one ERX per position.
*
*****************************************************************************************************/
void sw_test_place_erx( u8_t iso_position )
{
    TMD_ERX_config_st config;
    u8_t              instance = sw_test_find_free_slot( sw_test_port_of( iso_position ) );

    if( instance != SW_TEST_NO_ERX )
    {
        TMD_ERX_get_configuration( instance, &config );

        config.iso_position = iso_position;

        TMD_ERX_configure_instance( instance, &config );

        sw_test_erx_position_s[instance] = iso_position;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Unplug the ERX at an ISO position: it is off the bus, so it sends and answers nothing
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position it was placed at
*
*   \return        none
*
*****************************************************************************************************/
void sw_test_unplug_erx( u8_t iso_position )
{
    u8_t instance = sw_test_find_erx( iso_position );

    if( instance != SW_TEST_NO_ERX )
    {
        TMD_ERX_unplug_instance( instance );
    }
}

/*!
****************************************************************************************************
*
*   \brief         Plug the ERX at an ISO position back in: the HUB's supply is still on, so it starts again
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position it was placed at
*
*   \return        none
*
*****************************************************************************************************/
void sw_test_plug_in_erx( u8_t iso_position )
{
    u8_t instance = sw_test_find_erx( iso_position );

    if( instance != SW_TEST_NO_ERX )
    {
        TMD_ERX_plug_in_instance( instance );
    }
}

/*!
****************************************************************************************************
*
*   \brief         The slot of the model that holds the ERX at an ISO position
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position it was placed at
*
*   \return        The slot, for the models that talk about an ERX by it (TMD_ERX_UDS_*). SW_TEST_NO_ERX if none is there.
*
*****************************************************************************************************/
u8_t sw_test_erx_slot( u8_t iso_position )
{
    return( sw_test_find_erx( iso_position ) );
}
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
    STDC_memset( sw_test_erx_position_s, 0x00, sizeof( sw_test_erx_position_s ) );
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
*   \brief         The slot of the model whose ERX is at an ISO position
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position, or SW_TEST_FREE_SLOT for the first slot that has no ERX yet
*
*   \return        The slot, SW_TEST_NO_ERX if there is none
*
*****************************************************************************************************/
static u8_t sw_test_find_erx( u8_t iso_position )
{
    u8_t found = SW_TEST_NO_ERX;
    u8_t slot;

    for( slot = 0u; slot < TMD_ERX_MAX_INSTANCES; slot++ )
    {
        if( sw_test_erx_position_s[slot] == iso_position )
        {
            found = slot;
            break;
        }
    }

    return( found );
}

/*!
****************************************************************************************************
*
*   \brief         The first slot of a port of the model that has no ERX yet
*
*   \author        mstewart
*
*   \param[in]     port - The port: its slots are port * (TMD_ERX_MAX_INSTANCES / TMD_ERX_MAX_PORTS) and the ones after it
*
*   \return        The slot, SW_TEST_NO_ERX if the port is full
*
*****************************************************************************************************/
static u8_t sw_test_find_free_slot( u8_t port )
{
    u8_t first = port * ( TMD_ERX_MAX_INSTANCES / TMD_ERX_MAX_PORTS );
    u8_t found = SW_TEST_NO_ERX;
    u8_t slot;

    for( slot = first; slot < ( first + ( TMD_ERX_MAX_INSTANCES / TMD_ERX_MAX_PORTS ) ); slot++ )
    {
        if( sw_test_erx_position_s[slot] == SW_TEST_FREE_SLOT )
        {
            found = slot;
            break;
        }
    }

    return( found );
}

/*!
****************************************************************************************************
*
*   \brief         The port app_autocfg_init_s puts an ISO position on
*
*   \author        mstewart
*
*   \param[in]     iso_position - The position
*
*   \return        Its port, 0 if the init does not list the position
*
*****************************************************************************************************/
static u8_t sw_test_port_of( u8_t iso_position )
{
    u8_t port = 0u;
    u8_t row;

    for( row = 0u; row < AUTOCFG_V2_MAX_ERXS; row++ )
    {
        if( ( app_autocfg_init_s.erx_locations[row].iso_position == iso_position ) && ( app_autocfg_init_s.erx_locations[row].port < TMD_ERX_MAX_PORTS ) )
        {
            port = app_autocfg_init_s.erx_locations[row].port;
            break;
        }
    }

    return( port );
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
/*!
****************************************************************************************************
*
*   \brief         A first start is a blank ECU: no NVM yet. A restart that keeps its NVM does not run this.
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void sw_test_virgin_init( void )
{
    FLASH_SIM_erase_all();
}

/****************************** END OF FILE *******************************************************/
