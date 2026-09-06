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
*   CAN TX (sensor telemetry + heartbeat), 0x85 gates FAULT_MGR's DTC storage. 0x10, 0x11, 0x27 and
*   0x3E are all rows in uds_service_table_s (UDS_config.c) like every other service, but UDS.c
*   still dispatches them specially rather than through the generic subfunction search - see
*   UDS_service_table_st's comment in UDS.h.
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
/* Security levels granted by a successful SecurityAccess. Numbered to match ISO 14229-1's own
   subFunction convention (level N <-> requestSeed/sendKey subFunction 2N-1/2N), each with its own
   SendKey handler so a different key unlocks each one - see uds_handle_security_send_key()'s
   switch and security_access_cfg_s in UDS_config.c. RequestSeed itself is shared across all three
   (still a fixed placeholder seed regardless of level - see
   uds_handle_security_request_seed()'s comment).

   UDS.c's security check is uds_session_s.security_level >= entry_p->required_sec_level (see
   uds_check_subfunc_security()/the PROGRAMMING transition row in uds_session_table_s below), a
   single scalar rather than independent per-level flags. That makes this a GRADED hierarchy, not
   three independent unlocks: level 3 also satisfies anything gated on level 1 or 2, and level 2
   also satisfies level 1. Level 1 gates entry into PROGRAMMING; level 2 gates writing
   app_key_1_blk_g (DID_APP_KEY_1); level 3 gates writing app_key_2_blk_g (DID_APP_KEY_2) - so
   unlocking level 3 can also write key 1 and enter PROGRAMMING, but unlocking level 2 cannot
   write key 2. */
#define APP_SECURITY_LEVEL_1       ( 0x01u )
#define APP_SECURITY_LEVEL_2       ( 0x02u )
#define APP_SECURITY_LEVEL_3       ( 0x03u )

/* 0x31 RoutineControl routine identifiers - matches Vector SLP3 spec's ID exactly */
#define ROUTINE_ID_CHECK_PROGRAMMING_PRECONDITIONS ( 0x0203u )

/* 0x2E WriteDataByIdentifier DIDs for app_key_1_blk_g/app_key_2_blk_g (APP_NVM_BLOCKS.h). No
   standard ISO 14229-1 DID fits opaque key material, so these sit in the vehicle-manufacturer-
   specific range (0xF900-0xF9FF) per ISO_14229_DID_REFERENCE.md's own guidance, same "no OEM
   range of our own, use what the spec sets aside for exactly this" reasoning as
   DID_APPLICATION_SOFTWARE_FINGERPRINT in FBL/Src/UDS_CFG/UDS_config.h. */
#define DID_APP_KEY_1  ( 0xF900u )
#define DID_APP_KEY_2  ( 0xF901u )

/* 0x22 ReadDataByIdentifier DID - matches ISO_14229_DID_REFERENCE.md's standard
   applicationSoftwareIdentification entry (Table C.1), the APP-side sibling of FBL's
   DID_BOOT_SOFTWARE_IDENTIFICATION (0xF180, FBL/Src/UDS_CFG/UDS_config.h). */
#define DID_APP_SOFTWARE_IDENTIFICATION  ( 0xF181u )
#define APP_SW_ID_MAX_LEN                ( 24u )

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
const UDS_service_table_st* UDS_get_service_table( void );
u8_t                        UDS_get_service_table_size( void );

#endif /* UDS_CONFIG_H */

/****************************** END OF FILE *******************************************************/
