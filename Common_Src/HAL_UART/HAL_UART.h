
#ifndef HAL_UART_H
#define HAL_UART_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "HAL_config.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/*!< TX buffer size, per UART.
 *
 *   One message is in flight at a time and every send restarts from index 0, so this only has to
 *   be as large as the biggest single write a caller makes - there is no ring and no power-of-two
 *   constraint. A write longer than this is truncated to what fits.
 *
 *   USART2 carries the ESP01 link, whose largest write is ESP01_MAX_TX_SIZE (512), so 512 makes
 *   truncation impossible for its only caller. */
#define HAL_USART1_TX_BUFFER_SIZE   ( 128u )
#define HAL_USART2_TX_BUFFER_SIZE   ( 512u )

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef void(*HAL_USART_func_type)(u8_t received_byte); 

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void HAL_USART1_init( void );
void HAL_USART2_init( void );
void HAL_USART1_close( void );
void HAL_USART2_close( void );
void HAL_USART1_set_rx_callback( HAL_USART_func_type func_p );
void HAL_USART2_set_rx_callback( HAL_USART_func_type func_p );
void HAL_USART1_send_data( u8_t* data, u16_t data_size );
void HAL_USART2_send_data( u8_t* data, u16_t data_size );
void HAL_USART1_send_string( u8_t* data );
void HAL_USART2_send_string(u8_t* data );

/*!< TRUE while bytes are still to send or the last one is still shifting out of the transmitter.
 *   send_data() copies the message out and returns, so the caller's buffer is free the moment it
 *   returns - these are only needed by callers that must know the wire itself is idle (before a
 *   baud change, powering the peripheral down, or entering a sleep mode that stops the clock). */
false_true_et HAL_USART1_tx_busy( void );
false_true_et HAL_USART2_tx_busy( void );

#endif /* HAL_UART_PUB_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
