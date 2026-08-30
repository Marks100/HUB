/*! \file
*               Author: mstewart
*   \brief      APP's UDS service table - STM32F103 build
*
*   SecurityAccess (0x27) gates entry into PROGRAMMING session (see uds_handle_session_control()
*   in xCOMMON_MODULES/Src/UDS/UDS.c, which refuses 0x10 0x02 with NRC 0x22 unless security is
*   already unlocked). RoutineControl (0x31) carries CheckProgrammingPreconditions only - SLP3
*   §2.2.7.2's Pre-Programming Step routine, callable from EXTENDED before any SecurityAccess since
*   it is checking physical ECU state, not diagnostic access. CommunicationControl (0x28) and
*   ControlDTCSetting (0x85) are the other two Pre/Post-Programming Step services (§2.2.5/§2.2.12) -
*   both EXTENDED-only, both real (not stub) implementations: 0x28 gates MSG_SCHED's non-diagnostic
*   CAN TX (sensor telemetry + heartbeat), 0x85 gates FAULT_MGR's DTC storage. 0x10/0x11/0x3E are
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
/* Security level granted by a successful SecurityAccess, and required by the session table below
   to enter PROGRAMMING. One level is all APP needs - FBL is where graded access would matter. */
#define APP_SECURITY_LEVEL_1       ( 0x01u )

/* 0x31 RoutineControl routine identifiers - matches Vector SLP3 spec's ID exactly */
#define ROUTINE_ID_CHECK_PROGRAMMING_PRECONDITIONS ( 0x0203u )

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
const UDS_service_table_st*      UDS_get_service_table( void );
u8_t                             UDS_get_service_table_size( void );
const UDS_session_transition_st* UDS_get_session_table( void );
u8_t                             UDS_get_session_table_size( void );

#endif /* UDS_CONFIG_H */

/****************************** END OF FILE *******************************************************/
