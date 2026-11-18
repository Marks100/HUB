/*! \file
*               Author: mstewart
*   \brief      Gateway - software test
*
*   \details    The tester's bus (CAN1) and the nodes' bus (CAN2) through the real HAL_BRD, HAL_CAN, HAL_SPI, the SPL,
*               the TJA1051 driver, CANIF, PDUR, DIAG_GW and MCP2515 driver, built from the project's own tables in
*               INTEGRATION_STUBS.c. The STM32 is the virtual hardware (vhw.h); the buses are two TMD_CAN instances.
*               CAN1 reaches its bus through the TJA1051 model, which the firmware has to take out of standby (EN pin,
*               TJA1051_init) before a frame passes either way; CAN2 is the MCP2515 on SPI1 and its chip model. The wiring and
*               the start-up list are in sw_stubs/SW_TEST_STUBS.c; setUp() starts every test case as a virgin ECU.
*/
#include "unity.h"
#include "test_timing.h"

#include "STDC.h"

#include "INTEGRATION_STUBS.h"
#include "HAL_config.h"
#include "HAL_BRD.h"
#include "HAL_CAN.h"
#include "HAL_SPI.h"
#include "MCP2515.h"

#include "vhw.h"
#include "stm32f10x_can.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_spi.h"

#include "TMD_CAN.h"
#include "TMD_ERX_UDS.h"
#include "TMD_INIT.h"
#include "TMD_MCP2515.h"
#include "TMD_TIME.h"
#include "TMD_TJA1051.h"

#include "SW_TEST_STUBS.h"

#define TEST_FRAME_SETTLE_MS     ( 10u )    /* two scheduler passes: a frame that arrived crosses the gateway */
#define TEST_MCP2515_OSCILLATOR_HZ  ( 8000000u )    /* the crystal on the MCP2515 board */
#define TEST_NODES               ( 4u )     /* the nodes a burst goes to: DIAG_GW tracks four at once */

/* Request IDs of four nodes the HUB's own UDS client is not reading, so the gateway passes a tester's frame to each at once */
static const u32_t test_node_ids_c[ TEST_NODES ] =
{
    TMD_ERX_UDS_REQUEST_BASE_ID + APP_ISO_AXLE_3_CENTRE,
    TMD_ERX_UDS_REQUEST_BASE_ID + APP_ISO_AXLE_2_INNER_RIGHT,
    TMD_ERX_UDS_REQUEST_BASE_ID + APP_ISO_AXLE_2_OUTER_RIGHT,
    TMD_ERX_UDS_REQUEST_BASE_ID + APP_ISO_AXLE_5_OUTER_RIGHT
};
#define TEST_REQUEST_ID          ( TMD_ERX_UDS_REQUEST_BASE_ID  + APP_ISO_AXLE_3_CENTRE )    /* 0x738: a node's request, the third axle's centre line */
#define TEST_RESPONSE_ID         ( TMD_ERX_UDS_RESPONSE_BASE_ID + APP_ISO_AXLE_3_CENTRE )    /* 0x638: and its response */

/*!
****************************************************************************************************
*
*   \brief         A frame as the CAN bus model takes it
*
*   \author        mstewart
*
*   \param[out]    frame_p - The frame
*   \param[in]     id      - Arbitration ID, 11 bit
*   \param[in]     data_p  - Eight data bytes
*
*   \return        none
*
*****************************************************************************************************/
static void test_make_frame( TMD_CAN_message_st* frame_p, u32_t id, const u8_t* data_p )
{
    STDC_memset( frame_p, 0x00u, (u16_t)sizeof( TMD_CAN_message_st ) );

    frame_p->message_id  = id;
    frame_p->data_length = 8u;
    STDC_memcpy( frame_p->data, data_p, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Every test case starts as a virgin ECU: the shared start-up list of SW_TEST_STUBS.c
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void setUp( void )
{
    TMD_INIT_init( &sw_test_init_cfg_g );

    TEST_TIMING_START();
}

void tearDown( void )
{
    TEST_TIMING_STOP();
}

/*!
****************************************************************************************************
*
*   \brief         Verify the firmware takes the TJA1051 out of standby (EN pin driven by TJA1051_init through
*                  HAL_BRD), which is what lets frames through the transceiver at all
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_firmware_switches_the_tja1051_to_normal_mode( void )
{
    TEST_ASSERT_EQUAL( TMD_TJA1051_MODE_NORMAL, TMD_TJA1051_get_mode( TEST_BUS_TESTER ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a tester request on CAN1 leaves on CAN2 with the same ID and payload, and
*                  nothing goes back out on CAN1
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_tester_request_reaches_the_node_on_can2( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_REQUEST_ID, sent.message_id );
    TEST_ASSERT_EQUAL( 8u, sent.data_length );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( data, sent.data, 8u );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_REQUEST_ID ) );
    TEST_ASSERT_EQUAL( 0u, TMD_MCP2515_get_protocol_error_count() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the ECU can be power cycled: after a restart that keeps its NVM, app_main() has run again on a
*                  freshly powered MCU and the gateway forwards a tester request as before
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_the_gateway_forwards_again_after_the_ecu_restarts( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st sent;

    TMD_INIT_restart_ecu();

    test_make_frame( &request, TEST_REQUEST_ID, data );
    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( TMD_TJA1051_MODE_NORMAL, TMD_TJA1051_get_mode( TEST_BUS_TESTER ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_REQUEST_ID, sent.message_id );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a node's reply on CAN2 leaves on CAN1 with the same ID and payload
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_node_reply_on_can2_reaches_the_tester_on_can1( void )
{
    const u8_t         request_data[ 8u ]  = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const u8_t         response_data[ 8u ] = { 0x02u, 0x50u, 0x03u, 0x00u, 0x32u, 0x01u, 0xF4u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st response;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, request_data );
    test_make_frame( &response, TEST_RESPONSE_ID, response_data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_NODES, &response );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID ) );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RESPONSE_ID, sent.message_id );
    TEST_ASSERT_EQUAL( 8u, sent.data_length );
    TEST_ASSERT_EQUAL_HEX8_ARRAY( response_data, sent.data, 8u );
}

/*!
****************************************************************************************************
*
*   \brief         Verify that with the TJA1051 in standby a tester request on CAN1 never reaches the gateway
*                  - the transceiver drops it - so nothing leaves on CAN2
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_request_does_not_pass_the_tja1051_in_standby( void )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    TMD_CAN_message_st request;

    test_make_frame( &request, TEST_REQUEST_ID, data );
    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_STANDBY );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 1u, TMD_TJA1051_get_dropped_rx_count( TEST_BUS_TESTER ) );
    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_NODES, TEST_REQUEST_ID ) );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a reply for the tester waits in the bxCAN mailbox while the TJA1051 is in standby, as
*                  the bxCAN retransmits until the bus takes the frame, and leaves once the transceiver is back in
*                  normal mode
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_reply_waits_in_the_mailbox_until_the_tja1051_is_back_in_normal_mode( void )
{
    const u8_t         request_data[ 8u ]  = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u };
    const u8_t         response_data[ 8u ] = { 0x02u, 0x50u, 0x03u, 0x00u, 0x32u, 0x01u, 0xF4u, 0x00u };
    TMD_CAN_message_st request;
    TMD_CAN_message_st response;
    TMD_CAN_message_st sent;

    test_make_frame( &request, TEST_REQUEST_ID, request_data );
    test_make_frame( &response, TEST_RESPONSE_ID, response_data );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_STANDBY );

    TMD_CAN_inject_frame_to_ecu( TEST_BUS_NODES, &response );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL( 0u, TMD_CAN_get_message_count_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID ) );
    TEST_ASSERT_TRUE( CAN_SIM_get_mailboxes_in_use() > 0u );
    TEST_ASSERT_TRUE( TMD_TJA1051_get_blocked_tx_count( TEST_BUS_TESTER ) > 0u );

    TJA1051_set_mode( &tja1051_main_instance_s, TJA1051_MODE_NORMAL );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );    /* the bxCAN offers the frame again */

    TEST_ASSERT_EQUAL( 0u, CAN_SIM_get_mailboxes_in_use() );
    TEST_ASSERT_EQUAL( TRUE, TMD_CAN_consume_message_from_ecu( TEST_BUS_TESTER, TEST_RESPONSE_ID, &sent ) );
    TEST_ASSERT_EQUAL_HEX32( TEST_RESPONSE_ID, sent.message_id );
}

/*!
****************************************************************************************************
*
*   \brief         Verify the firmware sets CAN2, the MCP2515, to the bit rate the ERXs run at: the rate that the bit timing registers
*                  it wrote say, from the 8 MHz crystal, is the one the test bus runs at (SW_TEST_NODES_BUS_BIT_RATE, 125 kbit/s)
*
*   \author        mstewart
*
*   \return        none
*
*   \note          A bit is a number of time quanta: the sync segment (1), the propagation segment and phase segment 1 (CNF2) and
*                  phase segment 2 (CNF3, which the controller takes from there when BTLMODE, bit 7 of CNF2, is set). A time quantum
*                  is 2 x ( BRP + 1 ) oscillator periods, BRP being CNF1's low six bits.
*
*****************************************************************************************************/
void test_the_firmware_sets_can2_to_the_bit_rate_of_the_erxs( void )
{
    const u8_t  cnf1        = TMD_MCP2515_read_register( MCP2515_REG_CNF1 );
    const u8_t  cnf2        = TMD_MCP2515_read_register( MCP2515_REG_CNF2 );
    const u8_t  cnf3        = TMD_MCP2515_read_register( MCP2515_REG_CNF3 );
    const u32_t quanta      = 1u + ( (u32_t)( cnf2 & 0x07u ) + 1u ) + ( (u32_t)( ( cnf2 >> 3u ) & 0x07u ) + 1u ) + ( (u32_t)( cnf3 & 0x07u ) + 1u );
    const u32_t quantum_div = 2u * ( (u32_t)( cnf1 & 0x3Fu ) + 1u );

    TEST_ASSERT_BITS_HIGH( 0x80u, cnf2 );    /* BTLMODE: phase segment 2 is CNF3's */
    TEST_ASSERT_EQUAL_UINT32( SW_TEST_NODES_BUS_BIT_RATE, TEST_MCP2515_OSCILLATOR_HZ / ( quantum_div * quanta ) );
}

/*!
****************************************************************************************************
*
*   \brief         Take the frames the ECU has sent on CAN2, in the order they went, that carry one of four node request IDs
*
*   \author        mstewart
*
*   \param[out]    frames_p - Room for TEST_NODES frames
*
*   \return        How many there were. Anything else the ECU sent on CAN2 is dropped.
*
*****************************************************************************************************/
static u8_t test_take_node_frames( TMD_CAN_message_st* frames_p )
{
    TMD_CAN_message_st frame;
    u8_t               count = 0u;
    u8_t               i;

    while( TMD_CAN_pop_frame_from_ecu( TEST_BUS_NODES, &frame ) == TRUE )
    {
        for( i = 0u; ( i < TEST_NODES ) && ( count < TEST_NODES ); i++ )
        {
            if( frame.message_id == test_node_ids_c[i] )
            {
                frames_p[count] = frame;
                count++;
            }
        }
    }

    return( count );
}

/*!
****************************************************************************************************
*
*   \brief         A tester request to a node as the CAN bus model takes it: a single frame, DiagnosticSessionControl, whose last
*                  data byte says which of a run of requests it is
*
*   \author        mstewart
*
*   \param[in]     node_index - Which of the four nodes: the ID is test_node_ids_c[node_index]
*   \param[in]     sequence   - Last data byte
*
*   \return        none
*
*****************************************************************************************************/
static void test_inject_request( u8_t node_index, u8_t sequence )
{
    const u8_t         data[ 8u ] = { 0x02u, 0x10u, 0x03u, 0x00u, 0x00u, 0x00u, 0x00u, sequence };
    TMD_CAN_message_st request;

    test_make_frame( &request, test_node_ids_c[node_index], data );
    TMD_CAN_inject_frame_to_ecu( TEST_BUS_TESTER, &request );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a burst of frames for the nodes leaves CAN2 in the order it came, each once, when the bus is busy: the chip
*                  holds three, the fourth waits in the interface layer, and when the bus is free all four go, none lost and none
*                  twice
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_burst_for_the_nodes_leaves_in_order_and_once_when_the_bus_is_busy( void )
{
    TMD_CAN_message_st frames[ TEST_NODES ];
    u8_t               i;

    TMD_MCP2515_set_tx_hold( TRUE );

    for( i = 0u; i < TEST_NODES; i++ )
    {
        test_inject_request( i, i );
    }

    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT8( 0u, test_take_node_frames( frames ) );    /* the bus is busy: not one has gone */

    TMD_MCP2515_set_tx_hold( FALSE );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT8( TEST_NODES, test_take_node_frames( frames ) );

    for( i = 0u; i < TEST_NODES; i++ )
    {
        TEST_ASSERT_EQUAL_HEX32( test_node_ids_c[i], frames[i].message_id );
        TEST_ASSERT_EQUAL_UINT8( i, frames[i].data[7u] );
    }

    TMD_TIME_advance_time_ms( 100u );

    TEST_ASSERT_EQUAL_UINT8( 0u, test_take_node_frames( frames ) );    /* and nothing twice, later */
    TEST_ASSERT_EQUAL( 0u, TMD_MCP2515_get_protocol_error_count() );
}

/*!
****************************************************************************************************
*
*   \brief         Verify a frame the bus does not take for 100 ms is dropped and does not go out late: when the bus is free again
*                  the next frame goes and the old one does not
*
*   \author        mstewart
*
*   \return        none
*
*****************************************************************************************************/
void test_a_frame_the_bus_does_not_take_in_time_is_dropped_and_not_sent_late( void )
{
    TMD_CAN_message_st frames[ TEST_NODES ];

    TMD_MCP2515_set_tx_hold( TRUE );

    test_inject_request( 0u, 0xAAu );
    TMD_TIME_advance_time_ms( 150u );                                  /* longer than the ECU waits for the bus */

    TMD_MCP2515_set_tx_hold( FALSE );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT8( 0u, test_take_node_frames( frames ) );    /* the stale one was aborted, so it was not sent */

    test_inject_request( 1u, 0xBBu );
    TMD_TIME_advance_time_ms( TEST_FRAME_SETTLE_MS );

    TEST_ASSERT_EQUAL_UINT8( 1u, test_take_node_frames( frames ) );    /* the buffer is free again: this one goes */
    TEST_ASSERT_EQUAL_HEX32( test_node_ids_c[1], frames[0].message_id );
    TEST_ASSERT_EQUAL_HEX8( 0xBBu, frames[0].data[7u] );
}

/****************************** END OF FILE *******************************************************/
