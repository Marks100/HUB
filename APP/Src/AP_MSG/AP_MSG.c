/*! \file
*               Author: mstewart
*   \brief      AP_MSG - text sent to the HUB over its soft AP, kept for the display
*
*   \details    A client that joins HUB_AP and opens a TCP connection to the HUB's address
*               (192.168.4.1) on AP_MSG_SERVER_PORT can send text. WIFI hands every packet that
*               arrives on such a connection to AP_MSG_on_rx(), and this module keeps the last
*               AP_MSG_LINES lines of it for a screen to show.
*
*               Any ASCII text works: a terminal or TCP tool that sends a line at a time, or a
*               program that writes a few bytes with no newline. A line ends at a CR or LF, at the
*               end of the packet it was in, or when it has filled a display row, in which case it
*               carries on in the next one. Bytes that are not printable ASCII are dropped, and a
*               tab becomes a space. Empty lines are ignored.
*
*               WIFI calls AP_MSG_on_rx() from the ESP01 tick, and the screen reads from the HMI
*               tick. Both run from the same scheduler tick, one after the other, so the lines are
*               never read half written and nothing needs locking.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "AP_MSG.h"

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void ap_msg_add_char( char ch );
STATIC void ap_msg_commit_line( void );

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC char  ap_msg_lines_s[AP_MSG_LINES][AP_MSG_LINE_CHARS + 1u];   /* Finished lines, a ring */
STATIC u8_t  ap_msg_newest_s;                                         /* Index of the newest finished line */
STATIC u8_t  ap_msg_count_s;                                          /* Finished lines held, up to AP_MSG_LINES */
STATIC char  ap_msg_current_s[AP_MSG_LINE_CHARS + 1u];                /* The line being built */
STATIC u8_t  ap_msg_current_len_s;
STATIC u32_t ap_msg_rx_count_s;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Module (re-)initialisation function
*
*   \author        MS
*
*   \return        none
*
*   \note          Forgets every line and the receive count.
*
***************************************************************************************************/
void AP_MSG_init( void )
{
    STDC_memset( ap_msg_lines_s, 0x00, sizeof( ap_msg_lines_s ) );
    STDC_memset( ap_msg_current_s, 0x00, sizeof( ap_msg_current_s ) );

    ap_msg_newest_s      = 0u;
    ap_msg_count_s       = 0u;
    ap_msg_current_len_s = 0u;
    ap_msg_rx_count_s    = 0u;
}

/*!
****************************************************************************************************
*
*   \brief         Data from a client of the HUB's TCP server
*
*   \author        MS
*
*   \param[in]     link_id - Connection the data came in on, not used, every client is treated alike
*   \param[in]     data_p  - The bytes received
*   \param[in]     len     - How many
*
*   \return        none
*
*   \note          Register it as server_rx_callback_p in the WIFI configuration. A line still being
*                  built when the packet ends is finished there, so text sent without a newline shows
*                  up straight away.
*
***************************************************************************************************/
void AP_MSG_on_rx( u8_t link_id, u8_t* data_p, u16_t len )
{
    u16_t i;

    (void)link_id;

    if( data_p != NULL_P )
    {
        for( i = 0u; i < len; i++ )
        {
            ap_msg_add_char( (char)data_p[i] );
        }

        ap_msg_commit_line();
        ap_msg_rx_count_s++;
    }
}

/*!
****************************************************************************************************
*
*   \brief         Number of lines held
*
*   \author        MS
*
*   \return        0 to AP_MSG_LINES
*
***************************************************************************************************/
u8_t AP_MSG_get_line_count( void )
{
    return( ap_msg_count_s );
}

/*!
****************************************************************************************************
*
*   \brief         One of the held lines
*
*   \author        MS
*
*   \param[in]     age - 0 is the newest line, 1 the one before it, and so on
*
*   \return        The line as a null-terminated string, or an empty string if there is no such line.
*
***************************************************************************************************/
const char* AP_MSG_get_line( u8_t age )
{
    const char* line_p = "";

    if( age < ap_msg_count_s )
    {
        line_p = ap_msg_lines_s[( ap_msg_newest_s + AP_MSG_LINES - age ) % AP_MSG_LINES];
    }

    return( line_p );
}

/*!
****************************************************************************************************
*
*   \brief         Packets received since AP_MSG_init()
*
*   \author        MS
*
*   \return        Count of packets, not lines
*
***************************************************************************************************/
u32_t AP_MSG_get_rx_count( void )
{
    return( ap_msg_rx_count_s );
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Add one received byte to the line being built
*
*   \author        MS
*
*   \param[in]     ch - The byte
*
*   \return        none
*
*   \note          CR and LF finish the line. A tab is kept as a space, other non-printable bytes are
*                  dropped. A line that has filled a display row is finished and the text carries on
*                  in the next one.
*
***************************************************************************************************/
STATIC void ap_msg_add_char( char ch )
{
    if( ( ch == '\r' ) || ( ch == '\n' ) )
    {
        ap_msg_commit_line();
    }
    else
    {
        if( ch == '\t' )
        {
            ch = ' ';
        }

        if( ( ch >= ' ' ) && ( ch <= '~' ) )
        {
            ap_msg_current_s[ap_msg_current_len_s] = ch;
            ap_msg_current_len_s++;

            if( ap_msg_current_len_s >= AP_MSG_LINE_CHARS )
            {
                ap_msg_commit_line();
            }
        }
    }
}

/*!
****************************************************************************************************
*
*   \brief         Finish the line being built and keep it, if it is not empty
*
*   \author        MS
*
*   \return        none
*
*   \note          The newest line replaces the oldest once AP_MSG_LINES are held.
*
***************************************************************************************************/
STATIC void ap_msg_commit_line( void )
{
    if( ap_msg_current_len_s > 0u )
    {
        ap_msg_newest_s = (u8_t)( ( ap_msg_newest_s + 1u ) % AP_MSG_LINES );

        ap_msg_current_s[ap_msg_current_len_s] = '\0';
        STDC_memcpy( ap_msg_lines_s[ap_msg_newest_s], ap_msg_current_s, (u16_t)( ap_msg_current_len_s + 1u ) );

        if( ap_msg_count_s < AP_MSG_LINES )
        {
            ap_msg_count_s++;
        }

        ap_msg_current_len_s = 0u;
    }
}

/****************************** END OF FILE *******************************************************/
