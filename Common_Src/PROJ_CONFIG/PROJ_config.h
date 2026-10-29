#ifndef PROJ_CONFIG_H
#define PROJ_CONFIG_H

#define STM32F1                         ( 1u )
#define DBG_LOG_ENABLED                 ( 1u )
#define APP_TIMER_TICK_RATE_MS          ( 5u )
#define BL_TIMER_TICK_RATE_MS           ( 5u )
#define ONBOARD_BUTTON_PRESENT

/* OLED pages pushed per HMI tick - each is a blocking ~3 ms I2C write */
#define SH1106_MAX_PAGES_PER_TICK       ( 2u )

/* System clock in Hz - must match the CLK_STM32F1_init() profile selected in main.c
   (currently hse8_72mhz_s: HSE 8MHz + PLLx9 = 72MHz, see CLKS/README.md) */
#define SYSCLK_HZ                       ( 72000000u )

/* SENT_TX bench sensor emulator (APP/Src/SENT_CFG): 1 = TIM1 and PB12 run it from boot, 0 = left
   unused. It takes TIM1 and PB12 - nothing else in this image uses either. */
#define SENT_TX_BENCH_ENABLED           ( 1u )

/* WIFI mode the ESP01 starts in (it can be changed at run time from the WiFi screen):
     0 = station only - joins the home network, ThingsBoard runs (normal)
     1 = soft access point only ("HUB_AP", see wifi_cfg_s in INTEGRATION_STUBS.c), ThingsBoard offline
     2 = both at once - the soft AP is up and the station joins the home network */
#define WIFI_STARTUP_MODE               ( 0u )

/* 1 = the Messages server also listens on the station, so a device on the same network as the HUB (a
   wired PC, say) can send to the HUB's station IP shown on the WiFi screen, no WiFi on that device
   needed. 0 = only while the soft AP is running, so only a device that has joined HUB_AP can. The server
   has no login: anyone who can reach that port can put text on the Messages screen. */
#define WIFI_SERVER_ON_STATION          ( 1u )

/* WS2811 LED strip is NOT physically wired on this board revision. PA10 is a confirmed-free
   pin (USART1 RX, and USART1 itself is unused - see ABS2_INPUT_PIN's comment in HAL_config.h)
   used here purely as a placeholder so WS2811_HW_STM32F1.c compiles; MODE_MGR still calls
   WS2811_set_all_led_color() so this pin will toggle at runtime, but drives nothing. */
#define WS2811_HW_GPIO_PORT             GPIOA
#define WS2811_PIN                      ( 10u )

/* NVM_GEN2 reservation, fed to NVM_GEN2_hw_interface_st as plain config (not queried from the
   flash driver - see FLS_STM32F1.h's top comment for why). This part is a "C8" (64 KB nominal)
   that's physically 128 KB silicon, confirmed via a live F_SIZE register read at bring-up - see
   the flash layout comment at the top of each image's linker script. The top two 1 KB pages are
   reserved: both FBL and APP point at this SAME base and share both pages as NVM_GEN2's two
   flip-flop partitions (see xCOMMON_MODULES/Src/NVM_GEN2/README.md). This is the single place
   these two values are defined - the linker scripts (APP/BM/FBL) and Lauterbach .cmm configs all
   hand-carve flash around this same reservation and must be kept in sync with it by hand. */
#define NVM_BASE_ADDRESS                ( 0x0801F800UL )
#define NVM_TOTAL_SIZE                  ( 2u * 1024u )

#endif /* PROJ_CONFIG_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/