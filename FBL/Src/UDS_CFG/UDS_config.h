/*! \file
*               Author: mstewart
*   \brief      FBL's UDS service table - STM32F103 build
*
*   Trimmed to only the services needed to reflash APP, ported from AUTOCFG_HUB/FBL's UDS_config.c
*   against this project's UDS.h API (service-table-of-subfunction-tables, not a flat SID list -
*   see xCOMMON_MODULES/Src/UDS/UDS.c). 0x10/0x11/0x3E are handled internally by UDS.c itself and
*   need no entry here.
*
*   Dropped vs. the source project: 0x22 ReadDID (its handlers depend on a UDS_DID_common/UID
*   DID-reader module that doesn't exist anywhere in this project - a missing-dependency cut, not
*   just a size cut), 0x28/0x85 stubs, 0x23/0x3D RMBA/WMBA.
*
*   Kept: 0x27 SecurityAccess, 0x31 RoutineControl (EraseMemory + CheckMemory + StayInBoot +
*   CheckProgrammingDependencies + CheckProgrammingPreconditions), 0x34/0x36/0x37 the actual
*   download sequence, 0x2E WriteDataByIdentifier (fingerprint only - see
*   DID_APPLICATION_SOFTWARE_FINGERPRINT below). 0x2E was previously cut for NVM persistence size,
*   until FBL gained an NVM driver - it now stores the fingerprint as an NVM_GEN2 block in the
*   partitions it shares with APP, see fbl_nvm_gen2_hw_interface_s's comment in
*   FBL/Src/INT_STUBS/INTEGRATION_STUBS.c.
*
*   CheckProgrammingPreconditions ($0203) is duplicated here AND in APP/Src/UDS_CFG/UDS_config.c,
*   both "no real gate yet" - not redundant: CANFLASH's actual sequence (Tool_cfg/CANFLASH/
*   hub_bootloader.json's before_programming=true) sends it AFTER the SessionControl->PROGRAMMING
*   transition, i.e. against FBL, not APP, despite SLP3's own diagram placing it in the
*   Pre-Programming Step against the application. Without this entry the request NRCs harmlessly
*   (CANFLASH's routine_try() is best-effort) but logs a warning on every single flash.
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
/* CAN IDs - physical request/response pair, matches AUTOCFG_HUB's FBL convention */
#define FBL_UDS_REQUEST_ID   ( 0x700u )
#define FBL_UDS_RESPONSE_ID  ( 0x600u )
#define FBL_CAN_RX_ID        ( 0x7E0u )
#define FBL_CAN_TX_ID        ( 0x7E8u )

/* 0x31 RoutineControl routine identifiers */
#define ROUTINE_ID_ERASE_MEMORY               ( 0xFF00u )
#define ROUTINE_ID_CHECK_MEMORY               ( 0xFF02u )
#define ROUTINE_ID_STAY_IN_BOOT               ( 0xF518u ) /* Matches Vector SLP3 spec's "Force Boot Mode" ID exactly */
#define ROUTINE_ID_CHECK_PROGRAMMING_DEPENDENCIES ( 0xFF01u ) /* Matches Vector SLP3 spec's own example ID exactly */
#define ROUTINE_ID_CHECK_PROGRAMMING_PRECONDITIONS ( 0x0203u ) /* Matches Vector SLP3 spec's own example ID exactly */

/* 0x2E WriteDataByIdentifier DIDs - matches xCOMMON_MODULES/Src/UDS/ISO_14229_DID_REFERENCE.md's
   standard applicationSoftwareFingerprint entry (Table C.1) rather than inventing a
   manufacturer-specific one, since this project has no OEM DID range of its own. */
#define DID_APPLICATION_SOFTWARE_FINGERPRINT  ( 0xF184u )

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
/* None */

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
const UDS_service_table_st*      UDS_get_service_table( void );
u8_t                             UDS_get_service_table_size( void );
const UDS_session_transition_st* UDS_get_session_table( void );
u8_t                             UDS_get_session_table_size( void );

#endif /* UDS_CONFIG_H */

/****************************** END OF FILE *******************************************************/
