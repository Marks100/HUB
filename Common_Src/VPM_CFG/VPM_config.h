/*! \file
*               Author: mstewart
*   \brief      This project's variant parameter manager (VPM) configuration: where the variant parameters are, the STM32F1 flash they
*               are in and the NVM_GEN2 blocks the patches are kept in
*
*   \details    In Common_Src because APP (the VPM client, which keeps the patches), the FBL (the VPM server, which applies them) and
*               the software tests (the host library) all use it. APP and the FBL both hand vpm_cfg_s to their half; the server
*               takes vpm_server_cfg_s as well, for the flash.
*/
#ifndef VPM_CONFIG_H
#define VPM_CONFIG_H

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "NVM_GEN2.h"
#include "VPM_COMMON.h"
#include "VPM_SERVER.h"

/***************************************************************************************************
**                                       Definitions                                              **
***************************************************************************************************/
/* The variant parameter page, both ends included: after APP and before DATASET0, as the three linker scripts and APP/Makefile
   (VPM_FLASH_START/SIZE) lay it out. Not DATASET0, which a download makes. */
#define VPM_REGION_START    ( 0x0801E400ul )
#define VPM_REGION_END      ( 0x0801E7FFul )

/* APP's NVM_GEN2 block of patches: the first free ID of APP's range, which APP_NVM_BLOCKS.h's enum takes from here so the two cannot
   differ. The FBL reads it by this ID; the FBL's own block, of the batch it applied, is FBL_VPM_APPLIED_BLOCK_ID (FBL_NVM_BLOCKS.h). */
#define VPM_BLOCK_ID        ( NVM_GEN2_BLOCK_ID_OWNER_B_FIRST + 4u )

/***************************************************************************************************
**                                    Exported Globals                                            **
***************************************************************************************************/
extern const VPM_COMMON_cfg_st vpm_cfg_s;           /* for VPM_CLIENT_init() in APP, and what vpm_server_cfg_s is built on */
extern const VPM_SERVER_cfg_st vpm_server_cfg_s;    /* for VPM_SERVER_init() in the FBL */

/***************************************************************************************************
**                                    Function Prototypes                                        **
***************************************************************************************************/
void vpm_config_note_applied( u16_t sequence );     /* the FBL's note of the batch it applied: in vpm_server_cfg_s, and for the tests' bootloader */

#endif /* VPM_CONFIG_H */

/****************************** END OF FILE *******************************************************/