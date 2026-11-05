/*! \file
*               Author: mstewart
*   \brief      NODE_INFO - what the HUB has read from the selected node over UDS
*
*   \details    UDS_CLIENT only sends what it is told to. This is the layer above it that decides what to
*               read and when: the node's part number and software version, each asked for from the selected
*               node once a second (one DID per attempt, taking them in turn so one that keeps failing can't
*               starve the other) until it answers, then kept. Selecting another node through
*               NODE_INFO_select_node() clears them and starts again, since the values belong to the node
*               they were read from.
*
*               Every request asks DIAG_GW for the node first (UDS_CLIENT's reserve_func_p), so it never
*               interrupts a tester.
*/
#ifndef NODE_INFO_H
#define NODE_INFO_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "UDS_CLIENT.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define NODE_INFO_PART_NUMBER_DID       ( 0xF187u )   /* vehicleManufacturerSparePartNumber - confirm for the node */
#define NODE_INFO_VERSION_DID           ( 0xF189u )   /* vehicleManufacturerECUSoftwareVersionNumber - confirm for the node */
#define NODE_INFO_VALUE_MAX_LEN         ( 32u )       /* a longer value is truncated */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef enum
{
    NODE_INFO_PART_NUMBER = 0u,
    NODE_INFO_VERSION,
    NODE_INFO_ITEM_COUNT
} NODE_INFO_item_et;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
void         NODE_INFO_init( void );

/* Call periodically (SCHED_PERIOD_100MS) - the retry period is timed on TIME_MGR, so the call period is its resolution */
void         NODE_INFO_tick( void );

/* UDS_CLIENT_set_node(), and on a change of node the kept values are cleared and the reads start again */
pass_fail_et NODE_INFO_select_node( UDS_CLIENT_node_t node );

/* An item as the node sent it, or NULL_P until it has answered. *len_p (may be NULL_P) receives its length. */
const u8_t*  NODE_INFO_get( NODE_INFO_item_et item, u8_t* len_p );

/* The response function of NODE_INFO's own reads (UDS_CLIENT_response_func_t) - keeps a positive answer */
void         NODE_INFO_response( UDS_CLIENT_result_et result, const u8_t* request_p, u16_t request_len,
                                 const u8_t* response_p, u16_t response_len );

#endif /* NODE_INFO_H */

/****************************** END OF FILE *******************************************************/
