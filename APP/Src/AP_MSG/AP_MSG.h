#ifndef AP_MSG_H
#define AP_MSG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define AP_MSG_SERVER_PORT      ( 5000u )   /* TCP port a client connects to on the HUB */
#define AP_MSG_AP_IP            "192.168.77.1"  /* The HUB's address on its soft AP. Not in 192.168.4.x, the module's own default, because a home
                                                   network very often is, and the AP and the station cannot share a subnet. */
#define AP_MSG_LINES            ( 5u )      /* Lines kept, the newest replaces the oldest         */
#define AP_MSG_LINE_CHARS       ( 21u )     /* Characters in a line, the width of the display     */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void        AP_MSG_init( void );
void        AP_MSG_on_rx( u8_t link_id, u8_t* data_p, u16_t len );
u8_t        AP_MSG_get_line_count( void );
const char* AP_MSG_get_line( u8_t age );
u32_t       AP_MSG_get_rx_count( void );

#endif /* AP_MSG_H multiple inclusion guard */

/****************************** END OF FILE *******************************************************/
