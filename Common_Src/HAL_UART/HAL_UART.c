/*! \file
*               Author: mstewart
*   \brief      HAL uart module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_UART.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC HAL_USART_func_type HAL_USART1_func_p = NULL_P;
STATIC HAL_USART_func_type HAL_USART2_func_p = NULL_P;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Module (re-)initialisation function
*
*   \author        MS
*
*   \return        none
*
*   \note          Fixed baudrate for now at 115200 8N1
*
***************************************************************************************************/
void HAL_USART1_init( void )
{
	/* Enable USART1 clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE );

	/* NVIC Configuration */
	NVIC_InitTypeDef NVIC_InitStructure;
	/* Enable the USARTx Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init( &NVIC_InitStructure );

	/* Configure the USART1 */
	USART_InitTypeDef USART_InitStructure;

	/* USART1 configuration ------------------------------------------------------*/
	/* USART1 configured as follow:
		- BaudRate = 115200 baud
		- Word Length = 8 Bits
		- One Stop Bit
		- No parity
		- Hardware flow control disabled (RTS and CTS signals)
		- Receive and transmit enabled
		- USART Clock disabled
		- USART CPOL: Clock is active low
		- USART CPHA: Data is captured on the middle
		- USART LastBit: The clock pulse of the last data bit is not output to
			the SCLK pin
	 */
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init( USART1, &USART_InitStructure );

	/* Enable USART1 */
	USART_Cmd( USART1, ENABLE );

	/* Enable the USART2 Receive interrupt: this interrupt is generated when the
		USART2 receive data register is not empty */
	USART_ITConfig( USART1, USART_IT_RXNE, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Close the serial port
*
*   \author        MS
*
*   \return        none
*
*   \note         
*
***************************************************************************************************/
void HAL_USART1_close( void )
{
	USART_DeInit( USART1 );

	/* Disable USART1 */
	USART_Cmd( USART1, DISABLE );

	/* Disable the USART1 Receive interrupt */
	USART_ITConfig( USART1, USART_IT_RXNE, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Setup the callback
*
*   \author        MS
*
*   \return        none
*
*   \note         
*
***************************************************************************************************/
void HAL_USART1_set_callback( HAL_USART_func_type func_p )
{
	if( func_p != NULL_P )
	{
		HAL_USART1_func_p = func_p;
	}
}


/*!
****************************************************************************************************
*
*   \brief         Writes a buffer of information out to UART
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_USART1_send_data( const char* data, u16_t data_size )
{
	u16_t byte_index = 0u;

    while( data_size )
    {
        USART_SendData( USART1, data[byte_index++] );
        while( USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET );
        data_size--;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Writes a string out to UART
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_USART1_send_string( const char* data )
{
	HAL_USART1_send_data( data, STDC_strlen(data) );
}


/*!
****************************************************************************************************
*
*   \brief         Module (re-)initialisation function
*
*   \author        MS
*
*   \return        none
*
*   \note          Fixed baudrate for now at 9600 8N1
*
***************************************************************************************************/
void HAL_USART2_init( void )
{
	/* Enable GPIOA clock, should be enabled anyway but just in case */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE);

	/* Enable USART2 clock */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE );

	/* Configure PA2 (TX) as AF push-pull, PA3 (RX) as floating input */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin   = ESP_TX_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init( ESP_UART_PORT, &GPIO_InitStructure );

	GPIO_InitStructure.GPIO_Pin  = ESP_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init( ESP_UART_PORT, &GPIO_InitStructure );

	/* NVIC Configuration */
	NVIC_InitTypeDef NVIC_InitStructure;
	/* Enable the USARTx Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init( &NVIC_InitStructure );

	/* Configure the USART2 */
	USART_InitTypeDef USART_InitStructure;

	/* USART2 configuration ------------------------------------------------------*/
	/* USART2 configured as follow:
		- BaudRate = 19200 baud
		- Word Length = 8 Bits
		- One Stop Bit
		- No parity
		- Hardware flow control disabled (RTS and CTS signals)
		- Receive and transmit enabled
		- USART Clock disabled
		- USART CPOL: Clock is active low
		- USART CPHA: Data is captured on the middle
		- USART LastBit: The clock pulse of the last data bit is not output to
			the SCLK pin
	 */
	USART_InitStructure.USART_BaudRate = 19200;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init( USART2, &USART_InitStructure );

	/* Enable USART2 */
	USART_Cmd( USART2, ENABLE );

	/* Enable the USART2 Receive interrupt: this interrupt is generated when the
		USART2 receive data register is not empty */
	USART_ITConfig( USART2, USART_IT_RXNE, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Close the serial port
*
*   \author        MS
*
*   \return        none
*
*   \note         
*
***************************************************************************************************/
void HAL_USART2_close( void )
{
	USART_DeInit( USART2 );

	/* Disable USART2 */
	USART_Cmd( USART2, DISABLE );

	RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, DISABLE );

	/* Disable the USART2 Receive interrupt */
	USART_ITConfig( USART2, USART_IT_RXNE, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Setup the callback
*
*   \author        MS
*
*   \return        none
*
*   \note         
*
***************************************************************************************************/
void HAL_USART2_set_rx_callback( HAL_USART_func_type func_p )
{
	if( func_p != NULL_P )
	{
		HAL_USART2_func_p = func_p;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Writes a buffer of information out to UART
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_USART2_send_data( const char* data, u16_t data_size )
{
	u16_t byte_index = 0u;

    while( data_size )
    {
        USART_SendData( USART2, data[byte_index++] );
        while( USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET );
        data_size--;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Writes a string out to UART
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_USART2_send_string( const char* data )
{
	HAL_USART2_send_data( data, STDC_strlen(data) );
}

///***************************************************************************************************
//**                              ISR Handlers                                                      **
//***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Handles user input from the terminal
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void USART1_IRQHandler( void )
{	
	if( HAL_USART1_func_p != NULL_P )
	{
		HAL_USART1_func_p( (u8_t)USART_ReceiveData( USART1 ) );
	}
}

/*!
****************************************************************************************************
*
*   \brief         Handle data from the ESP01 chip
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void USART2_IRQHandler( void )
{	
	if( HAL_USART2_func_p != NULL_P )
	{
		HAL_USART2_func_p( (u8_t)USART_ReceiveData( USART2 ) );
	}
}

/******************************** END OF FILE *******************************************************/
