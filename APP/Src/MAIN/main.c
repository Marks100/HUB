#include "INTEGRATION_STUBS.h"
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
#include "TIME.h"
#include "MODE_MGR.h"
#include "WS2811.h"
#include "ST7567.h"
#include "RF_MGR.h"
#include "NRF24.h"
#include "NVM.h"
#include "BUZZER.h"
#include "CTRL_AXIS.h"
#include "nvic_driver.h"
#include "TB_CBK.h"

void app_main( void )
{
    /* SystemInit() is called by the startup file before main.
       Update the SystemCoreClock variable to reflect actual frequency. */
    SystemCoreClockUpdate();

    NVM_init( &nvm_hw_interface_s );
    NVM_register_block( 0u, &nvm_persist_block_s );

    HAL_BRD_init();
    DBG_MGR_init( &dbg_mgr_cfg_s, SystemCoreClock );
    DWT_init( SystemCoreClock );
    HAL_ADC_init();
    HAL_TIM3_init();
    HAL_TIM4_init_encoder();
    HAL_USART1_init();
    HAL_USART2_init();
    HAL_SPI1_init();
    CHKSUM_init_hw_crc( &hw_crc_cfg_s );
    UID_init();
    BTN_MGR_init( (BTN_MGR_func_table_st*)&btm_mgr_func_table_s, sizeof(btm_mgr_func_table_s)/sizeof(BTN_MGR_func_table_st), NULL_P, 0u );
    TIME_init( &time_cfg_s );
    ROTARY_MGR_init( &rotary_mgr_func_table_s );
    BUZZER_init( &buzzer_instance_s, &buzzer_func_table_s );
    WS2811_init( &ws2811_instance_s );
    ST7567_init( &st7567_func_table_s );
    NRF24_init( &nrf24_instance_s );
    RF_MGR_init( (RF_MGR_cfg_st){ .mode = RF_MGR_MODE_TX, .channel = 0u } );
    ESP01_init( &esp01_cfg_s );
    HAL_USART2_set_callback( esp01_uart_byte_rx );
    WIFI_init( &wifi_cfg_s );
    TB_CBK_init( &tb_cfg_s );
    TB_init( &tb_cfg_s );
    MODE_MGR_init();

    SYSTICK_init( &systick_cfg_s, SystemCoreClock );

    NVIC_EnableGlobalIRQ();

    /* Init WDG at the end */
    //WDG_init( &wdg_cfg_s );

    while(1)
    {
        __WFI();

        if( app_tick_pending() )
        {
            ESP01_tick();
            WIFI_tick();
            TB_tick();
            MODE_MGR_tick();
        }
    }
}

void main( void )
{
    app_main();
}

/****************************** END OF FILE *******************************************************/
