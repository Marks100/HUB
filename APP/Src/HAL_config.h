#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

#include "stm32f10x.h"

/***************************************************************************************************
**  Hardware Variant Selection
**  Define exactly one variant here.
***************************************************************************************************/
#define HW_VARIANT_BLUE_PILL    1u
#define HW_VARIANT_SUPER_PILL   2u
#define HW_VARIANT_CUSTOM_V1    3u

#define HW_VARIANT              HW_VARIANT_SUPER_PILL

/***************************************************************************************************
**  Per-Variant Pin Assignments
***************************************************************************************************/
#if ( HW_VARIANT == HW_VARIANT_BLUE_PILL )
 
#define ONBOARD_LED_PORT        GPIOC
#define ONBOARD_LED_PIN         GPIO_Pin_13
#define WS2811_PORT             GPIOA
#define WS2811_PIN              GPIO_Pin_8

#elif ( HW_VARIANT == HW_VARIANT_SUPER_PILL )

#define ONBOARD_LED_PORT        GPIOA
#define ONBOARD_LED_PIN         GPIO_Pin_1
#define ONBOARD_BTN_PORT        GPIOA
#define ONBOARD_BTN_PIN         GPIO_Pin_8
#define PANEL_3_BTN_PORT        GPIOA
#define PANEL_3_BTN_PIN         GPIO_Pin_8

/* USART2 - ESP8266 (PA2=TX, PA3=RX) */
#define ESP_UART_PORT       GPIOA
#define ESP_TX_PIN          GPIO_Pin_2
#define ESP_RX_PIN          GPIO_Pin_3

/* NRF24L01 control pins (SPI1 shared bus) */
#define NRF_CE_PORT         GPIOB
#define NRF_CE_PIN          GPIO_Pin_0
#define NRF_CS_PORT         GPIOB
#define NRF_CS_PIN          GPIO_Pin_2
#define NRF_IRQ_PORT        GPIOA
#define NRF_IRQ_PIN         GPIO_Pin_15
#define NRF24_IRQ_EXT_LINE  EXTI_Line15

#define WS2811_PORT             //GPIOA
#define WS2811_PIN              //GPIO_Pin_8

#else
#error "No HW_VARIANT defined — set HW_VARIANT in HAL_config.h"
#endif

/***************************************************************************************************
**  Shared Pin Assignments (common across all variants)
***************************************************************************************************/
/* SPI1 - shared bus: LCD display + NRF24 radio (PA5=SCK, PA6=MISO, PA7=MOSI) */
#define SPI1_PORT           GPIOA
#define SPI1_SCK_PIN        GPIO_Pin_5
#define SPI1_MISO_PIN       GPIO_Pin_6
#define SPI1_MOSI_PIN       GPIO_Pin_7

/* Debug mode LED */
#define DEBUG_MODE_LED_PORT GPIOB
#define DEBUG_MODE_LED_PIN  GPIO_Pin_12

/* Panel buttons (pull-down inputs) */
#define PANEL_2_BTN_PORT    GPIOA
#define PANEL_2_BTN_PIN     GPIO_Pin_0

/* LCD control pins (ST7567) */
#define LCD_A0_PORT         GPIOB
#define LCD_A0_PIN          GPIO_Pin_4
#define LCD_CS_PORT         GPIOB
#define LCD_CS_PIN          GPIO_Pin_5
#define LCD_RST_PORT        GPIOB
#define LCD_RST_PIN         GPIO_Pin_6

/* TIM3 CH4 buzzer output pin */
#define BUZZER_TIM_PORT     GPIOB
#define BUZZER_TIM_PIN      GPIO_Pin_1

/* TIM4 encoder inputs (CH1=PB6, CH2=PB7) */
#define ENC_PORT            GPIOB
#define ENC_CH1_PIN         GPIO_Pin_6
#define ENC_CH2_PIN         GPIO_Pin_7

/* I2C1 pins (remapped: PB8=SCL, PB9=SDA) */
#define I2C1_PORT           GPIOB
#define I2C1_SCL_PIN        GPIO_Pin_8
#define I2C1_SDA_PIN        GPIO_Pin_9

/* ADC channels for analog joystick/pot inputs */
#define ADC_STEERING_CHANNEL        ADC_Channel_4   /* PA4 */
#define ADC_STEERING_TRIM_CHANNEL   ADC_Channel_5   /* PA5 -- placeholder, verify */
#define ADC_THROTTLE_CHANNEL        ADC_Channel_6   /* PA6 -- placeholder, verify */

/* TJA1051 CAN transceiver */
#define TJA1051_EN_PORT             GPIOB
#define TJA1051_EN_PIN              GPIO_Pin_10

/* bxCAN1 — PA11=RX, PA12=TX (default mapping, no remap required) */
#define CAN_TX_PORT                 GPIOA
#define CAN_TX_PIN                  GPIO_Pin_12
#define CAN_RX_PORT                 GPIOA
#define CAN_RX_PIN                  GPIO_Pin_11

#endif /* HAL_CONFIG_H */
