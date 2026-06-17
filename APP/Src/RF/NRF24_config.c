/*! \file
*               Author: mstewart
*   \brief      NRF24 config module
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "NRF24_config.h"
#include "NRF24.h"

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
const NRF24_reg_config_st NRF24_default_tx_config_c[] = 
{
    { CONFIG,       1u, { 0x3E } },
	//{ EN_AUTO_ACK,  1u, { 0x3F } },
	{ EN_AUTO_ACK,  1u, { 0x00 } },
	{ EN_RXADDR,    1u, { 0x03 } },
    { SETUP_AW,     1u, { 0x03 } },
	{ SETUP_RETR,   1u, { 0xD3 } },
	{ RF_CH,        1u, { 0x4B } },
	{ RF_SETUP,     1u, { 0x26 } },
	{ RX_ADDR_P0,   5u, { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 } },
	{ RX_ADDR_P1,   5u, { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 } },
	{ RX_ADDR_P2,   1u, { 0xC3 } },
	{ RX_ADDR_P3,   1u, { 0xC4 } },
	{ RX_ADDR_P4,   1u, { 0xC5 } },
	{ RX_ADDR_P5,   1u, { 0xC6 } },
	{ TX_ADDR,      5u, { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 } },
	{ RX_PW_P0,     1u, { 0x00 } },
	{ RX_PW_P1,     1u, { 0x00 } },
	{ RX_PW_P2,     1u, { 0x00 } },
	{ RX_PW_P3,     1u, { 0x00 } },
	{ RX_PW_P4,     1u, { 0x00 } },
	{ RX_PW_P5,     1u, { 0x00 } },
	{ DYNPD,        1u, { 0x01 } },
	{ FEATURE,      1u, { 0x07 } },
};

const NRF24_reg_config_st NRF24_HUB_config_c[] = 
{
    { CONFIG,       1u, { 0x3F } },
	{ EN_AUTO_ACK,  1u, { 0x01 } },
	{ EN_RXADDR,    1u, { 0x01 } },
    { SETUP_AW,     1u, { 0x01 } },
	{ SETUP_RETR,   1u, { 0xF6 } },
	{ RF_CH,        1u, { 0x78 } },
	{ RF_SETUP,     1u, { 0x26 } },
	{ RX_ADDR_P0,   5u, { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 } },
	{ RX_ADDR_P1,   5u, { 0xC2, 0xC2, 0xC2, 0xC2, 0xC2 } },
	{ RX_ADDR_P2,   1u, { 0xC3 } },
	{ RX_ADDR_P3,   1u, { 0xC4 } },
	{ RX_ADDR_P4,   1u, { 0xC5 } },
	{ RX_ADDR_P5,   1u, { 0xC6 } },
	{ TX_ADDR,      5u, { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 } },
	{ RX_PW_P0,     1u, { 0x00 } },
	{ RX_PW_P1,     1u, { 0x00 } },
	{ RX_PW_P2,     1u, { 0x00 } },
	{ RX_PW_P3,     1u, { 0x00 } },
	{ RX_PW_P4,     1u, { 0x00 } },
	{ RX_PW_P5,     1u, { 0x00 } },
	{ DYNPD,        1u, { 0x01 } },
	{ FEATURE,      1u, { 0x06 } },
};

const NRF24_config_table_st NRF24_config_table_c[] = 
{
    { (NRF24_reg_config_st*)NRF24_default_tx_config_c, 22u },
    { (NRF24_reg_config_st*)NRF24_HUB_config_c,        22u },
};

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/* None */

/****************************** END OF FILE *******************************************************/
