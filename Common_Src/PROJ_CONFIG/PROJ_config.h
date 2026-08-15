#ifndef PROJ_CONFIG_H
#define PROJ_CONFIG_H

#define STM32F1                         ( 1u )
#define DBG_LOG_ENABLED                 ( 1u )
#define APP_TIMER_TICK_RATE_MS          ( 10u )
#define BL_TIMER_TICK_RATE_MS           ( 5u )
#define ONBOARD_BUTTON_PRESENT

/* System clock in Hz - must match the CLK_STM32F1_init() profile selected in main.c
   (currently hse8_72mhz_s: HSE 8MHz + PLLx9 = 72MHz, see CLKS/README.md) */
#define SYSCLK_HZ                       ( 72000000u )

/* WS2811 LED strip is NOT physically wired on this board revision. PA10 is a confirmed-free
   pin (USART1 RX, and USART1 itself is unused - see ABS2_INPUT_PIN's comment in HAL_config.h)
   used here purely as a placeholder so WS2811_HW_STM32F1.c compiles; MODE_MGR still calls
   WS2811_set_all_led_color() so this pin will toggle at runtime, but drives nothing. */
#define WS2811_HW_GPIO_PORT             GPIOA
#define WS2811_PIN                      ( 10u )

#endif /* PROJ_CONFIG_H multiple inclusion guard */
