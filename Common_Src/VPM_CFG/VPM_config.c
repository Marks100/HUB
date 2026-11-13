/*! \file
*               Author: mstewart
*   \brief      This project's variant parameter manager (VPM) configuration - see VPM_config.h
*/

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "VPM_config.h"
#include "FBL_NVM_BLOCKS.h"
#include "FLS_STM32F1.h"
#include "NVM_GEN2.h"

/***************************************************************************************************
**                                    Exported Globals                                            **
***************************************************************************************************/
const VPM_COMMON_cfg_st vpm_cfg_s =
{
    .region_start      = VPM_REGION_START,
    .region_end        = VPM_REGION_END,
    .patches_block_id  = VPM_BLOCK_ID,
    .applied_block_id  = FBL_VPM_APPLIED_BLOCK_ID,
    .read_block_func_p = NVM_GEN2_read_block    /* straight out of flash, so either image sees what the other last committed */
};

const VPM_SERVER_cfg_st vpm_server_cfg_s =
{
    .common_p           = &vpm_cfg_s,
    .page_size          = (u16_t)FLS_STM32F1_PAGE_SIZE,
    .flash_erase_func_p = FLS_STM32F1_erase_sector,
    .flash_write_func_p = FLS_STM32F1_write_data,
    .note_applied_func_p = vpm_config_note_applied
};

/***************************************************************************************************
**                                  Public Functions                                              **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Keep the number of the batch the FBL has dealt with in the NVM, where APP reads it
*
*   \author        mstewart
*
*   \param[in]     sequence   The sequence number of the batch
*
*   \return        none
*
*   \note          Flushed synchronously: it has to be on flash before the FBL resets, or APP would hand over again.
*
****************************************************************************************************/
void vpm_config_note_applied( u16_t sequence )
{
    fbl_vpm_applied_g.sequence = sequence;
    NVM_GEN2_flush_block( FBL_VPM_APPLIED_BLOCK_ID );
}

/****************************** END OF FILE *******************************************************/