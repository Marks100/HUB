#ifndef NRF24_CONFIG_H
#define NRF24_CONFIG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define NRF24_WRITE_CONFIG_DATA_SIZE     ( 22u )
#define NRF24_READ_CONFIG_DATA_SIZE      ( 27u )

/* When set, nrf24_set_configuration() reads back every register it writes and compares it
 * against the value just sent. A mismatch means the write didn't take - chip unpowered,
 * SPI miswired, or not responding - and STDC_basic_assert() is called immediately instead of
 * silently continuing with a half-configured (or unconfigured) radio. */
#define NRF24_VERIFY_CONFIG_WRITES ( 1 )

/***************************************************************************************************
**                              Constants                                                         **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef enum
{
    NRF24_DEFAULT_CONFIG,
    NRF24_HUB_CONFIG,
    NRF24_CFG_MAX
} NRF24_configuration_et;

typedef struct 
{
    u8_t reg;
    u8_t len;
    u8_t data[5u];
} NRF24_reg_config_st;

typedef struct 
{
    NRF24_reg_config_st* dataset;
    u8_t                 config_len;
} NRF24_config_table_st;

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern const NRF24_config_table_st NRF24_config_table_c[];

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/

#endif /* NRF24_CONFIG_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
