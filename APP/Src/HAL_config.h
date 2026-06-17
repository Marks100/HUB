#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

/* TODO: Verify ALL pin assignments below against your schematic before building */

#include "stm32f10x.h"

#define HW_VERSION      ( 2u )

/* SPI1 - LCD display (PA5=SCK, PA6=MISO, PA7=MOSI) */
#define SPI1_PORT           GPIOA
#define SPI1_SCK_PIN        GPIO_Pin_5
#define SPI1_MISO_PIN       GPIO_Pin_6
#define SPI1_MOSI_PIN       GPIO_Pin_7

/* SPI2 - NRF24 radio (PB13=SCK, PB14=MISO, PB15=MOSI) */
#define SPI2_PORT           GPIOB
#define SPI2_SCK_PIN        GPIO_Pin_13
#define SPI2_MISO_PIN       GPIO_Pin_14
#define SPI2_MOSI_PIN       GPIO_Pin_15

/* Onboard LED */
#define ONBOARD_LED_PORT    GPIOC
#define ONBOARD_LED_PIN     GPIO_Pin_13

/* Debug mode LED */
#define DEBUG_MODE_LED_PORT GPIOB
#define DEBUG_MODE_LED_PIN  GPIO_Pin_12

/* Debug select input (pull-down) */
#define DEBUG_SEL_PORT      GPIOB
#define DEBUG_SEL_PIN       GPIO_Pin_0

/* Panel buttons (pull-down inputs) */
#define PANEL_2_BTN_PORT    GPIOA
#define PANEL_2_BTN_PIN     GPIO_Pin_0
#define PANEL_3_BTN_PORT    GPIOA
#define PANEL_3_BTN_PIN     GPIO_Pin_1

/* Rotary encoder switch (pull-down input) */
#define ROTARY_SW_PORT      GPIOA
#define ROTARY_SW_PIN       GPIO_Pin_2

/* LCD reset switch (pull-down input) */
#define LCD_RST_SW_PORT     GPIOA
#define LCD_RST_SW_PIN      GPIO_Pin_3

/* LCD control pins (ST7567) */
#define LCD_A0_PORT         GPIOB
#define LCD_A0_PIN          GPIO_Pin_4
#define LCD_CS_PORT         GPIOB
#define LCD_CS_PIN          GPIO_Pin_5
#define LCD_RST_PORT        GPIOB
#define LCD_RST_PIN         GPIO_Pin_6

/* NRF24L01 control pins */
#define NRF_CS_PORT         GPIOB
#define NRF_CS_PIN          GPIO_Pin_1
#define NRF_CE_PORT         GPIOB
#define NRF_CE_PIN          GPIO_Pin_2
#define NRF_PWR_EN_PORT     GPIOB
#define NRF_PWR_EN_PIN      GPIO_Pin_3
#define NRF_IRQ_PORT        GPIOB
#define NRF_IRQ_PIN         GPIO_Pin_10
#define NRF24_IRQ_EXT_LINE  EXTI_Line10

/* WS2811 LED strip data pin (bit-banged) */
#define WS2811_PORT         GPIOA
#define WS2811_PIN          GPIO_Pin_8

/* TIM3 CH4 buzzer output pin */
#define BUZZER_TIM_PORT     GPIOB
#define BUZZER_TIM_PIN      GPIO_Pin_1

/* TIM4 encoder inputs (CH1=PB6, CH2=PB7) */
#define ENC_PORT            GPIOB
#define ENC_CH1_PIN         GPIO_Pin_6
#define ENC_CH2_PIN         GPIO_Pin_7

/* I2C1 pins (remapped: PB8=SCL, PB9=SDA) -- TODO: verify; remapped option avoids conflict with TIM4 encoder on PB6/PB7 */
#define I2C1_PORT       GPIOB
#define I2C1_SCL_PIN    GPIO_Pin_8
#define I2C1_SDA_PIN    GPIO_Pin_9

/* ADC channels for analog joystick/pot inputs -- TODO: verify against schematic */
#define ADC_STEERING_CHANNEL        ADC_Channel_4   /* PA4 */
#define ADC_STEERING_TRIM_CHANNEL   ADC_Channel_5   /* PA5 -- placeholder, verify */
#define ADC_THROTTLE_CHANNEL        ADC_Channel_6   /* PA6 -- placeholder, verify */

#endif /* HAL_CONFIG_H */
