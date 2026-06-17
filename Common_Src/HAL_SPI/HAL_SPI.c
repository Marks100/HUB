/*! \file
*               Author: mstewart
*   \brief      HAL_SPI module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_SPI.h"

/* Module Identification for STDC_assert functionality */
#define STDC_MODULE_ID STDC_MOD_HAL_SPI

/***************************************************************************************************
**                              Data declaratons and definitions                                 **
***************************************************************************************************/
/* None */

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
*   \note          Fixed baudrate for now at 5MHz 8N1
*
***************************************************************************************************/
void HAL_SPI1_init( void )
{
	/* Enable SPI clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_SPI1, ENABLE );

	/* Enable the GPIOA clock */
	RCC_APB2PeriphClockCmd( GPIO_Speed_10MHz, ENABLE );

	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Setup the SPI pins  */
	GPIO_InitStructure.GPIO_Pin = SPI1_SCK_PIN | SPI1_MISO_PIN | SPI1_MOSI_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init( SPI1_PORT, &GPIO_InitStructure );

	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 0xAAAA;

	SPI_Init( SPI1, &SPI_InitStructure );

	/* Enable SPI1 */
	SPI_Cmd( SPI1, ENABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Module (de-)initialisation function
*
*   \author        MS
*
*   \return        none
*
*   \note          Fixed baudrate for now at 5MHz 8N1
*
***************************************************************************************************/
void HAL_SPI1_de_init( void )
{
	/* Disable SPI1 */
	SPI_Cmd( SPI1, DISABLE );
	SPI_I2S_DeInit( SPI1 );

	/* Disable SPI clock */
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_SPI1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Writes a buffer of information out over SPI
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_SPI1_write_and_read_data( u8_t* tx_data, u8_t* rx_data, u8_t num_bytes )
{
	u8_t i = 0u;

	for( i=0u; i<num_bytes; i++ )
	{
		/* Send SPI1 data */
		SPI_I2S_SendData( SPI1, tx_data[i] );
		
		/* Wait for the SPI busy flag to clear */
		while( SPI_I2S_GetFlagStatus( SPI1, SPI_I2S_FLAG_BSY ) == SET );

		if( rx_data != NULL_P )
		{
			rx_data[i] = SPI_I2S_ReceiveData( SPI1 );
		}
		else
		{
			(void)SPI_I2S_ReceiveData( SPI1 );
		}
	}
}

/*!
****************************************************************************************************
*
*   \brief
*
*   \author        MS
*
*   \return        none
*
***************************************************************************************************/
void HAL_SPI1_clear_receive_spi_buffer( void )
{
}

void HAL_SPI2_init( void )
{
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_SPI2, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );

	GPIO_InitTypeDef GPIO_InitStructure;

	/* SCK and MOSI as alternate-function push-pull outputs */
	GPIO_InitStructure.GPIO_Pin   = SPI2_SCK_PIN | SPI2_MOSI_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_Init( SPI2_PORT, &GPIO_InitStructure );

	/* MISO as floating input */
	GPIO_InitStructure.GPIO_Pin  = SPI2_MISO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init( SPI2_PORT, &GPIO_InitStructure );

	SPI_InitTypeDef SPI_InitStructure;
	SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
	SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial     = 0xAAAA;
	SPI_Init( SPI2, &SPI_InitStructure );

	SPI_Cmd( SPI2, ENABLE );
}

void HAL_SPI2_de_init( void )
{
	SPI_Cmd( SPI2, DISABLE );
	SPI_I2S_DeInit( SPI2 );
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_SPI2, DISABLE );
}

void HAL_SPI1_send_byte( u8_t data )
{
	SPI_I2S_SendData( SPI1, data );
	while( SPI_I2S_GetFlagStatus( SPI1, SPI_I2S_FLAG_BSY ) == SET );
	(void)SPI_I2S_ReceiveData( SPI1 );
}

void HAL_SPI2_write_and_read_data( u8_t* tx_data, u8_t* rx_data, u16_t num_bytes )
{
	u16_t i = 0u;

	for( i = 0u; i < num_bytes; i++ )
	{
		SPI_I2S_SendData( SPI2, tx_data[i] );

		while( SPI_I2S_GetFlagStatus( SPI2, SPI_I2S_FLAG_BSY ) == SET );

		if( rx_data != NULL_P )
		{
			rx_data[i] = SPI_I2S_ReceiveData( SPI2 );
		}
		else
		{
			(void)SPI_I2S_ReceiveData( SPI2 );
		}
	}
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              ISR Handlers                                                      **
***************************************************************************************************/
/* None */

/****************************** END OF FILE *******************************************************/
