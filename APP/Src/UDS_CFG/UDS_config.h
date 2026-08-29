/*! \file
*               Author: mstewart
*   \brief      APP's UDS service table - STM32F103 build
*
*   Just SecurityAccess (0x27) for now - the only thing APP needs it for is gating entry into
*   PROGRAMMING session (see uds_handle_session_control() in xCOMMON_MODULES/Src/UDS/UDS.c, which
*   refuses 0x10 0x02 with NRC 0x22 unless security is already unlocked). 0x10/0x11/0x3E are
*   handled internally by UDS.c and need no entry here.
*/
#ifndef UDS_CONFIG_H
#define UDS_CONFIG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "UDS.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Standard ISO 14229-1 SecurityAccess sub-function convention: odd = requestSeed, even = sendKey,
   for security level N the pair is (2N-1, 2N) - matches FBL/Src/UDS_CFG/UDS_config.h's constants.
   All four pairs are accepted because testers differ on which level guards programming access -
   and a sub-function the table does not list is rejected with NRC 0x31 before the session is even
   considered, which reads like a session fault rather than the level mismatch it actually is. All
   four grant the same single level (see UDS_config.c): there is one stub algorithm behind them, so
   this is four doors to one room, not graded access. Drop whichever pairs your tester does not use
   once that is settled. */
#define APP_SECURITY_LEVEL_1_SEED  ( 0x01u )  /* Request seed for security level 1 */
#define APP_SECURITY_LEVEL_1_KEY   ( 0x02u )  /* Send key for security level 1 */
#define APP_SECURITY_LEVEL_2_SEED  ( 0x03u )  /* Request seed for security level 2 */
#define APP_SECURITY_LEVEL_2_KEY   ( 0x04u )  /* Send key for security level 2 */
#define APP_SECURITY_LEVEL_3_SEED  ( 0x05u )  /* Request seed for security level 3 */
#define APP_SECURITY_LEVEL_3_KEY   ( 0x06u )  /* Send key for security level 3 */
#define APP_SECURITY_LEVEL_4_SEED  ( 0x07u )  /* Request seed for security level 4 */
#define APP_SECURITY_LEVEL_4_KEY   ( 0x08u )  /* Send key for security level 4 */

/* Security level granted by a successful SecurityAccess, and required by the session table below
   to enter PROGRAMMING. One level is all APP needs - FBL is where graded access would matter. */
#define APP_SECURITY_LEVEL_1       ( 0x01u )

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
const UDS_service_table_st*      UDS_get_service_table( void );
u8_t                             UDS_get_service_table_size( void );
const UDS_session_transition_st* UDS_get_session_table( void );
u8_t                             UDS_get_session_table_size( void );

#endif /* UDS_CONFIG_H */

/****************************** END OF FILE *******************************************************/
