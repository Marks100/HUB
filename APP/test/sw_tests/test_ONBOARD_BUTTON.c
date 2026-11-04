/*! \file
*               Author: mstewart
*   \brief      Onboard button - software test
*
*   \details    The real chain from the pin to the button manager: GPIO_ReadInputDataBit() of the SPL on the virtual GPIOA,
*               HAL_BRD_read_onboard_btn(), and the BTN_MGR row of INTEGRATION_STUBS.c, ticked by the scheduler every 20 ms.
*               A test presses the button by driving the pin and moves the time on. The switch pulls the pin to GND against
*               the internal pull-up, so the pin is HIGH released and LOW pressed. Every test case starts as a virgin ECU
*               (setUp, SW_TEST_STUBS.c).
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "HAL_config.h"
#include "BTN_MGR.h"

#include "vhw.h"
#include "stm32f10x_gpio.h"

#include "TMD_INIT.h"
#include "TMD_TIME.h"

#include "SW_TEST_STUBS.h"

#define TEST_BUTTON_ONBOARD         ( 0u )                       /* its row in btm_mgr_func_table_s */
#define TEST_BUTTON_SETTLE_MS       ( 5u * BTN_MGR_TICK_TIME_MS )    /* five scans: past the debounce */

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU, with the button released: the virtual pin has no pull-up, so
*                  the test drives it HIGH as the board's pull-up does
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    GPIO_SIM_set_input( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN, HIGH );
    TMD_TIME_advance_time_ms( TEST_BUTTON_SETTLE_MS );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the button manager does not see the button held while the pin is released
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_released_button_is_not_detected( void )
{
    TMD_TIME_advance_time_ms( TEST_BUTTON_SETTLE_MS );

    TEST_ASSERT_FALSE( BTN_MGR_is_button_held( &btm_mgr_instance_s, TEST_BUTTON_ONBOARD ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a press is detected as held: the pin driven LOW reaches the button manager through the SPL,
*                  HAL_BRD and the scheduler's button tick
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_pressing_the_onboard_button_is_detected( void )
{
    GPIO_SIM_set_input( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN, LOW );
    TMD_TIME_advance_time_ms( TEST_BUTTON_SETTLE_MS );

    TEST_ASSERT_TRUE( BTN_MGR_is_button_held( &btm_mgr_instance_s, TEST_BUTTON_ONBOARD ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a release is detected after a press: the button is no longer held
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_releasing_the_onboard_button_is_detected( void )
{
    GPIO_SIM_set_input( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN, LOW );
    TMD_TIME_advance_time_ms( TEST_BUTTON_SETTLE_MS );

    GPIO_SIM_set_input( ONBOARD_BTN_PORT, ONBOARD_BTN_PIN, HIGH );
    TMD_TIME_advance_time_ms( TEST_BUTTON_SETTLE_MS );

    TEST_ASSERT_FALSE( BTN_MGR_is_button_held( &btm_mgr_instance_s, TEST_BUTTON_ONBOARD ) );
}

/****************************** END OF FILE *******************************************************/
