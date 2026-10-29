#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

#include "stm32f10x.h"

/***************************************************************************************************
**  Hardware Variant Selection
**  Define exactly one variant here.
***************************************************************************************************/
#define HW_VARIANT_BLUE_PILL    1u
#define HW_VARIANT_SUPER_PILL   2u

#define HW_VARIANT              HW_VARIANT_SUPER_PILL

/***************************************************************************************************
**  Per-Variant Pin Assignments
***************************************************************************************************/
#if ( HW_VARIANT == HW_VARIANT_BLUE_PILL )
 
#define ONBOARD_LED_PORT        GPIOC
#define ONBOARD_LED_PIN         GPIO_Pin_13

#elif ( HW_VARIANT == HW_VARIANT_SUPER_PILL )

#define ONBOARD_LED_PORT        GPIOA
#define ONBOARD_LED_PIN         GPIO_Pin_1
#define ONBOARD_BTN_PORT        GPIOA
#define ONBOARD_BTN_PIN         GPIO_Pin_8

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

/* SH1106 OLED panel buttons - wired to GND, internal pull-ups, so LOW means pressed.
 * PB13-15 chosen because they are the only contiguous free block on this part: SPI2 is unused,
 * they carry no alternate function this project needs, and they avoid PA13/PA14 (SWDIO/SWCLK)
 * which must stay available for the debugger. */
#define PANEL_SELECT_BTN_PORT   GPIOB
#define PANEL_SELECT_BTN_PIN    GPIO_Pin_13
#define PANEL_CONFIRM_BTN_PORT  GPIOB
#define PANEL_CONFIRM_BTN_PIN   GPIO_Pin_14
#define PANEL_BACK_BTN_PORT     GPIOB
#define PANEL_BACK_BTN_PIN      GPIO_Pin_15

/* TIM3 CH4 buzzer output pin */
#define BUZZER_TIM_PORT     GPIOB
#define BUZZER_TIM_PIN      GPIO_Pin_1

/* TIM4 encoder inputs (CH1=PB6, CH2=PB7) */
#define ENC_PORT            GPIOB
#define ENC_CH1_PIN         GPIO_Pin_6
#define ENC_CH2_PIN         GPIO_Pin_7

/* Crank Position Sensor (CPS) tooth input - EXTI Line 3. PB3 chosen because SWJ_JTAGDisable
 * (see HAL_BRD_init()) frees it from its JTDO alternate function, same reasoning PA15 was
 * freed for NRF_IRQ. This is the board's highest-priority (0) NVIC slot - see
 * HAL_BRD_init()'s priority scheme comment. */
#define CPS_CRANK_INPUT_PORT            GPIOB
#define CPS_CRANK_INPUT_PIN             GPIO_Pin_3
#define CPS_CRANK_INPUT_EXTI_LINE       EXTI_Line3
#define CPS_CRANK_INPUT_EXTI_PORT_SRC   GPIO_PortSourceGPIOB
#define CPS_CRANK_INPUT_EXTI_PIN_SRC    GPIO_PinSource3

/* SENT_TX sensor emulator output - PB12. SPI2 is unused and PB12 carries no alternate function this
 * project needs, and HAL_BRD_init() claims no other use of it (PB13-15 are the panel buttons).
 * Push-pull and idle HIGH, as a SENT line idles high. Driven from the TIM1 update interrupt, see
 * APP/Src/SENT_CFG. */
#define SENT_TX_OUT_PORT    GPIOB
#define SENT_TX_OUT_PIN     GPIO_Pin_12
/* I2C1 pins (remapped: PB8=SCL, PB9=SDA) */
#define I2C1_PORT           GPIOB
#define I2C1_SCL_PIN        GPIO_Pin_8
#define I2C1_SDA_PIN        GPIO_Pin_9

/* ADC channels for analog joystick/pot inputs */
#define ADC_STEERING_CHANNEL        ADC_Channel_4   /* PA4 */
#define ADC_STEERING_TRIM_CHANNEL   ADC_Channel_5   /* PA5 -- placeholder, verify. NOTE: PA5 is also
                                                       SPI1_SCK_PIN (SPI1 - shared bus, above) - same
                                                       conflict ADC_THROTTLE_CHANNEL had, not yet moved */

/* APS throttle pedal - PA0 (ADC_Channel_0), the only free ADC1-capable pin left on this board.
   ADC1 only reaches PA0-PA7/PB0-PB1/PC0-PC5, and this is a Blue Pill-family part (see
   CLK_STM32F1.c) whose standard 40-pin header exposes just PC13-PC15 out of GPIOC - not remotely
   ADC-capable - so PC0-PC5 are physically unreachable, not merely "unused". Of what's left,
   PA4 is ADC_STEERING_CHANNEL, PA5/6/7 are SPI1_SCK_PIN/SPI1_MISO_PIN/SPI1_MOSI_PIN (SPI1 -
   shared bus, above - a live bus driving the OLED panel and NRF24 radio), and PB0/PB1 are
   NRF_CE_PIN/BUZZER_TIM_PIN. Originally PA6 - moved off it rather than fighting SPI1 for the pin.
   ADC_THROTTLE_PORT/PIN is what HAL_BRD_init() configures GPIO_Mode_AIN. There is no free
   ADC-capable pin left for ch2 at all right now, so it stays unwired: aps_pedal_cfg_s
   (VEHICLE_SIM_config.c) has ch2_read_func_p = NULL_P - see APS.h - until one of the pins above
   is freed up for it. */
#define ADC_THROTTLE_PORT           GPIOA
#define ADC_THROTTLE_PIN            GPIO_Pin_0
#define ADC_THROTTLE_CHANNEL        ADC_Channel_0   /* PA0 */
#define ADC_THROTTLE_CHANNEL_2      ADC_Channel_7   /* PA7 -- unusable placeholder: collides with
                                                       SPI1_MOSI_PIN and no free ADC pin exists to
                                                       move it to right now */

/* TJA1051 CAN transceiver */
#define TJA1051_EN_PORT             GPIOB
#define TJA1051_EN_PIN              GPIO_Pin_10

/* MCP2515 CAN controller (CAN2) on SPI1. PB11 is a free-pin placeholder - confirm the real CS pin. */
#define MCP2515_CS_PORT            GPIOB
#define MCP2515_CS_PIN              GPIO_Pin_11

/* bxCAN1 — PA11=RX, PA12=TX (default mapping, no remap required) */
#define CAN_TX_PORT                 GPIOA
#define CAN_TX_PIN                  GPIO_Pin_12
#define CAN_RX_PORT                 GPIOA
#define CAN_RX_PIN                  GPIO_Pin_11

#endif /* HAL_CONFIG_H */

/****************************** END OF FILE *******************************************************/