/*! \file
*               Author: mstewart
*   \brief      BOOT_HANDOFF - what BM, FBL and APP leave each other in SHARED_RAM across a soft reset
*
*   The HUB's struct for the generic SHARED_RAM region (xCOMMON_MODULES/Src/SHARED_RAM): an FBL entry
*   request and the boot failure bookkeeping BM uses to decide when to give up on APP. All three images
*   build this same file, so they agree on the layout.
*
*   To add a field: append it to boot_handoff_st (fixed-width types only), add its default to
*   boot_handoff_defaults_c in the .c and write its accessors. No layout bump is needed, and BM, FBL and
*   APP need not be reflashed together - see SHARED_RAM.h. Reordering, resizing or removing a field
*   needs BOOT_HANDOFF_LAYOUT bumped instead.
*/
#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "SHARED_RAM.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
/* Bump only for a change that is not an append - see the header comment */
#define BOOT_HANDOFF_LAYOUT ( 1u )

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/

/*!
*   \brief The handoff data. Append new fields at the end.
*/
typedef struct
{
    u8_t  fbl_request;            /*!< Non-zero = BM/APP asks for FBL entry on next boot           */
    u8_t  boot_failure_count;     /*!< APP launches not yet confirmed good - incremented by
                                       BM before each jump to APP, cleared by APP once it has
                                       run stably and by FBL when a new download starts. See
                                       BM_run()'s boot-failure step (BM.c)                         */
    u8_t  last_failure_reason;    /*!< Last bm_failure_reason_et logged, see BM.h                  */
    u8_t  dataset_valid_mask;     /*!< Bit n set = dataset n passed BM's presence/CRC/
                                       signature checks this boot (informational only - see
                                       BM_run()'s dataset step). Datasets 0-7 only                 */
    u16_t fbl_timeout;            /*!< Reserved - FBL entry timeout in ms, not yet used            */
} boot_handoff_st;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* Set up the region for boot_handoff_st, keeping the previous boot's fbl_request/failure state on a
   soft reset rather than wiping it. Matches the function pointer BM takes. */
void          BOOT_HANDOFF_init( void );

void          BOOT_HANDOFF_set_fbl_request( false_true_et request );
false_true_et BOOT_HANDOFF_get_fbl_request( void );

/* Reserved for future use - stored but not read by anything, see .c */
void          BOOT_HANDOFF_set_fbl_timeout( u32_t timeout_ms );
u32_t         BOOT_HANDOFF_get_fbl_timeout( void );

void          BOOT_HANDOFF_set_last_failure_reason( u8_t reason );
u8_t          BOOT_HANDOFF_get_last_failure_reason( void );

void          BOOT_HANDOFF_set_boot_failure_count( u8_t count );
u8_t          BOOT_HANDOFF_get_boot_failure_count( void );

void          BOOT_HANDOFF_set_dataset_valid_mask( u8_t mask );
u8_t          BOOT_HANDOFF_get_dataset_valid_mask( void );

#endif /* BOOT_HANDOFF_H */

/****************************** END OF FILE *******************************************************/
