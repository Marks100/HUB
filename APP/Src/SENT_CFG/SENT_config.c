/*! \file
*               Author: mstewart
*   \brief      SENT_CFG - bench SENT sensor on TIM1 and PB12
*
*   \details    Wires SENT_TX to this board and sends a sawtooth on the 12-bit value, so a SENT
*               receiver on the bench has a moving signal to decode. Switched by
*               SENT_TX_BENCH_ENABLED in PROJ_config.h.
*
*                 TIM1 at 3 us per count, free-running. Each update interrupt:
*                   TIM1_UP_IRQHandler -> sent_cfg_timer_cb() -> SENT_TX_timer_isr()
*                 which writes PB12 through HAL_BRD_SENT_TX_set_pin() and the next phase length
*                 through HAL_TIM1_set_reload() (reload only, the counter is never zeroed).
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "SENT_config.h"
#include "PROJ_config.h"
#include "HAL_TIM.h"
#include "HAL_BRD.h"
#include "SENT_TX.h"
#include "SENT_TX_TIM_UPDATE_ISR.h"

/***************************************************************************************************
**                              Defines                                                           **
***************************************************************************************************/
#define SENT_CFG_TIM1_PRESCALER     ( 215u )        /* 72 MHz / 216 = 333.33 kHz, 3.000 us per count */
#define SENT_CFG_RAMP_STEP          ( 16u )         /* per 20 ms call, a full sweep in about 5 s     */
#define SENT_CFG_VALUE_MAX          ( 0x0FFFu )

/***************************************************************************************************
**                              Private Function Prototypes                                       **
***************************************************************************************************/
STATIC void sent_cfg_timer_cb( void );

/***************************************************************************************************
**                              Data declarations and definitions                                 **
***************************************************************************************************/
STATIC const SENT_TX_cfg_st sent_tx_cfg_s =
{
	.data_nibble_count     = 3u,
	.crc_seed              = SENT_TX_CRC_SEED_LEGACY,
	.crc_includes_status   = FALSE,

	.counts_per_tick       = 1u,
	.low_ticks             = 5u,
	.frame_ticks           = 300u,      /* constant 900 us frame, the pause pulse takes up the slack */

	.drive_p               = &SENT_TX_TIM_UPDATE_ISR_drive,

	.tim_update_isr =
	{
		.set_pin_fn_p      = HAL_BRD_SENT_TX_set_pin,
		.set_period_fn_p   = HAL_TIM1_set_reload,
		.timer_start_fn_p  = HAL_TIM1_start_periodic,
		.timer_stop_fn_p   = HAL_TIM1_stop
	}
};

STATIC SENT_TX_instance_st sent_tx_instance_s;
STATIC u16_t               sent_cfg_value_s;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         Set up TIM1 and the SENT transmitter, without starting it
*
*   \author        mstewart
*
*   \return        none
*
*   \note          The timer's NVIC channel stays off until SENT_CFG_start(), so call this before
*                  interrupts are enabled. HAL_BRD_init() has already made PB12 an output, high.
*
***************************************************************************************************/
void SENT_CFG_init( void )
{
#if ( SENT_TX_BENCH_ENABLED == 1u )
	HAL_TIM1_init();
	HAL_TIM1_set_prescaler( SENT_CFG_TIM1_PRESCALER );
	HAL_TIM1_register_callback( sent_cfg_timer_cb );

	SENT_TX_init( &sent_tx_instance_s, &sent_tx_cfg_s );
#endif
}

/*!
****************************************************************************************************
*
*   \brief         Start sending frames
*
*   \author        mstewart
*
*   \return        none
*
*   \note          Call once interrupts are enabled, the first timer interrupt follows one tick later.
*
***************************************************************************************************/
void SENT_CFG_start( void )
{
#if ( SENT_TX_BENCH_ENABLED == 1u )
	(void)SENT_TX_start( &sent_tx_instance_s );
#endif
}

/*!
****************************************************************************************************
*
*   \brief         Step the sawtooth and hand the next frame to the transmitter - call every 20 ms
*
*   \author        mstewart
*
*   \return        none
*
***************************************************************************************************/
void SENT_CFG_tick( void )
{
#if ( SENT_TX_BENCH_ENABLED == 1u )
	u8_t nibbles[ 3 ];

	sent_cfg_value_s = (u16_t)( ( sent_cfg_value_s + SENT_CFG_RAMP_STEP ) & SENT_CFG_VALUE_MAX );

	nibbles[ 0 ] = (u8_t)( ( sent_cfg_value_s >> 8u ) & 0x0Fu );
	nibbles[ 1 ] = (u8_t)( ( sent_cfg_value_s >> 4u ) & 0x0Fu );
	nibbles[ 2 ] = (u8_t)( sent_cfg_value_s & 0x0Fu );

	(void)SENT_TX_set_frame( &sent_tx_instance_s, 0u, nibbles );
#endif
}

/***************************************************************************************************
**                              Private Functions                                                 **
***************************************************************************************************/

/*!
****************************************************************************************************
*
*   \brief         TIM1 update callback - hands the interrupt to the transmitter
*
*   \author        mstewart
*
*   \return        none
*
***************************************************************************************************/
STATIC void sent_cfg_timer_cb( void )
{
	SENT_TX_timer_isr( &sent_tx_instance_s );
}

/****************************** END OF FILE *******************************************************/
