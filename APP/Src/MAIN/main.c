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
#include "RNG.h"
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
#include "NVM_GEN2.h"
#include "APP_NVM_BLOCKS.h"
#include "BUZZER.h"
#include "CTRL_AXIS.h"
#include "nvic_driver.h"
#include "scb_driver.h"
#include "MCU_JUMP.h"
#include "TB_CBK.h"
#include "TJA1051.h"
#include "HAL_CAN.h"
#include "CPS.h"
#include "VEHICLE_SIM_config.h"
#include "VER.h"
#include "UDS_config.h"
#include "SHARED_RAM.h"

extern u32_t __isr_vector_start;   /* Linker symbol - APP/linker_script/STM32F103C8_flash.ld */

void app_main( void )
{
    /* Points VTOR at APP's own vector table, read from the linker symbol. */
    MCU_JUMP_set_vector_table( (u32_t)&__isr_vector_start );

    CLK_STM32F1_init( &hse8_72mhz_s );

    /* Idempotent (guarded by shared_ram_g.magic, see SHARED_RAM_init()) - a no-op on the normal
       BM->APP boot chain where BM already did this, but a real safety net if APP is ever entered
       directly (debugger, factory reflash) without BM having run first. FBL already does the same
       (fbl_board_init(), FBL/Src/INT_STUBS/INTEGRATION_STUBS.c) - APP was the one stage skipping
       it, silently trusting shared_ram_g.magic was already valid before this point. */
    SHARED_RAM_init();

    NVM_GEN2_init( &nvm_gen2_hw_interface_s );
    NVM_GEN2_register_block( APP_GENERIC_BLOCK_ID, &app_nvm_gen2_generic_block_s );
    NVM_GEN2_register_block( APP_KEY_1_BLOCK_ID, &app_nvm_gen2_key_1_block_s );
    NVM_GEN2_register_block( APP_KEY_2_BLOCK_ID, &app_nvm_gen2_key_2_block_s );
    NVM_GEN2_register_block( APP_CHASSIS_NUM_BLOCK_ID, &app_nvm_gen2_chassis_num_block_s );
    (void)APP_read_fbl_fingerprint( &app_fbl_fingerprint_g );
    (void)APP_read_fbl_boot_count( &app_fbl_boot_count_g );
    (void)APP_read_fbl_download_attempt_count( &app_fbl_download_attempt_count_g );
    (void)APP_read_fbl_dataset_download_count( &app_fbl_dataset_download_count_g );

    DBG_MGR_init( &dbg_mgr_cfg_s, SystemCoreClock );
    DWT_init( SystemCoreClock );

    HAL_BRD_init();
    HAL_CAN_init();
    HAL_CAN_set_rx_callback( app_can_rx_wrapper );
    HAL_ADC_init();
    HAL_TIM3_init();
    HAL_TIM4_init_encoder();
    HAL_USART2_init();
    HAL_SPI1_init();
    HAL_I2C1_init();
    CHKSUM_init_hw_crc( &hw_crc_cfg_s );
    CPS_init( &cps_crank_instance_s, &cps_crank_cfg_s, SystemCoreClock );
    APS_init( &aps_pedal_instance_s, &aps_pedal_cfg_s );
    ENGINE_SIM_init( &engine_sim_instance_s, &engine_sim_cfg_s );
    UID_init();
    RNG_init( UID_get_unique_id_32() );

    VER_init();
    BTN_MGR_init( &btm_mgr_instance_s, btm_mgr_control_s,
                  btm_mgr_func_table_s, BTM_MGR_FUNC_TABLE_SIZE( btm_mgr_func_table_s ),
                  NULL_P, 0u, BTN_MGR_TICK_TIME_MS );
    TIME_init( &time_cfg_s );
    HMI_SH1106_init( &hmi_sh1106_cfg_s );
    MENU_NAV_init();
    BUZZER_init( &buzzer_instance_s, &buzzer_func_table_s );
    WS2811_init( &ws2811_instance_s );
    NRF24_init( &nrf24_instance_s );
    RF_MGR_init( &rf_mgr_cfg_s );
    ESP01_init( &esp01_cfg_s );
    HAL_USART2_set_rx_callback( ESP01_uart_byte_rx );
    //WIFI_init( &wifi_cfg_s );
    TB_CBK_init( &tb_cfg_s );
    TB_init( &tb_cfg_s );
    TJA1051_init( &tja1051_func_s, &tja1051_cfg_s );
    PDUR_init( pdur_routing_table_s, pdur_num_routes_s );
    MSG_SCHED_init( &msg_sched_cfg_s );

    TCU_init( &tcu_cfg_s );
    GKT_SHIFTER_init( &gkt_shifter_cfg_s );
    MQB_CLUSTER_init( &mqb_cluster_cfg_s );

    app_cantp_instance_init();
    CANTP_init( &app_cantp_instance_s );

    const UDS_init_cfg_st app_uds_init_cfg_s =
    {
        .tp_send_func_p          = PDUR_tx,
        .message_received_notify = NULL_P,
        .service_table_p         = UDS_get_service_table(),
        .service_table_size      = UDS_get_service_table_size(),
        .buffer_p                = pdur_buffer_s,
        .buffer_size             = PDUR_BUFFER_SIZE
    };
    UDS_init( &app_uds_init_cfg_s );

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
