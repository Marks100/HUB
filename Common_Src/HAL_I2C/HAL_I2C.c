/*! \file
*               Author: mstewart
*   \brief      HAL_SPI module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "stm32f10x_rcc.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_gpio.h"
#include "misc.h"

#include "C_defs.h"
#include "STDC.h"
#include "COMPILER_defs.h"
#include "HAL_config.h"
#include "HAL_BRD.h"
#include "DBG_MGR.h"
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
 
	I2C_SoftwareResetCmd( I2C1, ENABLE );
	I2C_SoftwareResetCmd( I2C1, DISABLE );

	/* Configure the GPIOs */
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Configure I2C_EE pins: SCL and SDA */
	GPIO_InitStructure.GPIO_Pin = I2C1_SDA_PIN | I2C1_SCL_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_Init( I2C1_PORT, &GPIO_InitStructure );

	I2C_InitTypeDef  I2C_InitStructure;

	/* I2C configuration */
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x38;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 100000u;

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
*   \brief         Writes a single register
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
pass_fail_et HAL_I2C1_write_single_register( u8_t dev_add, u8_t reg_add, u8_t* data )
{
	pass_fail_et status = PASS;

	status = HAL_I2C_write_multiple_registers( dev_add, reg_add, data, 1u );

	return( status );
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
pass_fail_et HAL_I2C_write_multiple_registers( u8_t dev_add, u8_t reg_address, u8_t* data, u8_t num_bytes )
{
	u8_t i = 0u;
	pass_fail_et status = PASS;
	HAL_I2C1_timeout_s = 0u;

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

	return status;
}


/*!
****************************************************************************************************
*
*   \brief         Read single register
*
*   \author        MS
*
*   \return        none
*
*   \note
*
***************************************************************************************************/
pass_fail_et HAL_I2C1_read_single_register( u8_t dev_add, u8_t reg_add, u8_t* data )
{
	pass_fail_et status = PASS;

	status = HAL_I2C_read_multiple_registers( dev_add, reg_add, data, 1u );

	return status;
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
pass_fail_et HAL_I2C_read_multiple_registers( u8_t dev_add, u8_t reg_address, u8_t* data, u8_t num_bytes )
{
	u8_t i = 0u;
	pass_fail_et status = PASS;
	HAL_I2C1_timeout_s = 0u;

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

    HAL_I2C1_timeout_s = 0u;

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
*   \note
*
***************************************************************************************************/
pass_fail_et HAL_I2C_check_event( u32_t event )
{
	pass_fail_et status = PASS;

	while( !I2C_CheckEvent(I2C1, (uint32_t)event) )
	{
		HAL_I2C1_timeout_s++;

		if( HAL_I2C1_timeout_s == 1000u )
		{
			HAL_BRD_reset();
		}
	}

	return status;
}

///***************************************************************************************************
//**                              ISR Handlers                                                      **
//***************************************************************************************************/
/* None */

///****************************** END OF FILE *******************************************************/
