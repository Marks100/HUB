/*! \file
*               Author: mstewart
*   \brief      NODE_INFO - see NODE_INFO.h
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "NODE_INFO.h"
#include "INTEGRATION_STUBS.h"   /* app_node_is_available() */
#include "TIME_MGR.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define NODE_INFO_READ_PERIOD_MS        ( 1000u )

#define NODE_INFO_READ_DID_HEADER_LEN   ( 3u )    /* 62 DID_hi DID_lo, then the value, in the response */

/***************************************************************************************************
**                              Data Types and Enums                                              **
***************************************************************************************************/
typedef struct
{
    u8_t          value[ NODE_INFO_VALUE_MAX_LEN ];
    u8_t          len;
    false_true_et valid;
} node_info_item_st;

typedef enum
{
    NODE_INFO_STATE_READ = 0u,      /* the current item is due: ask for it as soon as the client is free */
    NODE_INFO_STATE_AWAIT_RESPONSE, /* asked, waiting for NODE_INFO_response() - which moves on, whatever the outcome */
    NODE_INFO_STATE_PAUSE,          /* waiting out NODE_INFO_READ_PERIOD_MS before the current item's turn */
    NODE_INFO_STATE_DONE            /* every item answered */
} node_info_state_et;

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
/* The DID of each NODE_INFO_item_et, in its order */
STATIC const u16_t node_info_dids_c[ NODE_INFO_ITEM_COUNT ] = { NODE_INFO_PART_NUMBER_DID, NODE_INFO_VERSION_DID };

STATIC node_info_item_st  node_info_items_s[ NODE_INFO_ITEM_COUNT ];
STATIC node_info_state_et node_info_state_s;
STATIC u8_t               node_info_item_s;            /* the item being read, or next to be */
STATIC u32_t              node_info_pause_start_ms_s;  /* TIME_MGR time the pause began */

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void node_info_forget( void );
STATIC void node_info_store( u8_t item, const u8_t* response_p, u16_t response_len );
STATIC u8_t node_info_find_unanswered( u8_t from_item );
STATIC void node_info_advance( void );

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Forget everything read; the first read goes out on the first tick
*
*   \author        mstewart
*
*   \return        none
*
***************************************************************************************************/
void NODE_INFO_init( void )
{
    node_info_forget();
}

/*!
****************************************************************************************************
*
*   \brief         Run the reads: one item at a time, a period apart, until every item has been answered
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Nothing is asked of a node that is not available (not configured, its port off or its
*                  heartbeat missing, see app_node_is_available()): the machine waits in READ and asks the moment
*                  it is. A refused reservation, a negative response or a timeout moves on to the next item's
*                  turn a period later. A client that is busy with someone else's request is tried again on
*                  the next tick.
*
***************************************************************************************************/
void NODE_INFO_tick( void )
{
    switch( node_info_state_s )
    {
        case NODE_INFO_STATE_READ:
        {
            if( ( UDS_CLIENT_is_busy() == FALSE ) && ( app_node_is_available( UDS_CLIENT_get_node() ) == TRUE ) )
            {
                if( UDS_CLIENT_read_did( node_info_dids_c[node_info_item_s], NODE_INFO_response ) == PASS )
                {
                    node_info_state_s = NODE_INFO_STATE_AWAIT_RESPONSE;
                }
                else
                {
                    node_info_advance();
                }
            }
        }
        break;

        case NODE_INFO_STATE_PAUSE:
        {
            if( TIME_has_time_elapsed_ms( node_info_pause_start_ms_s, NODE_INFO_READ_PERIOD_MS ) == TRUE )
            {
                node_info_state_s = NODE_INFO_STATE_READ;
            }
        }
        break;

        case NODE_INFO_STATE_AWAIT_RESPONSE:
        case NODE_INFO_STATE_DONE:
        default:
        {
            /* Nothing to do: the response moves on, or everything has been read */
        }
        break;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Select the node later requests go to
*
*   \author        mstewart
*
*   \param[in]     node - Index of the node, see UDS_CLIENT_set_node()
*
*   \return        PASS if selected (or already selected), FAIL if refused by UDS_CLIENT
*
*   \note          A different node clears what was read from the previous one and starts the reads again
*
***************************************************************************************************/
pass_fail_et NODE_INFO_select_node( UDS_CLIENT_node_t node )
{
    pass_fail_et result = FAIL;

    if( node == UDS_CLIENT_get_node() )
    {
        result = UDS_CLIENT_set_node( node );
    }
    else if( UDS_CLIENT_set_node( node ) == PASS )
    {
        node_info_forget();
        result = PASS;
    }
    else
    {
        /* Refused: a request is in flight or there is no such node - nothing changes */
    }

    return( result );
}

/*!
****************************************************************************************************
*
*   \brief         An item read from the selected node
*
*   \author        mstewart
*
*   \param[in]     item  - Which one
*   \param[out]    len_p - Receives the length (0 until read). May be NULL_P.
*
*   \return        The bytes as the node sent them, or NULL_P until it has answered or for an unknown
*                  item. Replaced by any later positive answer, so copy it if it must not change.
*
***************************************************************************************************/
const u8_t* NODE_INFO_get( NODE_INFO_item_et item, u8_t* len_p )
{
    const u8_t* value_p = NULL_P;
    u8_t        len     = 0u;

    if( ( item < NODE_INFO_ITEM_COUNT ) && ( node_info_items_s[item].valid == TRUE ) )
    {
        value_p = node_info_items_s[item].value;
        len     = node_info_items_s[item].len;
    }

    if( len_p != NULL_P )
    {
        *len_p = len;
    }

    return( value_p );
}

/*!
****************************************************************************************************
*
*   \brief         Keep the answer to NODE_INFO's read of the current item, and move on to the next
*
*   \author        mstewart
*
*   \param[in]     result       - Outcome of the request
*   \param[in]     request_p    - The request it was for (not used: the state machine knows which item it asked for)
*   \param[in]     request_len  - Request length (not used)
*   \param[in]     response_p   - The positive response, 62 DID_hi DID_lo then the value
*   \param[in]     response_len - Response length
*
*   \return        none
*
*   \note          Anything but a positive answer keeps nothing - the item is tried again on its next turn.
*                  Only called for NODE_INFO's own reads, and only acted on while one is awaited.
*
***************************************************************************************************/
void NODE_INFO_response( UDS_CLIENT_result_et result, const u8_t* request_p, u16_t request_len,
                         const u8_t* response_p, u16_t response_len )
{
    ( void )request_p;
    ( void )request_len;

    if( node_info_state_s == NODE_INFO_STATE_AWAIT_RESPONSE )
    {
        if( result == UDS_CLIENT_RESULT_POSITIVE )
        {
            node_info_store( node_info_item_s, response_p, response_len );
        }

        node_info_advance();
    }
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Drop every value read, and start again at the first item with no pause
*
*   \author        mstewart
*
*   \return        none
*
***************************************************************************************************/
STATIC void node_info_forget( void )
{
    STDC_memset( node_info_items_s, 0x00u, (u16_t)sizeof( node_info_items_s ) );

    node_info_item_s  = 0u;
    node_info_state_s = NODE_INFO_STATE_READ;
}

/*!
****************************************************************************************************
*
*   \brief         Keep the value in a positive response to a read of an item
*
*   \author        mstewart
*
*   \param[in]     item         - The item that was read
*   \param[in]     response_p   - The response, 62 DID_hi DID_lo then the value
*   \param[in]     response_len - Response length
*
*   \return        none
*
*   \note          A value longer than NODE_INFO_VALUE_MAX_LEN is truncated. A response with no room for
*                  the DID header keeps nothing.
*
***************************************************************************************************/
STATIC void node_info_store( u8_t item, const u8_t* response_p, u16_t response_len )
{
    u16_t value_len;

    if( ( response_p != NULL_P ) && ( response_len >= NODE_INFO_READ_DID_HEADER_LEN ) )
    {
        value_len = response_len - NODE_INFO_READ_DID_HEADER_LEN;
        value_len = ( value_len > NODE_INFO_VALUE_MAX_LEN ) ? NODE_INFO_VALUE_MAX_LEN : value_len;

        STDC_memcpy( node_info_items_s[item].value, &response_p[NODE_INFO_READ_DID_HEADER_LEN], value_len );
        node_info_items_s[item].len   = (u8_t)value_len;
        node_info_items_s[item].valid = TRUE;
    }
}

/*!
****************************************************************************************************
*
*   \brief         The first item that has not been answered, looking from one item round to the item before it
*
*   \author        mstewart
*
*   \param[in]     from_item - Where to start looking, wrapping round
*
*   \return        The item, NODE_INFO_ITEM_COUNT if every item has been answered
*
***************************************************************************************************/
STATIC u8_t node_info_find_unanswered( u8_t from_item )
{
    u8_t found = NODE_INFO_ITEM_COUNT;
    u8_t item;
    u8_t i;

    for( i = 0u; ( i < NODE_INFO_ITEM_COUNT ) && ( found == NODE_INFO_ITEM_COUNT ); i++ )
    {
        item = (u8_t)( ( from_item + i ) % NODE_INFO_ITEM_COUNT );

        if( node_info_items_s[item].valid == FALSE )
        {
            found = item;
        }
    }

    return( found );
}

/*!
****************************************************************************************************
*
*   \brief         Finish with the current item and wait out a period before the next unanswered one
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Taking the items in turn, starting after the current one, means one that keeps failing
*                  cannot starve the others. With every item answered the machine is done.
*
***************************************************************************************************/
STATIC void node_info_advance( void )
{
    u8_t item = node_info_find_unanswered( (u8_t)( ( node_info_item_s + 1u ) % NODE_INFO_ITEM_COUNT ) );

    if( item < NODE_INFO_ITEM_COUNT )
    {
        node_info_item_s           = item;
        node_info_pause_start_ms_s = TIME_get_cumulative_run_time_ms();
        node_info_state_s          = NODE_INFO_STATE_PAUSE;
    }
    else
    {
        node_info_state_s = NODE_INFO_STATE_DONE;
    }
}

/****************************** END OF FILE *******************************************************/
