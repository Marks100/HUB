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

/* One message in flight at a time - a plain buffer and a send index, no ring.
   Every send starts again from index 0, so there is no wrap, no mask, and no power-of-two
   constraint on the size.

   The copy into these buffers is what makes the send asynchronous safely. The driver cannot
   simply hold the caller's pointer: esp01_send_command() clears its output buffer on the line
   after the send returns, and send_string() is routinely handed a local, so a zero-copy
   transmitter would be reading memory that had already been wiped or gone out of scope.

   idx and len are deliberately plain module-level statics rather than members of a struct:
   -fpack-struct=1 would be free to place a struct member on an odd address, and an unaligned
   halfword that straddles a word boundary is not a single atomic bus transfer on Cortex-M3, so
   they could tear between the thread and the ISR. Standalone statics keep natural alignment. */
STATIC u8_t           HAL_USART1_tx_buf_s[HAL_USART1_TX_BUFFER_SIZE];
STATIC volatile u16_t HAL_USART1_tx_len_s = 0u;   /* Bytes in this message */
STATIC volatile u16_t HAL_USART1_tx_idx_s = 0u;   /* Next byte the ISR will send; idx >= len means idle */

STATIC u8_t           HAL_USART2_tx_buf_s[HAL_USART2_TX_BUFFER_SIZE];
STATIC volatile u16_t HAL_USART2_tx_len_s = 0u;
STATIC volatile u16_t HAL_USART2_tx_idx_s = 0u;

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
	/* Priority 1 - below the CPS input ISR (EXTI2, priority 0) so it can preempt this */
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
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

	/* Drop anything a previous session left queued, and make sure the transmitter starts
	   disarmed - there is nothing to send, so an armed TXEIE would re-enter the handler immediately */
	USART1->CR1          &= ~USART_CR1_TXEIE;
	HAL_USART1_tx_idx_s  = 0u;
	HAL_USART1_tx_len_s  = 0u;

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

	/* Disarm the transmitter and discard anything still queued - the peripheral is going away,
	   so a pending TXEIE would leave the ISR unable to ever finish the message */
	USART_ITConfig( USART1, USART_IT_TXE, DISABLE );
	HAL_USART1_tx_idx_s = 0u;
	HAL_USART1_tx_len_s = 0u;
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
*   \brief         Queues a buffer of information for transmission over UART
*
*   \author        MS
*
*   \return        none
*
*   \note          Copies into the TX buffer and returns - the caller's buffer is free immediately,
*                  and the bytes are shifted out by USART1_IRQHandler(). Blocks only if a previous
*                  message is still going out, which callers do not do.
*
***************************************************************************************************/
void HAL_USART1_send_data( u8_t* data, u16_t data_size )
{
	if( ( data != NULL_P ) && ( data_size != 0u ) )
	{
		u16_t len = data_size;

		if( len > HAL_USART1_TX_BUFFER_SIZE )
		{
			len = HAL_USART1_TX_BUFFER_SIZE;   /* Longer than the buffer - send what fits */
		}

		/* Wait for the previous message to finish. Callers send one message at a time so this
		   should never actually spin, but overwriting a buffer the ISR is midway through
		   would corrupt the wire. */
		while( HAL_USART1_tx_idx_s < HAL_USART1_tx_len_s )
		{
			/* Previous transmission still in progress */
		}

		STDC_memcpy( HAL_USART1_tx_buf_s, data, len );

		/* idx before len: an ISR landing between the two sees idx < the old len and sends
		   buf[0], which already holds the new message. Writing len first would briefly leave
		   idx >= len and disarm the transmitter. */
		HAL_USART1_tx_idx_s = 0u;
		HAL_USART1_tx_len_s = len;

		USART1->CR1 |= USART_CR1_TXEIE;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Reports whether the transmitter still has work outstanding
*
*   \author        MS
*
*   \return        TRUE if bytes are queued or the last byte is still on the wire
*
***************************************************************************************************/
false_true_et HAL_USART1_tx_busy( void )
{
	false_true_et busy = FALSE;

	/* Bytes still to send, or the shift register has not finished the last one. TC (not TXE) is
	   the flag that means the stop bit is actually out. */
	if( ( HAL_USART1_tx_idx_s < HAL_USART1_tx_len_s ) ||
	    ( ( USART1->SR & USART_SR_TC ) == 0u ) )
	{
		busy = TRUE;
	}

	return( busy );
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
void HAL_USART1_send_string( u8_t* data )
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
	/* Priority 1 - below the CPS input ISR (EXTI2, priority 0) so it can preempt this */
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
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

	/* Drop anything a previous session left queued, and make sure the transmitter starts
	   disarmed - there is nothing to send, so an armed TXEIE would re-enter the handler immediately */
	USART2->CR1          &= ~USART_CR1_TXEIE;
	HAL_USART2_tx_idx_s  = 0u;
	HAL_USART2_tx_len_s  = 0u;

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

	/* Disarm the transmitter and discard anything still queued - the peripheral is going away,
	   so a pending TXEIE would leave the ISR unable to ever finish the message */
	USART_ITConfig( USART2, USART_IT_TXE, DISABLE );
	HAL_USART2_tx_idx_s = 0u;
	HAL_USART2_tx_len_s = 0u;
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
*   \brief         Queues a buffer of information for transmission over UART
*
*   \author        MS
*
*   \return        none
*
*   \note          Copies into the TX buffer and returns - the caller's buffer is free immediately,
*                  and the bytes are shifted out by USART2_IRQHandler(). Blocks only if a previous
*                  message is still going out, which callers do not do.
*
*                  This is what lets esp01_send_command() clear its output buffer on the very
*                  next line: the data has already been copied out by the time this returns.
*
***************************************************************************************************/
void HAL_USART2_send_data( u8_t* data, u16_t data_size )
{
	if( ( data != NULL_P ) && ( data_size != 0u ) )
	{
		u16_t len = data_size;

		if( len > HAL_USART2_TX_BUFFER_SIZE )
		{
			len = HAL_USART2_TX_BUFFER_SIZE;   /* Longer than the buffer - send what fits */
		}

		STDC_memcpy( HAL_USART2_tx_buf_s, data, len );

		/* idx before len: an ISR landing between the two sees idx < the old len and sends
		   buf[0], which already holds the new message. Writing len first would briefly leave
		   idx >= len and disarm the transmitter. */
		HAL_USART2_tx_idx_s = 0u;
		HAL_USART2_tx_len_s = len;

		USART2->CR1 |= USART_CR1_TXEIE;
	}
}

/*!
****************************************************************************************************
*
*   \brief         Reports whether the transmitter still has work outstanding
*
*   \author        MS
*
*   \return        TRUE if bytes are queued or the last byte is still on the wire
*
***************************************************************************************************/
false_true_et HAL_USART2_tx_busy( void )
{
	false_true_et busy = FALSE;

	/* Bytes still to send, or the shift register has not finished the last one. TC (not TXE) is
	   the flag that means the stop bit is actually out. */
	if( ( HAL_USART2_tx_idx_s < HAL_USART2_tx_len_s ) ||
	    ( ( USART2->SR & USART_SR_TC ) == 0u ) )
	{
		busy = TRUE;
	}

	return( busy );
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
void HAL_USART2_send_string( u8_t* data )
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
	/* Direct USART1->SR/DR access instead of the SPL's USART_GetITStatus()/USART_ReceiveData() -
	 * this build has no LTO, so those are real, avoidable non-inlined calls (they are just masked
	 * register reads) on every byte in both directions, same reasoning as CPS's direct EXTI->PR
	 * access. SR is sampled once: reading it repeatedly would race with the hardware. */
	u16_t status = (u16_t)USART1->SR;

	/* Reading DR is what clears RXNE, so it must happen only when RXNE is genuinely set. The
	 * previous implementation read DR unconditionally, which was correct only while RXNE was the
	 * sole enabled source of this interrupt - now that TXE also raises it, an unguarded read
	 * would invent bytes and hand them to the receive callback. */
	if( ( status & USART_SR_RXNE ) != 0u )
	{
		u8_t rx_byte = (u8_t)( USART1->DR & 0x01FFu );

		if( HAL_USART1_func_p != NULL_P )
		{
			HAL_USART1_func_p( rx_byte );
		}
	}

	/* TXE on its own is not enough to act on - it sits set whenever the data register is empty,
	 * which is most of the time. Only an enabled TXEIE means this driver actually wants to send. */
	if( ( ( status & USART_SR_TXE ) != 0u ) && ( ( USART1->CR1 & USART_CR1_TXEIE ) != 0u ) )
	{
		if( HAL_USART1_tx_idx_s < HAL_USART1_tx_len_s )
		{
			USART1->DR = HAL_USART1_tx_buf_s[HAL_USART1_tx_idx_s];   /* the write clears TXE */
			HAL_USART1_tx_idx_s++;
		}
		else
		{
			/* Message sent. TXE stays set until DR is written, so leaving TXEIE armed here would
			 * re-enter this handler forever and lock the CPU out. */
			USART1->CR1 &= ~USART_CR1_TXEIE;
		}
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
	/* Direct USART2->SR/DR access instead of the SPL equivalents - see USART1_IRQHandler().
	 * SR is sampled once: reading it repeatedly would race with the hardware. */
	u16_t status = (u16_t)USART2->SR;

	/* Reading DR is what clears RXNE, so it must happen only when RXNE is genuinely set. The
	 * previous implementation read DR unconditionally, which was correct only while RXNE was the
	 * sole enabled source of this interrupt - now that TXE also raises it, an unguarded read
	 * would invent bytes and feed them straight into the ESP01 frame parser. */
	if( ( status & USART_SR_RXNE ) != 0u )
	{
		u8_t rx_byte = (u8_t)( USART2->DR & 0x01FFu );

		if( HAL_USART2_func_p != NULL_P )
		{
			HAL_USART2_func_p( rx_byte );
		}
	}

	/* TXE on its own is not enough to act on - it sits set whenever the data register is empty,
	 * which is most of the time. Only an enabled TXEIE means this driver actually wants to send. */
	if( ( ( status & USART_SR_TXE ) != 0u ) && ( ( USART2->CR1 & USART_CR1_TXEIE ) != 0u ) )
	{
		if( HAL_USART2_tx_idx_s < HAL_USART2_tx_len_s )
		{
			USART2->DR = HAL_USART2_tx_buf_s[HAL_USART2_tx_idx_s];   /* the write clears TXE */
			HAL_USART2_tx_idx_s++;
		}
		else
		{
			/* Message sent. TXE stays set until DR is written, so leaving TXEIE armed here would
			 * re-enter this handler forever and lock the CPU out. */
			USART2->CR1 &= ~USART_CR1_TXEIE;
		}
	}
}

/******************************** END OF FILE *******************************************************/
