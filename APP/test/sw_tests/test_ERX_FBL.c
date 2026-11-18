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

#include "TMD_CAN.h"
#include "TMD_CANTP.h"
#include "TMD_DEBUG.h"
#include "TMD_ERX.h"
#include "TMD_ERX_FBL.h"
#include "TMD_INIT.h"
#include "TMD_TESTER.h"
#include "TMD_TIME.h"
#include "TMD_UDS.h"

#include "SW_TEST_STUBS.h"

#define TEST_ERX_SETTLE_MS          ( 2500u )     /* the ERX is up, and the HUB's own client has read what it wants from it: the gateway is free for the tester */
#define TEST_BUS_ERX_DIRECT         ( TMD_CAN_INSTANCE_2 )    /* a bus with no ECU on it, that a tester wired straight to an ERX shares with it: the ERX's speed, SW_TEST_NODES_BUS_BIT_RATE */
#define TEST_ERX_ISO_POSITION       ( APP_ISO_AXLE_1_CENTRE )    /* the first axle's centre line: IDs 0x718 and 0x618 */
#define TEST_IMAGE_MAX_MS           ( 75000u )    /* through the HUB: the 1204 blocks of 253 bytes took 71.8 s, about 60 ms a block, of which CAN2 is 37 ms. A 5 ms gateway tick and 2 ms separation took 138 s */
#define TEST_DIRECT_IMAGE_MAX_MS    ( 65000u )    /* straight to the ERX: the same 1204 blocks took 60.1 s, the wire and the ERX's writes with no HUB, which is 11 s less */
#define TEST_FC_STMIN_MIN_MS        ( 1u )        /* the least separation CAN2 and the MCP2515 can keep up with (a frame is about 1 ms on CAN2), which the tester has to be told */
#define TEST_FC_STMIN_MAX_MS        ( 0x7Fu )     /* above this the field is not milliseconds */
#define TEST_MESSAGE_LEN            ( 120u )

static u8_t test_read_back_s[TMD_ERX_FBL_APP_WINDOW];

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

/****************************** END OF FILE *******************************************************/
