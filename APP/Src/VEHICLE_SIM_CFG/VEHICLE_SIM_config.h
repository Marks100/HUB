#ifndef VEHICLE_SIM_CONFIG_H
#define VEHICLE_SIM_CONFIG_H

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "STDC.h"
#include "PDUR.h"
#include "APS.h"
#include "CPS.h"
#include "TCU.h"
#include "GKT_SHIFTER.h"
#include "MQB_CLUSTER.h"
#include "ENGINE_SIM.h"

/***************************************************************************************************
**                              Exported Globals                                                  **
***************************************************************************************************/
extern       APS_instance_st        aps_pedal_instance_s;
extern const APS_cfg_st             aps_pedal_cfg_s;
extern       CPS_instance_st        cps_crank_instance_s;
extern const CPS_cfg_st             cps_crank_cfg_s;
extern const TCU_cfg_st             tcu_cfg_s;
extern const GKT_SHIFTER_cfg_st     gkt_shifter_cfg_s;
extern const MQB_CLUSTER_cfg_st     mqb_cluster_cfg_s;
extern       ENGINE_SIM_instance_st engine_sim_instance_s;
extern const ENGINE_SIM_cfg_st      engine_sim_cfg_s;

/***************************************************************************************************
**                              Function Prototypes                                               **
***************************************************************************************************/
/* upperLayerRxIndication for INTEGRATION_STUBS.c's pdur_routing_table_s, APP_PDU_GKT_LEVER_RX
   route - not STATIC so that table (which also carries the unrelated sensor/UDS routes, so it
   stayed in INTEGRATION_STUBS.c) can still reference it. */
void app_pdur_gkt_lever_rx( PDUR_pdu_id_t pdu_id, u8_t* data_p, u16_t len );
void app_tcu_gear_state_local_relay( void );  /* call once per cycle, right after TCU_tick() - see MODE_MGR.c */
void app_engine_sim_cluster_relay( void );    /* call once per cycle, right after ENGINE_SIM_tick() - see MODE_MGR.c */

#endif /* VEHICLE_SIM_CONFIG_H */

/****************************** END OF FILE *******************************************************/
