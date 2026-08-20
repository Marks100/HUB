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
*   just a size cut), 0x2E WriteDID/fingerprint (NVM persistence cut for size), 0x28/0x85 stubs,
*   0x23/0x3D RMBA/WMBA.
*
*   Kept: 0x27 SecurityAccess, 0x31 RoutineControl (EraseMemory + CheckMemory), 0x34/0x36/0x37
*   the actual download sequence.
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
#define ROUTINE_ID_ERASE_MEMORY  ( 0xFF00u )
#define ROUTINE_ID_CHECK_MEMORY  ( 0xFF02u )

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
