/*! \file
*               Author: mstewart
*   \brief      Software tests - how every test starts the ECU
*
*   \details    The one TMD_INIT_cfg_st the software tests share: the virtual STM32 powered on, the clock and the CAN
*               models wired to it, a blank flash and then the real app_main(). setUp() of a test is
*               TMD_INIT_init( &sw_test_init_cfg_g ), so every test case begins as a virgin ECU.
*
*               The includes of a software test are in this order, in blocks, so the tests read the same: the test framework
*               (unity.h, test_timing.h), STDC.h, the firmware (INTEGRATION_STUBS.h, HAL_config.h, then its modules A to Z),
*               the virtual STM32 (vhw.h, then stm32f10x_*.h A to Z), the TMD models A to Z, and this header last.
*/
#ifndef SW_TEST_STUBS_H
#define SW_TEST_STUBS_H

/***************************************************************************************************
**                                        Includes                                                **
***************************************************************************************************/
#include "STDC.h"
#include "TMD_INIT.h"
#include "TMD_CAN.h"

/***************************************************************************************************
**                                       Definitions                                              **
***************************************************************************************************/
#define TEST_BUS_TESTER          ( TMD_CAN_INSTANCE_0 )    /* CAN1: bxCAN through the TJA1051 */
#define TEST_BUS_NODES           ( TMD_CAN_INSTANCE_1 )    /* CAN2: the MCP2515 */

/* The tester on CAN1 is the UDS tester model (TMD_UDS, over TMD_CANTP), started with every other model: TMD_UDS_send_request() and the
   services on it. It starts at the ECU's own UDS server; TMD_CANTP_set_ids() points it at another node. */
#define SW_TEST_ECU_UDS_REQUEST_ID      ( 0x7E0u )  /* the ECU's physical request, APP_CAN_RX_ID in INTEGRATION_STUBS.c */
#define SW_TEST_ECU_UDS_RESPONSE_ID     ( 0x7E8u )  /* its answer, APP_CAN_TX_ID */
#define SW_TEST_TESTER_TIMEOUT_MS       ( 500u )    /* how long the tester waits for the whole answer, the first frame to the last */
#define SW_TEST_TESTER_RESPONSE_MAX_LEN ( 128u )    /* what a test's buffer for an answer has to hold */

/***************************************************************************************************
**                                    Exported Globals                                            **
***************************************************************************************************/
extern const TMD_INIT_cfg_st sw_test_init_cfg_g;    /* power on, models, blank flash, app_main(): a virgin ECU */

/***************************************************************************************************
**                                    Function Prototypes                                        **
***************************************************************************************************/
/* A test puts an ERX on the bus with TMD_ERX_place_instance( APP_ISO_..., TMD_ERX_PORT_A or B ), and takes it off and puts it back
   with TMD_ERX_unplug_instance() / TMD_ERX_plug_in_instance() on TMD_ERX_find_instance( APP_ISO_... ). The HUB powers it. */

#endif /* SW_TEST_STUBS_H */

/****************************** END OF FILE *******************************************************/
