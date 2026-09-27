/*! \file
*               Author: mstewart
*   \brief      Bench vehicle simulation wiring - crank sensor, gearbox, and CAN dash cluster
*
*   \details    Binds the generic APS/CPS/TCU/GKT_SHIFTER/MQB_CLUSTER/ENGINE_SIM modules to this
*               specific bench rig: an APS pedal feeding an ENGINE_SIM-simulated engine, a CPS
*               crank sensor stood in for by a plain signal generator, a TCU that both decodes the
*               F30 lever and automatically shifts ENGINE_SIM through gears using its RPM
*               feedback, and a CAN dash cluster (MQB_CLUSTER) fed the results. Split out of
*               INTEGRATION_STUBS.c, which is everything else this board needs (generic
*               HAL/peripheral wiring) - this file is specifically the vehicle-simulation half.
*
*               One piece stayed behind regardless: pdur_routing_table_s (INTEGRATION_STUBS.c)
*               also carries the unrelated sensor and UDS routes, so it's a single array that
*               can't be split across files - only its GKT_SHIFTER RX rows and the TX rows for
*               TCU/GKT_SHIFTER/MQB_CLUSTER are vehicle-sim's concern. Every frame those modules
*               send goes out via PDUR_tx() on the route id given in their cfg structs below
*               (APP_PDU_*_TX, APP_PDU_IDS.h). app_pdur_gkt_lever_rx() below is the
*               APP_PDU_GKT_LEVER_RX row's upperLayerRxIndication - external linkage (not STATIC)
*               purely so that table can still reference it.
*
*               Every tunable scalar (not the real per-gear ratio tables below - those are fixed
*               spec data for a named real gearbox, not a bench "taste" figure) is a named
*               #define in the Defines section, so every PLACEHOLDER/tuning figure in the file
*               can be found and changed in one place. Cfg structs are grouped together per
*               module, top to bottom (TCU/GKT/MQB, CPS, APS, ENGINE_SIM); the small adapter/
*               wrapper functions each struct's function pointers bind to all live together in one
*               block at the end of the file instead, not interleaved with the data - see
*               Application Adapter Functions below.
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "VEHICLE_SIM_config.h"
#include "APP_PDU_IDS.h"    /* APP_PDU_*_TX - the PDUR routes TCU/GKT_SHIFTER/MQB_CLUSTER transmit on */
#include "HAL_CAN.h"
#include "HAL_ADC.h"
#include "HAL_BRD.h"
#include "DWT.h"
#include "TIME.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/

/* Pick the gearbox you want */
#define GEARBOX_DQ381   ( 0u )   /* 7-speed DSG, vRS 245 / facelift diesel */
#define GEARBOX_DQ250   ( 1u )   /* 6-speed DSG, pre-facelift / early Mk3, wet clutch */
#define GEARBOX_722_9   ( 2u )   /* 7-speed torque-converter auto, Mercedes 7G-Tronic Plus */
#define GEARBOX_ZF_8HP  ( 3u )   /* 8-speed torque-converter auto, ZF 8HP - the F30's own gearbox */
#define ACTIVE_GEARBOX  GEARBOX_ZF_8HP

/* TCU tuning - see TCU.h's own doc on these fields */
#define TCU_AUTO_UPSHIFT_RPM            ( 8000u )
#define TCU_AUTO_DOWNSHIFT_RPM          ( 1500u )
#define TCU_MIN_AUTO_SHIFT_INTERVAL_MS  ( 300u )
#define TCU_WHEEL_CIRCUMFERENCE_M       ( 2.00f ) /* ~225/40R18's */
#define TCU_COAST_DECEL_KMH_PER_S       ( 3.0f )  /* PLACEHOLDER: how fast road speed falls in Neutral, km/h per second
                                                     (0.0f = snap to 0). ~38 s to stop from 114 km/h. A real car free-rolling
                                                     in Neutral loses roughly 1-2 km/h/s at speed, so this is a touch brisk
                                                     to be easy to watch on the bench - tune to taste */
#define TCU_GEAR_STATE_PERIOD_MS        ( 20u )
#define TCU_DYNAMICS_PERIOD_MS          ( 20u )   /* RPM + speed + requested throttle, one frame */

/* BENCH ONLY: the REQUESTED THROTTLE is this fixed percentage instead of the APS pedal (APS is still
   ticked, just not read). Set to 0u to use the real pedal. It is a stand-in for the pedal, not a
   stand-in for the RPM: nothing here sets an RPM - ENGINE_SIM turns this request into one (idle +
   throttle% of the way to redline, so 50% gives 800 + 0.5 x 7200 = 4400 RPM, and it free-revs to that
   in Park/Neutral as well). The same request is on the bus in TCU_DYNAMICS_CAN_ID, next to the RPM and
   speed it produces, because TCU reads it through app_read_throttle_percent() too. Why a throttle is
   needed at all: with none, an idling ENGINE_SIM (ENGINE_IDLE_RPM) sits below TCU_AUTO_DOWNSHIFT_RPM,
   and the low-RPM downshift (active in the manual gate too) would pull every manual upshift straight
   back down. With it held, TCU acts on a real RPM and the rev-match
   dip after each shift shows on the tacho and in TCU_DYNAMICS_CAN_ID. Pace matters, because RPM recovers
   slowly in the tall gears: M+ taps faster than roughly one a second sink it under
   TCU_AUTO_DOWNSHIFT_RPM (the tap gets bounced back), and rapid M- taps can rev-match past
   TCU_AUTO_UPSHIFT_RPM (bounced up) - both are TCU's engine protection, not faults. */
#define BENCH_FIXED_THROTTLE_PERCENT    ( 0u )

/* CPS tuning - see CPS.h's own doc on these fields */
#define CPS_TOTAL_TEETH       ( 60u )          /* stand-in for a 60-2 wheel's tooth rate */
#define CPS_STALL_TIMEOUT_US  ( 500000u )
#define CPS_RPM_MAX_CREDIBLE  ( 0xFFFFFFFFu )

/* APS tuning - see APS.h's own doc on these fields. Measured on the bench (PA0/ADC_THROTTLE_CHANNEL -
   see HAL_config.h). Note wot_counts is the ADC's nominal 12-bit full scale (2^12), one past the
   highest count HAL_ADC_read_channel() can actually return (0-4095) - aps_map_to_percent() clamps,
   so a floored pedal reads 99% rather than 100%; re-measure the real WOT count if that matters. */
#define APS_IDLE_COUNTS                     ( 2065u )  /* ch1 raw count, pedal released */
#define APS_WOT_COUNTS                      ( 4092u )  /* ch1 raw count, pedal floored */
#define APS_EMA_WEIGHTING                   ( 50u )    /* light smoothing (tau ~29ms, 90%
                                              * settle ~66ms at this 20ms tick rate - see
                                              * APS_tick()'s call site, MODE_MGR.c) so pedal
                                              * response stays crisp. Single-channel for now (see
                                              * aps_pedal_cfg_s below), so there is no plausibility
                                              * check to catch a genuine sensor fault - just this
                                              * filter */

/* ENGINE_SIM tuning - see ENGINE_SIM.h's own doc on these fields. PLACEHOLDER figures below -
   tune to taste. */
#define ENGINE_IDLE_RPM                   ( 800u )
#define ENGINE_REDLINE_RPM                ( 8000u )
#define ENGINE_INERTIA_KG_M2              ( 0.15f )    /* rotating assembly (crank/flywheel/clutch) 
                                                          moment of * inertia */
#define VEHICLE_MASS_KG                   ( 1450.0f )  /* real curb weight for the class of
                                                          car this gearbox/engine combo suits */
#define ENGINE_PEAK_TORQUE_NM             ( 350u )     /* THIS engine's real torque rating */
#define ENGINE_SHIFT_DURATION_MS          ( 200u )     /* typical real DSG/torque-converter
                                                          shift time under load */

/***************************************************************************************************
**                              Private Function Prototypes                                       **
**  Definitions live in Application Adapter Functions, end of file - forward-declared here only    **
**  because the cfg structs below bind them as function pointers before that point.                **
***************************************************************************************************/
STATIC u16_t app_read_engine_rpm( void );
STATIC u8_t  app_read_throttle_percent( void );

/***************************************************************************************************
**                              TCU / F30 GKT Lever / MQB Dash                                     **
**  NOTE: TCU_GEAR_STATE_CAN_ID (TCU.h) defaults to 0x7F0, unused elsewhere on this bus - fine as  **
**  long as that stays true.                                                                       **
***************************************************************************************************/
#if ACTIVE_GEARBOX == GEARBOX_DQ381

/* Real ratios - VW Group DQ381 7-speed DSG (vRS 245 / facelift diesel). Two final drives: 4.17
   for forward gears 1-5, 3.13 for 6-7 and reverse (the dual-clutch box's second output shaft) -
   stored per-gear rather than as one shared value for exactly that reason, see
   TCU_gear_ratio_cfg_st's own doc. Note 6th's own gear_ratio (0.76) is numerically taller than
   5th's (0.71) - it's the final drive switch that still makes 6th's COMBINED ratio lower than
   5th's (2.38 vs 2.96), i.e. still a genuinely taller gear overall; the raw ratio column alone is
   not monotonic here and that's correct, not a typo. */
STATIC const TCU_gear_ratio_cfg_st tcu_gear_ratios_s[] =
{
    /* gear_ratio, final_drive_ratio */
    { 3.40f, 4.17f },  /* 1st */
    { 2.75f, 4.17f },  /* 2nd */
    { 1.77f, 4.17f },  /* 3rd */
    { 0.93f, 4.17f },  /* 4th */
    { 0.71f, 4.17f },  /* 5th */
    { 0.76f, 3.13f },  /* 6th - switches to the second output shaft's final drive */
    { 0.64f, 3.13f },  /* 7th */
};

STATIC const TCU_gear_ratio_cfg_st tcu_reverse_gear_ratio_s = { 2.90f, 3.13f };

#elif ACTIVE_GEARBOX == GEARBOX_DQ250

/* Real ratios - VW Group DQ250 6-speed DSG (pre-facelift / early Mk3, wet clutch). Two final
   drives: 4.77 for forward gears 1-4, 3.44 for 5-6 and reverse (the second output shaft) - same
   per-gear storage reasoning as the DQ381 table above. 5th's own gear_ratio (0.80) is likewise
   numerically taller than 4th's (0.78), and likewise still ends up a genuinely taller gear once
   the final drive switch is combined in (2.75 vs 3.72). */
STATIC const TCU_gear_ratio_cfg_st tcu_gear_ratios_s[] =
{
    /* gear_ratio, final_drive_ratio */
    { 2.92f, 4.77f },  /* 1st */
    { 1.79f, 4.77f },  /* 2nd */
    { 1.14f, 4.77f },  /* 3rd */
    { 0.78f, 4.77f },  /* 4th */
    { 0.80f, 3.44f },  /* 5th - switches to the second output shaft's final drive */
    { 0.64f, 3.44f },  /* 6th */
};

STATIC const TCU_gear_ratio_cfg_st tcu_reverse_gear_ratio_s = { 3.26f, 3.44f };

#elif ACTIVE_GEARBOX == GEARBOX_722_9

/* Real ratios - Mercedes 7G-Tronic Plus (722.9). A conventional torque-converter automatic, not
   a dual-clutch box like the two DSGs above - one output shaft, so every gear (and reverse) uses
   the SAME final drive, unlike DQ381/DQ250's per-gear switch. */
STATIC const TCU_gear_ratio_cfg_st tcu_gear_ratios_s[] =
{
    /* gear_ratio, final_drive_ratio */
    { 4.38f, 3.07f },  /* 1st */
    { 2.86f, 3.07f },  /* 2nd */
    { 1.92f, 3.07f },  /* 3rd */
    { 1.37f, 3.07f },  /* 4th */
    { 1.00f, 3.07f },  /* 5th - direct drive */
    { 0.82f, 3.07f },  /* 6th - overdrive */
    { 0.73f, 3.07f },  /* 7th - overdrive */
};

STATIC const TCU_gear_ratio_cfg_st tcu_reverse_gear_ratio_s = { 3.42f, 3.07f };  /* normal, not Comfort/Winter - see above */

#elif ACTIVE_GEARBOX == GEARBOX_ZF_8HP

/* ZF 8HP45 8-speed torque-converter automatic (what the F30 this shifter comes from is fitted
   with). One output shaft like the 722.9, so every gear and reverse share the same final drive.
   Gear ratios are the commonly published 8HP45 set - verify against your own unit's spec sheet.
   PLACEHOLDER final drive: 3.15 is a mid-range figure, the real one varies with model/engine
   (roughly 2.8 - 3.9) - set yours, since it scales the road speed TCU_get_speed_kmh() reports and
   ENGINE_SIM's reflected inertia. */
STATIC const TCU_gear_ratio_cfg_st tcu_gear_ratios_s[] =
{
    /* gear_ratio, final_drive_ratio */
    { 4.714f, 3.15f },  /* 1st */
    { 3.143f, 3.15f },  /* 2nd */
    { 2.106f, 3.15f },  /* 3rd */
    { 1.667f, 3.15f },  /* 4th */
    { 1.285f, 3.15f },  /* 5th */
    { 1.000f, 3.15f },  /* 6th - direct drive */
    { 0.839f, 3.15f },  /* 7th - overdrive */
    { 0.667f, 3.15f },  /* 8th - overdrive */
};

STATIC const TCU_gear_ratio_cfg_st tcu_reverse_gear_ratio_s = { 3.317f, 3.15f };

#else
#error "ACTIVE_GEARBOX must be GEARBOX_DQ381, GEARBOX_DQ250, GEARBOX_722_9, or GEARBOX_ZF_8HP"
#endif

STATIC const TCU_lever_source_cfg_st tcu_lever_sources_s[] =
{
    { GKT_SHIFTER_STATE_CAN_ID, GKT_SHIFTER_decode_status_frame },
};

STATIC const TCU_periodic_output_cfg_st tcu_periodic_outputs_s[] =
{
    { APP_PDU_TCU_GEAR_STATE_TX, TCU_GEAR_STATE_DLC, TCU_GEAR_STATE_PERIOD_MS, TCU_encode_gear_state_frame },
    { APP_PDU_TCU_DYNAMICS_TX,   TCU_DYNAMICS_DLC,   TCU_DYNAMICS_PERIOD_MS,   TCU_encode_dynamics_frame },
};

const TCU_cfg_st tcu_cfg_s =
{
    .num_gears                  = (u8_t)( sizeof(tcu_gear_ratios_s) / sizeof(tcu_gear_ratios_s[0u]) ),
    .lever_sources              = tcu_lever_sources_s,
    .num_lever_sources          = (u8_t)( sizeof(tcu_lever_sources_s) / sizeof(tcu_lever_sources_s[0u]) ),
    .periodic_outputs           = tcu_periodic_outputs_s,
    .num_periodic_outputs       = (u8_t)( sizeof(tcu_periodic_outputs_s) / sizeof(tcu_periodic_outputs_s[0u]) ),
    .get_rpm_func_p             = app_read_engine_rpm,
    .auto_upshift_rpm           = TCU_AUTO_UPSHIFT_RPM,
    .auto_downshift_rpm         = TCU_AUTO_DOWNSHIFT_RPM,
    .min_auto_shift_interval_ms = TCU_MIN_AUTO_SHIFT_INTERVAL_MS,
    .gear_ratios                = tcu_gear_ratios_s,
    .reverse_gear_ratio         = &tcu_reverse_gear_ratio_s,
    .wheel_circumference_m      = TCU_WHEEL_CIRCUMFERENCE_M,
    .coast_decel_kmh_per_s      = TCU_COAST_DECEL_KMH_PER_S,
    .idle_rpm                   = ENGINE_IDLE_RPM,   /* same idle ENGINE_SIM free-revs to at 0% throttle - see
                                                         TCU_cfg_st.idle_rpm's own doc, TCU.h, on why Drive/Reverse
                                                         needs to know it */
    .get_throttle_percent_func_p = app_read_throttle_percent,   /* the very function ENGINE_SIM reads below, so the
                                                                   throttle on the bus is exactly what drives the RPM */
};

const GKT_SHIFTER_cfg_st gkt_shifter_cfg_s =
{
    .led_pdu_id         = APP_PDU_GKT_LED_TX,
    .backlight_pdu_id   = APP_PDU_GKT_BACKLIGHT_TX,
};

const MQB_CLUSTER_cfg_st mqb_cluster_cfg_s =
{
    .tx_pdu_id_base     = APP_PDU_MQB_TX_BASE,
};

/* W204 facelift dash - same RPM/speed/gear feeds as MQB_CLUSTER (see the relays at the end of file) */
const MRA_CLUSTER_cfg_st mra_cluster_cfg_s =
{
    .tx_pdu_id_base     = APP_PDU_MRA_TX_BASE,
};

/***************************************************************************************************
**                              CPS — Crank Position Sensor                                        **
**  Bench-test input: a plain square wave, 60 pulses = 1 revolution (CPS_GAP_NONE/total_teeth=60) - **
**  stands in for a real 60-2 wheel's tooth rate without the actual missing-tooth gap, since a       **
**  plain signal generator can't produce one. At this tooth count, Hz numerically ≈ RPM (60 teeth /  **
**  60 sec-per-min cancel out) - e.g. ~800 Hz for 800 RPM idle, ~8000 Hz for 8000 RPM. Swap gap_type **
**  to CPS_GAP_2_MISSING (still 60 total_teeth) once a gap-capable signal source (real sensor, or a  **
**  programmable pulse generator) is available. Pin setup + EXTI3_IRQHandler live in HAL_BRD.c,      **
**  which calls CPS_tooth_event() directly — no generic dispatch layer. Wired directly in main():    **
**  CPS_init(&cps_crank_instance_s, &cps_crank_cfg_s, SystemCoreClock), which itself runs AFTER      **
**  HAL_BRD_init() (unlike a plain GPIO peripheral, order here doesn't matter for safety - see       **
**  interrupt_enable_func_p below). Ticked via CPS_tick() from MODE_MGR. Watch                        **
**  cps_crank_instance_s.rpm live in the debugger, or call CPS_get_rpm().                            **
**  CPS_tooth_event() does not NULL/state-guard instance_p (see its own doc, CPS.c) - that's traded  **
**  for HAL_BRD_init() leaving EXTI3's NVIC line disabled (EXTI itself still armed, so a real edge    **
**  in the meantime just sets EXTI->PR and waits) and interrupt_enable_func_p below unmasking it      **
**  only once CPS_init() has fully finished - order-independent by construction, not by convention.  **
***************************************************************************************************/
CPS_instance_st cps_crank_instance_s;

const CPS_cfg_st cps_crank_cfg_s =
{
    .total_teeth                     = CPS_TOTAL_TEETH,
    .gap_type                        = CPS_GAP_NONE,   /* plain square wave, no gap to sync on */
    .capture_edge                    = CPS_EDGE_RISING,
    .filter_depth                    = CPS_RPM_FILTER_DEPTH_MAX, /* max averaging (8 samples) —
                                                 * smooths out sample-to-sample jitter at high
                                                 * input frequencies */
    .stall_timeout_us                = CPS_STALL_TIMEOUT_US,
    .rpm_max_credible                = CPS_RPM_MAX_CREDIBLE,
    .get_timer_ticks_func_p          = DWT_get_count, /* raw cycle counter — no conversion in the ISR */
    .revolution_sync_callback_func_p = NULL_P, /* never fires under CPS_GAP_NONE — no gap to find */
    .stall_callback_func_p           = NULL_P,
    .rpm_implausible_callback_func_p = NULL_P, /* CPS_RPM_MAX_CREDIBLE is unclamped above, so
                                                 * this can never actually fire — wire it up once a
                                                 * real credible ceiling is set */
    .critical_enter_func_p           = HAL_BRD_cps_crank_interrupt_disable,
    .critical_exit_func_p            = HAL_BRD_cps_crank_interrupt_enable, 
    .interrupt_enable_func_p         = HAL_BRD_cps_crank_interrupt_enable,                                       
};

/***************************************************************************************************
**                              APS — Accelerator Pedal Sensor                                     **
**  Single-channel for now: only ch1 is wired on the bench (PA0/ADC_THROTTLE_CHANNEL, HAL_config.h  **
**  - GPIO_Mode_AIN configured in HAL_BRD_init()). ch2 has no ADC-capable pin free at all right now  **
**  (see ADC_THROTTLE_CHANNEL_2's own comment, HAL_config.h) - see APS.h on ch2_read_func_p ==       **
**  NULL_P (no second channel, no plausibility check, ch1 accepted as-is). Swap in                   **
**  HAL_ADC_measure_throttle_input_2 for ch2_read_func_p once a pin is freed up and the second        **
**  track is wired to it (and give it its own GPIO_Mode_AIN in HAL_BRD_init()), and restore           **
**  ch2_ratio/plausibility_tolerance_percent (APS.h's own doc on those). Ticked from MODE_MGR. Its    **
**  only consumer in this project is ENGINE_SIM below (app_read_throttle_percent) - that's why it     **
**  lives here rather than in INTEGRATION_STUBS.c's generic peripheral wiring.                        **
***************************************************************************************************/
APS_instance_st aps_pedal_instance_s;

const APS_cfg_st aps_pedal_cfg_s =
{
    .idle_counts                    = APS_IDLE_COUNTS,
    .wot_counts                     = APS_WOT_COUNTS,
    .ema_weighting                  = APS_EMA_WEIGHTING,
    .ch1_read_func_p                = HAL_ADC_measure_throttle_input,
    .ch2_read_func_p                = NULL_P,
    .implausible_fn_p               = NULL_P,
};

/***************************************************************************************************
**                              ENGINE_SIM — Bench Engine RPM Simulator                            **
**  There's no real engine on this bench, so MQB_CLUSTER's tachometer is driven by ENGINE_SIM        **
**  instead - throttle and gear in, RPM out. ENGINE_SIM does not decide the gear itself - TCU does    **
**  (see tcu_cfg_s's automatic-gearbox fields above and TCU_get_current_gear()) - so the shift        **
**  behaviour (rev - upshift - dip - rev again to top gear, then downshift back on lift-off) is       **
**  really TCU deciding when, driven by ENGINE_SIM's RPM feedback, and ENGINE_SIM just responding     **
**  to whatever gear TCU says it's in. An implausible APS reading forces APS_get_output_percent()     **
**  to 0%, which drives ENGINE_SIM back towards idle exactly like a real lifted throttle would - no   **
**  special-casing needed here either (dormant for now since aps_pedal_cfg_s is single-channel -      **
**  see its own comment - so nothing ever goes implausible). Only RPM is relayed to the cluster       **
**  below - MQB_CLUSTER's gear-selector display is separately driven by TCU/GKT_SHIFTER's P/R/N/D +   **
**  gear state (see app_tcu_gear_state_local_relay(), Application Adapter Functions below). Road       **
**  speed is TCU's job too (TCU_get_speed_kmh()) - not relayed from here at all, since ENGINE_SIM     **
**  only produces RPM.                                                                                */
ENGINE_SIM_instance_st engine_sim_instance_s;

const ENGINE_SIM_cfg_st engine_sim_cfg_s =
{
    .idle_rpm                    = ENGINE_IDLE_RPM,
    .redline_rpm                 = ENGINE_REDLINE_RPM,
    .engine_inertia_kg_m2        = ENGINE_INERTIA_KG_M2,
    .vehicle_mass_kg             = VEHICLE_MASS_KG,
    .peak_torque_nm              = ENGINE_PEAK_TORQUE_NM,
    .shift_duration_ms           = ENGINE_SHIFT_DURATION_MS,
    .get_throttle_percent_func_p    = app_read_throttle_percent,
    .get_gear_func_p                = TCU_get_current_gear,
    .get_gear_ratio_func_p          = TCU_get_gear_combined_ratio,
    .get_wheel_circumference_func_p = TCU_get_wheel_circumference_m,  
    .get_shift_matched_rpm_func_p   = TCU_get_shift_matched_rpm,  
};

/***************************************************************************************************
**                              Application Adapter Functions                                       **
**  Small no-argument/fixed-signature wrappers binding a specific instance to a generic driver's     **
**  function-pointer config above - the "adapter closes over a fixed instance pointer" pattern.     **
**  Grouped here, after all the tuning data/cfg structs above, rather than interleaved next to      **
**  whichever struct references each one.                                                            **
***************************************************************************************************/
STATIC u16_t app_read_engine_rpm( void )
{
    return( ENGINE_SIM_get_rpm( &engine_sim_instance_s ) );
}

/* GKT_SHIFTER is the gateway for every real CAN frame the physical shifter sends - this goes to
   GKT_SHIFTER_process_rx_frame(), not straight to TCU_process_rx_frame(). GKT_SHIFTER tracks its
   own P/R/N/D from the frame and then feeds it on into TCU itself (see GKT_SHIFTER.c's own header
   comment); routing it any other way would mean two different modules both claiming to own the
   shifter's real CAN IDs, and TCU would decode it before GKT_SHIFTER had updated its own gear. */
void app_pdur_gkt_lever_rx( PDUR_pdu_id_t pdu_id, u8_t* data_p, u16_t len )
{
    (void)pdu_id;
    GKT_SHIFTER_process_rx_frame( GKT_SHIFTER_STATE_CAN_ID, data_p, (u8_t)len );
}

/* Real bus RX for the shifter's own heartbeat (GKT_SHIFTER_HEARTBEAT_CAN_ID) - same gateway as
   app_pdur_gkt_lever_rx above, feeds GKT_SHIFTER_is_heartbeat_missing() with live data. */
void app_pdur_gkt_heartbeat_rx( PDUR_pdu_id_t pdu_id, u8_t* data_p, u16_t len )
{
    (void)pdu_id;
    GKT_SHIFTER_process_rx_frame( GKT_SHIFTER_HEARTBEAT_CAN_ID, data_p, (u8_t)len );
}

/* This board runs TCU itself rather than TCU being a separate physical ECU, so TCU's own
   TCU_GEAR_STATE_CAN_ID broadcast never comes back in as an RX frame - HAL_CAN doesn't loop back
   its own transmissions, so a PDUR RX route for it would simply never fire (see
   INTEGRATION_STUBS.c's pdur_routing_table_s - there is deliberately no route for this ID).
   Delivers the same payload TCU_encode_gear_state_frame() would put on the wire straight to
   GKT_SHIFTER's and MQB_CLUSTER's normal process_rx_frame() decode path instead, skipping the
   bus round-trip entirely - not through a wrapper, since none is needed. Not STATIC: called from
   MODE_MGR.c's mode_mgr_action_schedule_normal(), right after TCU_tick() so this cycle's resolved
   gear is current. */
void app_tcu_gear_state_local_relay( void )
{
    u8_t         buf[TCU_GEAR_STATE_DLC];
    TCU_state_st state;

    TCU_get_state( &state );
    TCU_encode_gear_state_frame( &state, buf );

    GKT_SHIFTER_process_rx_frame( TCU_GEAR_STATE_CAN_ID, buf, TCU_GEAR_STATE_DLC );
    MQB_CLUSTER_process_rx_frame( TCU_GEAR_STATE_CAN_ID, buf, TCU_GEAR_STATE_DLC );
    MRA_CLUSTER_process_rx_frame( TCU_GEAR_STATE_CAN_ID, buf, TCU_GEAR_STATE_DLC );
}

/* ENGINE_SIM's get_throttle_percent_func_p - a no-argument adapter binding aps_pedal_instance_s
   above, the same "adapter closes over a fixed instance pointer" pattern used throughout this
   project. BENCH_FIXED_THROTTLE_PERCENT, if set, replaces the pedal reading - see its own comment. */
STATIC u8_t app_read_throttle_percent( void )
{
#if BENCH_FIXED_THROTTLE_PERCENT > 0u
    return( (u8_t)BENCH_FIXED_THROTTLE_PERCENT );
#else
    return( APS_get_output_percent( &aps_pedal_instance_s ) );
#endif
}

/* Not STATIC: called from MODE_MGR.c right after ENGINE_SIM_tick() so the cluster sees this
   cycle's freshly simulated RPM and road speed. Speed comes from TCU_get_speed_kmh(), not
   ENGINE_SIM - see TCU_cfg_st.gear_kmh_per_1000rpm's own doc for why that's a gearbox output
   rather than something the engine model itself should compute. */
void app_engine_sim_cluster_relay( void )
{
    MQB_CLUSTER_set_rpm( ENGINE_SIM_get_rpm( &engine_sim_instance_s ) );
    MQB_CLUSTER_set_vehicle_speed_kmh( (f32_t)TCU_get_speed_kmh() );
    MRA_CLUSTER_set_rpm( ENGINE_SIM_get_rpm( &engine_sim_instance_s ) );
    MRA_CLUSTER_set_vehicle_speed_kmh( (f32_t)TCU_get_speed_kmh() );
}

/****************************** END OF FILE *******************************************************/
