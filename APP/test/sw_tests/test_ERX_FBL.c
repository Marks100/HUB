/*! \file
*               Author: mstewart
*   \brief      Flashing an ERX - software test
*
*   \details    A tester (TMD_TESTER) downloads an application to an ERX (TMD_ERX_FBL, the model of its real bootloader) that is running its
*               application and resets into its bootloader when the tester asks for the programming session. The ERX asks a sender for no
*               separation time and no block size, takes a message only while its frames arrive in sequence, says response pending for an erase,
*               and checks the CRC-32 the image carries. The same download is done two ways, wired differently:
*
*                 - through the HUB: the tester is on CAN1 (500 kbit/s) and the ERX on CAN2 (125 kbit/s), and the HUB's gateway (CANIF, PDUR,
*                   DIAG_GW, the MCP2515 driver) carries every frame between them. The tester sends a 253 byte block as a first frame and 36
*                   consecutive frames at the rate the flow control it received allows, so the gateway has to bring that down to what CAN2 and the
*                   MCP2515 can take, by the flow control it passes on to the tester.
*                 - straight to the ERX: the tester and the ERX are on one 125 kbit/s bus, and the HUB is not between them.
*
*               What the tester needs to know of the ERX (its IDs, its key, its erase range), the dummy application it downloads, the ERX itself
*               and the straight wiring are the rig's, in SW_TEST_STUBS.c; the test chooses the wiring and looks at what the ERX ends up with (the
*               flash it holds, the frames it lost, the CRC it accepted or refused) and at the flow control the tester is given. Every test case
*               starts as a virgin ECU (setUp, SW_TEST_STUBS.c).
*/
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "ERX_MGR.h"

#include "TMD_CAN.h"
#include "TMD_CANTP.h"
#include "TMD_DEBUG.h"
#include "TMD_ERX.h"
#include "TMD_ERX_UDS.h"
#include "TMD_ERX_FBL.h"
#include "TMD_INIT.h"
#include "TMD_TESTER.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_TESTER_IDLE_MS         ( 5600u )     /* a little over the gateway's tester_idle_ms (5500): the tester has been quiet for good */
#define TEST_ERX_SETTLE_MS          ( 2500u )     /* the ERX is up, and the HUB's own client has read what it wants from it: the gateway is free for the tester */
#define TEST_BUS_ERX_DIRECT         ( TMD_CAN_INSTANCE_2 )    /* a bus with no ECU on it, that a tester wired straight to an ERX shares with it: the ERX's speed, SW_TEST_NODES_BUS_BIT_RATE */
#define TEST_ERX_ISO_POSITION       ( APP_ISO_AXLE_1_CENTRE )    /* the first axle's centre line: IDs 0x718 and 0x618 */
#define TEST_OTHER_ERX_ISO_POSITION ( APP_ISO_AXLE_1_OUTER_LEFT )    /* another ERX the HUB is set up for, on the same port and the same bus as the first, at its own position: IDs 0x716 and 0x616 */
#define TEST_IMAGE_MAX_MS           ( 101000u )   /* through the HUB: the 1204 blocks of 253 bytes took 96.3 s, 80 ms a block, of which CAN2 is 35 ms, the ERX's flow control 7 ms and its write 28 ms. A 5 ms gateway tick and 2 ms separation took ~40% longer */
#define TEST_DIRECT_IMAGE_MAX_MS    ( 88000u )    /* straight to the ERX: the same 1204 blocks took 84.2 s (70 ms a block), the wire and the ERX with no HUB, which is 12 s less. A real ERX flashed with CANFLASH took 73 ms a block (88.3 s for the four segments); the 3 ms a block between them is the real wire's bit stuffing, which the model does not have */
#define TEST_FC_STMIN_MIN_MS        ( 1u )        /* the least separation CAN2 and the MCP2515 can keep up with (a frame is about 1 ms on CAN2), which the tester has to be told */
#define TEST_FC_STMIN_MAX_MS        ( 0x7Fu )     /* above this the field is not milliseconds */
#define TEST_MESSAGE_LEN            ( 120u )

static u8_t test_read_back_s[TMD_ERX_FBL_APP_WINDOW];

/*!
****************************************************************************************************
*
*   \brief         The record of an ERX in the heartbeat info of the diagnostic service (0xF910)
*
*   \author        mstewart
*
*   \param[out]    data_p       - Buffer for ERX_MGR_HEARTBEAT_INFO_SIZE bytes, the info
*   \param[in]     iso_position - The ERX's ISO position
*
*   \return        Its record in data_p, NULL_P if the HUB has none for that position
*
*****************************************************************************************************/
static const u8_t* test_find_record( u8_t* data_p, u8_t iso_position )
{
    const u8_t* record_p = NULL_P;
    u16_t       len      = 0u;
    u16_t       i;

    ERX_MGR_get_erx_heartbeat_info( data_p, &len );

    for( i = 0u; i < len; i += ERX_MGR_HEARTBEAT_INFO_SLOT_SIZE )
    {
        if( data_p[i] == iso_position )
        {
            record_p = &data_p[i];
            break;
        }
    }

    return( record_p );
}

/*!
****************************************************************************************************
*
*   \brief         Whether the HUB shows an ERX as lost
*
*   \author        mstewart
*
*   \param[in]     iso_position - The ERX's ISO position
*
*   \return        TRUE if the ERX has a record and its lost bit is set
*
*****************************************************************************************************/
static false_true_et test_is_lost( u8_t iso_position )
{
    u8_t        data[ERX_MGR_HEARTBEAT_INFO_SIZE];
    const u8_t* record_p = test_find_record( data, iso_position );

    return( ( ( record_p != NULL_P ) && ( ( record_p[1u] & ERX_MGR_HEARTBEAT_INFO_LOST ) != 0u ) ) ? TRUE : FALSE );
}

/*!
****************************************************************************************************
*
*   \brief         How many heartbeats of an ERX the HUB has received
*
*   \author        mstewart
*
*   \param[in]     iso_position - The ERX's ISO position
*
*   \return        The count, 0 if the ERX has no record
*
*****************************************************************************************************/
static u32_t test_heartbeat_count( u8_t iso_position )
{
    u8_t        data[ERX_MGR_HEARTBEAT_INFO_SIZE];
    const u8_t* record_p = test_find_record( data, iso_position );

    return( ( record_p != NULL_P ) ? STDC_make_32bit( record_p[2u], record_p[3u], record_p[4u], record_p[5u] ) : 0u );
}

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU with the ERX running its application, and the tester on CAN1, the HUB's tester bus
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The ERX gets into its bootloader the way the real one does: by the tester's request for a programming session, which is the
*                  first step of the download.
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    TMD_ERX_place_instance( TEST_ERX_ISO_POSITION, TMD_ERX_PORT_A );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TMD_DEBUG_log_close();

    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the tester is on the HUB's tester bus, CAN1 at 500 kbit/s, and the ERX on the nodes' bus, CAN2 at 125 kbit/s
*
*   \author        mstewart
*
*   \return        none
*
*   \note          That is how the HUB is wired: the tester is not slowed to the ERX's speed, the gateway is what brings the one down to the other.
*
*****************************************************************************************************/
static void test_assert_the_tester_is_wired_through_the_hub( void )
{
    TEST_ASSERT_EQUAL_UINT32( 500000u, TMD_CAN_get_bit_rate( TEST_BUS_TESTER ) );
    TEST_ASSERT_EQUAL_UINT32( 125000u, TMD_CAN_get_bit_rate( TEST_BUS_NODES ) );
    TEST_ASSERT_EQUAL_UINT8( TEST_BUS_NODES, TMD_ERX_get_can_instance() );
}

/*!
****************************************************************************************************
*
*   \brief         Wire the tester straight to the ERX: they are on one bus at the ERX's 125 kbit/s, and the HUB is not between them
*
*   \author        mstewart
*
*   \return        none
*
*   \note          One instance of TMD_CAN with no ECU on it, that the tester and the ERX share: the tester is on a shared wire (TMD_CAN_set_shared_wire),
*                  and the ERX is put on it. A frame either sends crosses it once, at the ERX's speed, and the other finds it. Call it once the ERX is placed.
*
*****************************************************************************************************/
static void test_wire_the_tester_straight_to_the_erx( void )
{
    const TMD_CANTP_config_st tester_cfg =
    {
        .can_instance = TEST_BUS_ERX_DIRECT,
        .request_id   = SW_TEST_ECU_UDS_REQUEST_ID,    /* a flash points the tester at the ERX */
        .response_id  = SW_TEST_ECU_UDS_RESPONSE_ID
    };

    TMD_CAN_set_bit_rate( TEST_BUS_ERX_DIRECT, SW_TEST_NODES_BUS_BIT_RATE );
    TMD_CAN_set_shared_wire( TEST_BUS_ERX_DIRECT, TRUE );

    TMD_ERX_set_can_instance( TEST_BUS_ERX_DIRECT );
    TMD_CANTP_init( &tester_cfg );
}
/*!
****************************************************************************************************
*
*   \brief         Verify the tester is wired straight to the ERX on a wire at 125 kbit/s, which is the ERX's own speed
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
static void test_assert_the_tester_is_wired_straight_to_the_erx( void )
{
    TEST_ASSERT_EQUAL_UINT32( 125000u, TMD_CAN_get_bit_rate( TEST_BUS_ERX_DIRECT ) );    /* the tester's bus is the ERX's, at the ERX's speed */
    TEST_ASSERT_EQUAL_UINT8( TEST_BUS_ERX_DIRECT, TMD_ERX_get_can_instance() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the download went through: the tester says so, and says where it stopped if it did not
*
*   \author        mstewart
*
*   \param[in]     ok       - What the tester returned
*   \param[in]     result_p - What it did
*
*   \return        none
*
*****************************************************************************************************/
static void test_assert_the_download_went_through( false_true_et ok, const TMD_TESTER_flash_result_st* result_p )
{
    char message[TEST_MESSAGE_LEN];

    snprintf( message, sizeof( message ), "stopped at step %d, uds result %d, nrc 0x%02X, address 0x%05X, erx lost %u, late %u", (int)result_p->failed_step,
              (int)result_p->uds_result, result_p->nrc, (unsigned)result_p->failed_address, (unsigned)TMD_ERX_FBL_get_stats()->sequence_errors,
              (unsigned)TMD_ERX_FBL_get_stats()->rx_timeouts );

    TEST_ASSERT_EQUAL_MESSAGE( TRUE, ok, message );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ERX ended up with the image: nothing lost on the way, every byte the tester sent written, the flash the same, and
*                  the presence pattern written
*
*   \author        mstewart
*
*   \param[in]     result_p - What the tester did
*
*   \return        none
*
*****************************************************************************************************/
static void test_assert_the_erx_holds_the_image( const TMD_TESTER_flash_result_st* result_p )
{
    const TMD_ERX_FBL_stats_st* stats_p      = TMD_ERX_FBL_get_stats();
    const u8_t                  presence[8u] = TMD_ERX_FBL_PRESENCE_PATTERN;
    u8_t                        flashed[8u];

    TEST_ASSERT_EQUAL_UINT32( 0u, stats_p->sequence_errors );
    TEST_ASSERT_EQUAL_UINT32( 0u, stats_p->rx_timeouts );
    TEST_ASSERT_EQUAL_UINT32( TMD_ERX_FBL_dummy_image_bytes(), stats_p->bytes_written );
    TEST_ASSERT_EQUAL_UINT32( TMD_ERX_FBL_dummy_image_bytes(), result_p->bytes );
    TEST_ASSERT_TRUE( TMD_CANTP_get_statistics()->pending_responses > 0u );    /* the erase took longer than P2 and the tester waited */

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_read_flash( TMD_ERX_FBL_APP_START, test_read_back_s, TMD_ERX_FBL_APP_WINDOW ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( TMD_ERX_FBL_get_dummy_image(), test_read_back_s, TMD_ERX_FBL_PRESENCE_ADDRESS - TMD_ERX_FBL_APP_START );    /* all of it up to the presence pattern, which the image does not have */

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_read_flash( TMD_ERX_FBL_PRESENCE_ADDRESS, flashed, sizeof( flashed ) ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( presence, flashed, sizeof( presence ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an application of the size and shape of the ERX's real one downloads through the HUB's gateway with no frame lost,
*                  passes the ERX's own CRC check, has its presence pattern written, and ends up in the ERX's flash byte for byte
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_full_size_application_downloads_through_the_hub_and_passes_the_erx_crc_check( void )
{
    TMD_TESTER_flash_result_st result;
    false_true_et              ok;

    test_assert_the_tester_is_wired_through_the_hub();
    TMD_DEBUG_log_open( "erx_flash_through_the_hub" );

    ok = TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( TEST_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result );

    test_assert_the_download_went_through( ok, &result );
    test_assert_the_erx_holds_the_image( &result );

    TEST_ASSERT_TRUE( result.transfer_ms <= TEST_IMAGE_MAX_MS );    /* the gateway keeps CAN2 busy: a slow tick or a long separation time shows here */
}

/*!
****************************************************************************************************
*
*   \brief         Verify an image whose CRC is wrong is refused with general reject (0x10), and no presence pattern is written, so the ERX
*                  does not take the application as valid
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_an_image_with_a_bad_crc_is_refused_by_the_erx_and_not_marked_present( void )
{
    TMD_TESTER_flash_result_st result;
    u8_t                       flashed[8u];
    u8_t                       erased[8u];

    memset( erased, 0xFFu, sizeof( erased ) );

    test_assert_the_tester_is_wired_through_the_hub();
    TMD_DEBUG_log_open( "erx_flash_bad_crc_through_the_hub" );

    TEST_ASSERT_EQUAL( FALSE, TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( TEST_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC + 1u ), &result ) );    /* a CRC that is not the image's */

    TEST_ASSERT_EQUAL( TMD_TESTER_STEP_CHECKSUM, result.failed_step );    /* every block went in: it is the check of the image that refused it */
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_ERROR_NEGATIVE_RESPONSE, result.uds_result );
    TEST_ASSERT_EQUAL_HEX8( 0x10u, result.nrc );

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_read_flash( TMD_ERX_FBL_PRESENCE_ADDRESS, flashed, sizeof( flashed ) ) );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( erased, flashed, sizeof( erased ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the tester is told a separation time the nodes' bus can keep up with: the ERX asks for none (its block size and
*                  separation time are 0), which CAN2 cannot keep up with, so the HUB passes on one it can. The tester sends the requests, so
*                  what it was told is what its own transport received.
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The erase request is 13 bytes: a first frame, the flow control the ERX answers it with, and a consecutive frame.
*
*****************************************************************************************************/
void test_the_hub_tells_the_tester_a_separation_time_the_nodes_bus_can_keep_up_with( void )
{
    const TMD_CANTP_statistics_st* cantp_p = TMD_CANTP_get_statistics();
    TMD_TESTER_flash_result_st     result;

    test_assert_the_tester_is_wired_through_the_hub();
    test_assert_the_download_went_through( TMD_TESTER_flash_prepare( TMD_ERX_FBL_make_flash_job( TEST_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result ), &result );

    TEST_ASSERT_TRUE( cantp_p->multi_frames_sent > 0u );                             /* the tester did send a request in several frames */
    TEST_ASSERT_TRUE( cantp_p->last_flow_control_stmin >= TEST_FC_STMIN_MIN_MS );    /* and was told to leave a gap, though the ERX asked for none */
    TEST_ASSERT_TRUE( cantp_p->last_flow_control_stmin <= TEST_FC_STMIN_MAX_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the same download with the tester wired straight to the ERX, and no HUB between them: the ERX loses no frame
*                  though the tester sends flat out, as the ERX asks, and ends up with the same image
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Nothing slows the tester here: the flow control it is told is the ERX's own, no separation time.
*
*****************************************************************************************************/
void test_the_same_application_downloads_straight_to_the_erx_without_the_hub( void )
{
    TMD_TESTER_flash_result_st result;
    false_true_et              ok;

    test_wire_the_tester_straight_to_the_erx();
    test_assert_the_tester_is_wired_straight_to_the_erx();
    TMD_DEBUG_log_open( "erx_flash_straight_to_the_erx" );

    ok = TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( TEST_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result );

    test_assert_the_download_went_through( ok, &result );
    test_assert_the_erx_holds_the_image( &result );

    TEST_ASSERT_EQUAL_HEX8( 0x00u, TMD_CANTP_get_statistics()->last_flow_control_stmin );    /* told what the ERX asks: nothing */
    TEST_ASSERT_TRUE( result.transfer_ms <= TEST_DIRECT_IMAGE_MAX_MS );
}

/*!
****************************************************************************************************
*
*   \brief         Verify an ERX is flashed through the HUB by its own CAN IDs while another ERX on the same bus carries on undisturbed
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Two ERXs share CAN2, as the five on a port do. The tester downloads to the one at the first axle's outer left (0x716 and 0x616) and not to
*                  the one at the centre line (0x718 and 0x618), which is there throughout: it must not be sent into its bootloader or answer for the other, its
*                  heartbeat must go on through the whole download so the HUB still hears it, and afterwards it must still answer a read on its own IDs.
*
*****************************************************************************************************/
void test_an_erx_is_flashed_by_its_own_id_while_another_on_the_same_bus_carries_on( void )
{
    TMD_TESTER_flash_result_st result;
    u8_t                       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t                      length    = 0u;
    const u8_t                 request[] = { 0x22u, (u8_t)( TMD_ERX_UDS_DID_PART_NUMBER >> 8u ), (u8_t)TMD_ERX_UDS_DID_PART_NUMBER };
    const u8_t*                part_number_p;
    u8_t                       part_number_len = 0u;
    u8_t                       other;
    u8_t                       target;
    false_true_et              ok;

    other  = TMD_ERX_find_instance( TEST_ERX_ISO_POSITION );
    target = TMD_ERX_place_instance( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_PORT_A );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, other );
    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, target );
    TEST_ASSERT_EQUAL( TRUE, ERX_MGR_is_erx_available( TEST_ERX_ISO_POSITION ) );
    TEST_ASSERT_EQUAL( TRUE, ERX_MGR_is_erx_available( TEST_OTHER_ERX_ISO_POSITION ) );

    ok = TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result );

    test_assert_the_download_went_through( ok, &result );
    test_assert_the_erx_holds_the_image( &result );

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_is_active( target ) );     /* the one addressed is in its bootloader... */
    TEST_ASSERT_EQUAL( FALSE, TMD_ERX_FBL_is_active( other ) );     /* ...and the other never left its application */
    TEST_ASSERT_EQUAL( TRUE, ERX_MGR_is_erx_available( TEST_ERX_ISO_POSITION ) );    /* its heartbeat has gone on all through the download */

    TMD_CANTP_set_ids( TMD_ERX_UDS_REQUEST_BASE_ID + TEST_ERX_ISO_POSITION, TMD_ERX_UDS_RESPONSE_BASE_ID + TEST_ERX_ISO_POSITION );
    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( request, (u16_t)sizeof( request ), response, (u16_t)sizeof( response ), &length, NULL_P ) );

    part_number_p = TMD_ERX_UDS_get_did_value( TMD_ERX_UDS_DID_PART_NUMBER, &part_number_len );
    TEST_ASSERT_NOT_NULL( part_number_p );
    TEST_ASSERT_EQUAL_UINT16( (u16_t)( 3u + part_number_len ), length );
    TEST_ASSERT_EQUAL_HEX8( 0x62u, response[0u] );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( part_number_p, &response[3u], part_number_len );    /* its own answer, not the bootloader's */
}
/*!
****************************************************************************************************
*
*   \brief         Flash one ERX by its own IDs, reset it into its application, and check it and the other ERX on the bus
*
*   \author        mstewart
*
*   \param[in]     position       - The ISO position of the ERX to flash: its IDs are 0x700 and 0x600 plus it
*   \param[in]     node           - Its TMD_ERX instance
*   \param[in]     other_position - The ISO position of the other ERX
*   \param[in]     other_node     - Its TMD_ERX instance
*
*   \return        none
*
*   \note          After the download the tester resets the ERX (0x11 0x01): its bootloader says response pending, and its application, which is up 0.63 s
*                  later, sends the 0x51 0x01. The ERX then has its image, is no longer in its bootloader, and is heard by the HUB again. The other ERX has been
*                  in its application all the while.
*
*****************************************************************************************************/
static void test_flash_one_erx_then_reset_it( u8_t position, u8_t node, u8_t other_position, u8_t other_node )
{
    TMD_TESTER_flash_result_st result;
    u8_t                       response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t                      length          = 0u;
    u32_t                      programmed_crc  = 0u;
    u32_t                      heard_before;
    const u8_t                 reset_request[] = { 0x11u, 0x01u };
    false_true_et              ok;

    ok = TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( position, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result );

    test_assert_the_download_went_through( ok, &result );
    test_assert_the_erx_holds_the_image( &result );

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_is_active( node ) );
    TEST_ASSERT_EQUAL( FALSE, TMD_ERX_FBL_is_active( other_node ) );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( reset_request, (u16_t)sizeof( reset_request ), response, (u16_t)sizeof( response ), &length, NULL_P ) );
    TEST_ASSERT_EQUAL_UINT16( 2u, length );
    TEST_ASSERT_EQUAL_HEX8( 0x51u, response[0u] );
    TEST_ASSERT_EQUAL_HEX8( 0x01u, response[1u] );

    TEST_ASSERT_EQUAL( FALSE, TMD_ERX_FBL_is_active( node ) );    /* it is in its application again... */
    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_get_programmed_image_crc( node, &programmed_crc ) );    /* ...with the image it was given */
    TEST_ASSERT_EQUAL_HEX32( TMD_ERX_FBL_DUMMY_IMAGE_CRC, programmed_crc );

    heard_before = test_heartbeat_count( position );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );
    TEST_ASSERT_TRUE( test_heartbeat_count( position ) > heard_before );    /* the HUB hears it again */
    TEST_ASSERT_EQUAL( TRUE, ERX_MGR_is_erx_available( other_position ) );    /* and it never stopped hearing the other */
}

/*!
****************************************************************************************************
*
*   \brief         Verify two ERXs on the same bus are flashed one after the other, each by its own IDs, the first again after the second
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The tester talks to one ERX at a time, and which one is only the IDs it uses. Each download passes, each ERX is reset into its application
*                  and heard by the HUB again before the next, and both keep the image they were given: the one flashed second does not disturb the first, and
*                  the first can be flashed again.
*
*****************************************************************************************************/
void test_erxs_are_flashed_one_after_the_other_on_the_same_bus( void )
{
    u32_t crc = 0u;
    u8_t  centre;
    u8_t  outer_left;

    centre     = TMD_ERX_find_instance( TEST_ERX_ISO_POSITION );
    outer_left = TMD_ERX_place_instance( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_PORT_A );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, centre );
    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, outer_left );

    test_flash_one_erx_then_reset_it( TEST_OTHER_ERX_ISO_POSITION, outer_left, TEST_ERX_ISO_POSITION, centre );    /* 0x716 first... */
    test_flash_one_erx_then_reset_it( TEST_ERX_ISO_POSITION, centre, TEST_OTHER_ERX_ISO_POSITION, outer_left );    /* ...then 0x718... */
    test_flash_one_erx_then_reset_it( TEST_OTHER_ERX_ISO_POSITION, outer_left, TEST_ERX_ISO_POSITION, centre );    /* ...and 0x716 again */

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_get_programmed_image_crc( outer_left, &crc ) );    /* both still hold what they were given */
    TEST_ASSERT_EQUAL_HEX32( TMD_ERX_FBL_DUMMY_IMAGE_CRC, crc );
    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_get_programmed_image_crc( centre, &crc ) );
    TEST_ASSERT_EQUAL_HEX32( TMD_ERX_FBL_DUMMY_IMAGE_CRC, crc );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a virgin ERX is at the default ISO position, and one placed at another position is flashed by that position's IDs
*
*   \author        mstewart
*
*   \return        none
*
*   \note          setUp() fits a virgin ERX: it is at 0x18, so the tester reaches it on 0x718 and 0x618. The ERX fitted afterwards at 0x16 is
*                  reached on 0x716 and 0x616 only: the download of that one goes through and the virgin ERX stays in its application.
*
*****************************************************************************************************/
void test_a_virgin_erx_is_at_the_default_position_and_one_placed_elsewhere_is_flashed_by_its_own_ids( void )
{
    TMD_ERX_config_st          config;
    TMD_TESTER_flash_result_st result;
    u8_t                       virgin;
    u8_t                       placed;
    false_true_et              ok;

    virgin = TMD_ERX_find_instance( TMD_ERX_DEFAULT_ISO_POSITION );
    placed = TMD_ERX_place_instance( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_PORT_A );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, virgin );
    TEST_ASSERT_NOT_EQUAL( TMD_ERX_NO_INSTANCE, placed );
    TEST_ASSERT_EQUAL_HEX8( 0x18u, TMD_ERX_DEFAULT_ISO_POSITION );
    TEST_ASSERT_EQUAL_HEX8( TEST_ERX_ISO_POSITION, TMD_ERX_DEFAULT_ISO_POSITION );

    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_get_configuration( placed, &config ) );
    TEST_ASSERT_EQUAL_HEX8( TEST_OTHER_ERX_ISO_POSITION, config.iso_position );

    ok = TMD_TESTER_flash( TMD_ERX_FBL_make_flash_job( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC ), &result );

    test_assert_the_download_went_through( ok, &result );
    TEST_ASSERT_EQUAL( TRUE, TMD_ERX_FBL_is_active( placed ) );
    TEST_ASSERT_EQUAL( FALSE, TMD_ERX_FBL_is_active( virgin ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the HUB does not flag the ERX it is flashing as lost, nor the ERX next to it, and ends the update once the tester is silent
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The ERX in its bootloader sends no heartbeat, and the download is far longer than ERX_MGR_HEARTBEAT_TIMEOUT_MS. The
*                  gateway marks the ERX as being updated when the tester's programming session request goes through (nothing is set by the
*                  test). The lost flag is read after each phase of the download (erase, transfer, checksum) for both ERXs on the port, and
*                  again once the flashed one is back in its application and the tester has been quiet long enough for the update to be over.
*                  The HUB has no DTC for a lost ERX, so the flag is all there is to read.
******************************************************************************************************/
void test_flashing_an_erx_does_not_set_the_lost_flag_of_it_or_of_the_one_next_to_it( void )
{
    TMD_TESTER_flash_result_st    result;
    const TMD_TESTER_flash_job_st* job_p;
    u8_t                          response[SW_TEST_TESTER_RESPONSE_MAX_LEN];
    u16_t                         length          = 0u;
    const u8_t                    reset_request[] = { 0x11u, 0x01u };
    u32_t                         heard_before;

    TMD_ERX_place_instance( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_PORT_A );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ) );
    TEST_ASSERT_EQUAL( FALSE, test_is_lost( TEST_ERX_ISO_POSITION ) );

    job_p = TMD_ERX_FBL_make_flash_job( TEST_OTHER_ERX_ISO_POSITION, TMD_ERX_FBL_DUMMY_IMAGE_CRC );


    test_assert_the_download_went_through( TMD_TESTER_flash_prepare( job_p, &result ), &result );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ), "flashed ERX lost after the erase" );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_ERX_ISO_POSITION ), "other ERX lost after the erase" );

    TEST_ASSERT_EQUAL( TRUE, TMD_TESTER_flash_transfer( job_p, &result ) );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ), "flashed ERX lost after the transfer" );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_ERX_ISO_POSITION ), "other ERX lost after the transfer" );

    TEST_ASSERT_EQUAL( TRUE, TMD_TESTER_flash_finish( job_p, &result ) );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ), "flashed ERX lost after the checksum" );
    TEST_ASSERT_EQUAL_MESSAGE( FALSE, test_is_lost( TEST_ERX_ISO_POSITION ), "other ERX lost after the checksum" );

    TEST_ASSERT_EQUAL( TMD_UDS_RESULT_OK, TMD_UDS_send_request( reset_request, (u16_t)sizeof( reset_request ), response, (u16_t)sizeof( response ), &length, NULL_P ) );
    heard_before = test_heartbeat_count( TEST_OTHER_ERX_ISO_POSITION );
    TMD_TIME_advance_time_ms( TEST_ERX_SETTLE_MS );

    TEST_ASSERT_EQUAL( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ) );
    TEST_ASSERT_EQUAL( FALSE, test_is_lost( TEST_ERX_ISO_POSITION ) );
    TEST_ASSERT_TRUE( test_heartbeat_count( TEST_OTHER_ERX_ISO_POSITION ) > heard_before );    /* back in its application and heard again */

    TMD_TIME_advance_time_ms( TEST_TESTER_IDLE_MS );    /* the tester stays quiet: the update is over */

    TEST_ASSERT_EQUAL( FALSE, test_is_lost( TEST_OTHER_ERX_ISO_POSITION ) );
    TEST_ASSERT_EQUAL( TRUE, ERX_MGR_is_erx_available( TEST_OTHER_ERX_ISO_POSITION ) );    /* and no longer held as being updated */
}
/****************************** END OF FILE *******************************************************/
