/*! \file
*               Author: mstewart
*   \brief      HAL_SPI module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_I2C.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
u32_t HAL_I2C1_timeout_s;

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
*   \note
*
***************************************************************************************************/
void HAL_I2C1_init( void )
{
	/* Enable I2C and GPIOA clock, should be enabled anyway but just in case */
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_I2C1, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB, ENABLE );
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_AFIO,  ENABLE );

	/* I2C1 defaults to PB6/PB7 on this part. Those are TIM4's encoder channels here, so the
	   peripheral MUST be remapped onto PB8/PB9 - without this the I2C engine drives the encoder
	   pins and the pins actually wired to the display are left connected to nothing. */
	GPIO_PinRemapConfig( GPIO_Remap_I2C1, ENABLE );

	I2C_SoftwareResetCmd( I2C1, ENABLE );
	I2C_SoftwareResetCmd( I2C1, DISABLE );

	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure I2C_EE pins: SCL and SDA */
	GPIO_InitStructure.GPIO_Pin = I2C1_SDA_PIN | I2C1_SCL_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_Init( I2C1_PORT, &GPIO_InitStructure );

	I2C_InitTypeDef  I2C_InitStructure;

	/* I2C configuration */
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x38;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	/* 400kHz fast mode. The SH1106 pushes a 128-byte page per transfer and SH1106_tick() may send
	   two of them in one 10ms slot; at 100kHz a single page is ~11.5ms of bus time on its own and
	   would overrun the slot. 400kHz brings that to ~2.9ms per page. */
	I2C_InitStructure.I2C_ClockSpeed = 400000u;

	/* I2C Peripheral Enable */
	I2C_Cmd( I2C1, ENABLE );
	
	/* Apply I2C configuration after enabling it */
	I2C_Init(I2C1, &I2C_InitStructure);

	I2C_AcknowledgeConfig( I2C1, ENABLE );

	HAL_I2C1_timeout_s = 0u;
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
*   \note
*
***************************************************************************************************/
void HAL_I2C1_de_init( void )
{
	/* De-init the I2C module */
	I2C_DeInit( I2C1 );

	/* I2C Peripheral Disable */
	I2C_Cmd( I2C1, DISABLE );

	/* Disable I2C clock*/
	RCC_APB1PeriphClockCmd( RCC_APB1Periph_I2C1, DISABLE );
}

/*!
****************************************************************************************************
*
*   \brief         Writes multiple registers
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
pass_fail_et HAL_I2C_write_registers( u8_t dev_add, u8_t reg_address, u8_t* data, u8_t num_bytes )
{
	u8_t i = 0u;
	pass_fail_et status = PASS;

	I2C_GenerateSTART(I2C1,ENABLE);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_MODE_SELECT);

	I2C_Send7bitAddress(I2C1, ( dev_add << 1u ), I2C_Direction_Transmitter);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);

	I2C_SendData(I2C1, reg_address );
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);

	for( i = 0; i < num_bytes; i++ )
	{
		I2C_SendData(I2C1, data[i] );
		status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);
	}

	I2C_GenerateSTOP(I2C1,ENABLE);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);

	return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Reads multiple registers
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
pass_fail_et HAL_I2C_read_registers( u8_t dev_add, u8_t reg_address, u8_t* data, u8_t num_bytes )
{
	u8_t i = 0u;
	pass_fail_et status = PASS;

	I2C_AcknowledgeConfig(I2C1,ENABLE);

	I2C_GenerateSTART(I2C1,ENABLE);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_MODE_SELECT);

	I2C_Send7bitAddress(I2C1, ( dev_add << 1u ), I2C_Direction_Transmitter);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED);

	I2C_SendData(I2C1, reg_address);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_TRANSMITTED);

	I2C_GenerateSTART(I2C1,ENABLE);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_MODE_SELECT);

	I2C_Send7bitAddress(I2C1, ( dev_add << 1u ), I2C_Direction_Receiver);
	status = HAL_I2C_check_event(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED);

	for( i = 0; i < num_bytes ; i++ )
	{
		status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_RECEIVED);
		data[i] = I2C_ReceiveData(I2C1);
	}

	I2C_GenerateSTOP(I2C1, ENABLE);
	I2C_AcknowledgeConfig(I2C1, DISABLE);

	status = HAL_I2C_check_event(I2C_EVENT_MASTER_BYTE_RECEIVED);
	I2C_ReceiveData(I2C1);

	return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Raw I2C read — no register write phase
*
*   \author        MS
*
*   \param[in]     dev_add   - 7-bit device address
*   \param[out]    data      - Buffer to receive bytes
*   \param[in]     num_bytes - Number of bytes to read
*
*   \return        PASS if read successful; FAIL otherwise
*
*   \note          Issues START | ADDR+R | data | STOP with no preceding register write.
*                  Required by sensors such as AHT20 that return data on a plain read.
*
***************************************************************************************************/
pass_fail_et HAL_I2C_raw_read( u8_t dev_add, u8_t* data, u8_t num_bytes )
{
    u8_t         i      = 0u;
    pass_fail_et status = PASS;

    I2C_AcknowledgeConfig( I2C1, ENABLE );

    I2C_GenerateSTART( I2C1, ENABLE );
    status = HAL_I2C_check_event( I2C_EVENT_MASTER_MODE_SELECT );

    I2C_Send7bitAddress( I2C1, ( dev_add << 1u ), I2C_Direction_Receiver );
    status = HAL_I2C_check_event( I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED );

    for( i = 0u; i < num_bytes; i++ )
    {
        status = HAL_I2C_check_event( I2C_EVENT_MASTER_BYTE_RECEIVED );
        data[i] = I2C_ReceiveData( I2C1 );
    }

    I2C_GenerateSTOP( I2C1, ENABLE );
    I2C_AcknowledgeConfig( I2C1, DISABLE );

    status = HAL_I2C_check_event( I2C_EVENT_MASTER_BYTE_RECEIVED );
    I2C_ReceiveData( I2C1 );

    return( status );
}

/*!
****************************************************************************************************
*
*   \brief         Check the I2C event with a timeout
*
*   \author        MS
*
*   \return        none
*
*   \note          Also polled for I2C_FLAG_AF (acknowledge failure, ie. a NACK) so a missing
*                  device fails as soon as hardware reports it rather than only once the 1000
*                  iteration budget runs out. AF is cleared here when seen - it is sticky in
*                  hardware and would otherwise persist into whatever the caller does next.
*
*                  HAL_I2C1_timeout_s is reset here rather than once per multi-byte transaction -
*                  every wait gets its own full budget instead of a long transfer's later bytes
*                  drawing down whatever earlier bytes left behind. Deliberately the only change
*                  from this function's previous shape: callers still ignore the returned status
*                  and carry on to the next step regardless, exactly as before - that turned out to
*                  be load-bearing (see sh1106_write_data() in SH1106.c for the story), so it stays.
*
***************************************************************************************************/
pass_fail_et HAL_I2C_check_event( u32_t event )
{
	pass_fail_et status;

	HAL_I2C1_timeout_s = 0u;

	while( ( !I2C_CheckEvent(I2C1, (uint32_t)event) ) &&
	       ( I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != SET ) &&
	       ( HAL_I2C1_timeout_s < 1000u ) )
	{
		HAL_I2C1_timeout_s++;
	}

	if( I2C_CheckEvent(I2C1, (uint32_t)event) == SUCCESS )
	{
		status = PASS;
	}
	else
	{
		status = FAIL;

		if( I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET )
		{
			I2C_ClearFlag( I2C1, I2C_FLAG_AF );
		}
	}

	return( status );
}

///***************************************************************************************************
//**                              ISR Handlers                                                      **
//***************************************************************************************************/
/* None */

///****************************** END OF FILE *******************************************************/
