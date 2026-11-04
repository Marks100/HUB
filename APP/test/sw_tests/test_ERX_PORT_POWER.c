/*! \file
*               Author: mstewart
*   \brief      ERX port power - software test
*
*   \details    The supply enable pins of the two ERX ports (PB4 and PB5, HAL_config.h), seen as the level on the pin of the
*               virtual STM32. The HUB runs AUTOCFG without its master (autocfg_enabled FALSE in app_autocfg_init_s), so
*               nothing else powers the ERXs: AUTOCFG_init() switches on the ports that have an ERX configured (both,
*               one ERX each, in app_autocfg_init_s), through the real HAL_BRD, and leaves any other off. Every test case starts as a
*               virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "HAL_config.h"

#include "vhw.h"
#include "stm32f10x_gpio.h"

#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_SETTLE_MS      ( 1000u )

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify both ERX ports are powered once the ECU has started, as an ERX is configured on each: AUTOCFG is not
*                  enabled, so nothing else would power them
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_both_erx_ports_are_powered_when_the_ecu_starts( void )
{
    TEST_ASSERT_EQUAL( HIGH, GPIO_SIM_get_pin( ERX_PORT_0_PWR_PORT, ERX_PORT_0_PWR_PIN ) );
    TEST_ASSERT_EQUAL( HIGH, GPIO_SIM_get_pin( ERX_PORT_1_PWR_PORT, ERX_PORT_1_PWR_PIN ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ports stay powered while the ECU runs: nothing switches them off
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_erx_ports_stay_powered( void )
{
    TMD_TIME_advance_time_ms( TEST_SETTLE_MS );

    TEST_ASSERT_EQUAL( HIGH, GPIO_SIM_get_pin( ERX_PORT_0_PWR_PORT, ERX_PORT_0_PWR_PIN ) );
    TEST_ASSERT_EQUAL( HIGH, GPIO_SIM_get_pin( ERX_PORT_1_PWR_PORT, ERX_PORT_1_PWR_PIN ) );
}

/****************************** END OF FILE *******************************************************/
