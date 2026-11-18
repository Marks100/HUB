/*! \file
*               Author: mstewart
*   \brief      TMD_FBL - the HUB's own bootloader (FBL), as it starts when the application hands over to it
*
*   \details    The host library runs the application only, so this model is the FBL's start: the request the application leaves in the
*               handoff RAM (BOOT_HANDOFF), the values it hands over with it, the NVM the FBL starts as its own start does, and the write it
*               makes if the ECU kept one (TMD_MBA is the server that makes it). It does not run the FBL's comms, so what the FBL sends at
*               the baud rate a LinkControl (0x87) hand-over asks for is not here to see, only what it found in the handoff RAM.
*
*               It is the HUB's, not a generic model: it uses the handoff RAM and the NVM block of the firmware the HUB's FBL shares with
*               its application. The ECU start of the software tests (SW_TEST_STUBS.c) runs it between BM and the application.
*/
#ifndef TMD_FBL_H
#define TMD_FBL_H

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"

/***************************************************************************************************
**                                      Function Prototypes                                       **
***************************************************************************************************/
void  TMD_FBL_init( void );                          /* the FBL has not run: nothing found */
void  TMD_FBL_start( void );                         /* BM has started: the FBL runs if the application asked for it, and the ECU is left as the reset to the application leaves it */
u8_t  TMD_FBL_get_baud_param( void );                /* what the FBL found at its last start: the baud rate a LinkControl hand-over asked for. 0 for a start the application did not hand over at */
u8_t  TMD_FBL_get_response_owed( void );             /* and the response the application owed (BOOT_HANDOFF_RESPONSE_*), which the FBL sends once. BOOT_HANDOFF_RESPONSE_NONE if none */

#endif /* TMD_FBL_H */

/****************************** END OF FILE *******************************************************/