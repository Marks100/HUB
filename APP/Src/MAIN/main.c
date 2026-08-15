#include "INTEGRATION_STUBS.h"
#include "CLK_STM32F1.h"
#include "CHKSUM.h"
#include "HAL_BRD.h"
#include "HAL_ADC.h"
#include "HAL_TIM.h"
#include "HAL_SPI.h"
#include "HAL_UART.h"
#include "DWT.h"
#include "UID.h"
#include "WDG.h"
#include "DBG_MGR.h"
#include "BTN_MGR.h"
#include "ROTARY_MGR.h"
#include "HMI_SH1106.h"
#include "MENU_NAV.h"
#include "HAL_I2C.h"
#include "TIME.h"
#include "MODE_MGR.h"
#include "WS2811.h"
#include "RF_MGR.h"
#include "NRF24.h"
#include "NVM.h"
#include "BUZZER.h"
#include "CTRL_AXIS.h"
#include "nvic_driver.h"
#include "scb_driver.h"
#include "TB_CBK.h"
#include "TJA1051.h"
#include "HAL_CAN.h"
#include "CPS.h"
#include "VER.h"

void app_main( void )
{
    CLK_STM32F1_init( &hse8_72mhz_s );
    NVM_init( &nvm_hw_interface_s );
    NVM_register_block( 0u, &nvm_persist_block_s );

    DBG_MGR_init( &dbg_mgr_cfg_s, SystemCoreClock );
    DWT_init( SystemCoreClock );
    HAL_BRD_init();
    HAL_CAN_init( NULL_P );
    HAL_ADC_init();
    HAL_TIM3_init();
    HAL_TIM4_init_encoder();
    HAL_USART2_init();
    HAL_SPI1_init();
    HAL_I2C1_init();
    CHKSUM_init_hw_crc( &hw_crc_cfg_s );
    UID_init();
    VER_init();
    BTN_MGR_init( &btm_mgr_instance_s, btm_mgr_control_s,
                  btm_mgr_func_table_s, BTM_MGR_FUNC_TABLE_SIZE( btm_mgr_func_table_s ),
                  NULL_P, 0u, BTN_MGR_TICK_TIME_MS );
    TIME_init( &time_cfg_s );

    /* SH1106 panel - owns TIM4's encoder, its own three buttons and the OLED. Needs I2C1 up
       first. There is no standalone ROTARY_MGR instance: the board has one encoder and this
       panel is what drives it. MENU_NAV_init() has to run after the panel exists, since it
       selects the screen that HMI_SH1106's pending first repaint will paint. */
    HMI_SH1106_init( &hmi_sh1106_cfg_s );
    MENU_NAV_init();
    BUZZER_init( &buzzer_instance_s, &buzzer_func_table_s );
    WS2811_init( &ws2811_instance_s );
    NRF24_init( &nrf24_instance_s );
    RF_MGR_init( &rf_mgr_cfg_s );
    ESP01_init( &esp01_cfg_s );
    HAL_USART2_set_rx_callback( ESP01_uart_byte_rx );
    WIFI_init( &wifi_cfg_s );
    TB_CBK_init( &tb_cfg_s );
    TB_init( &tb_cfg_s );
    TJA1051_init( &tja1051_func_s, &tja1051_cfg_s );
    PDUR_init( pdur_routing_table_s, pdur_num_routes_s );
    MODE_MGR_init();

    SYSTICK_init( &systick_cfg_s, SystemCoreClock );
    NVIC_EnableGlobalIRQ();

    /* Init WDG at the end */
    //WDG_init( &wdg_cfg_s );

    while(1);
}

void main( void )
{
    app_main();
}

/****************************** END OF FILE *******************************************************/
