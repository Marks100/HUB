/*! \file
*               Author: mstewart
*   \brief      HAL_CAN — STM32F1 bxCAN1 driver, 500 kbit/s
*
*   Timing: 36 MHz APB1 (72 MHz SYSCLK halved by CLK_STM32F1 - see CLK_STM32F1_APB1_MAX_HZ),
*           prescaler=4, BS1=14tq, BS2=3tq → 18 TQ/bit → 500 kbit/s
*   Pins:   PA11=CAN_RX (IPU), PA12=CAN_TX (AF_PP)
*   RX ISR: USB_LP_CAN1_RX0_IRQn (shared with USB low-priority; USB not used here)
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_CAN.h"

/***************************************************************************************************
**                              Data                                                              **
***************************************************************************************************/
STATIC HAL_CAN_rx_callback_ft hal_can_rx_callback_s = NULL_P;

/***************************************************************************************************
**                              Public Functions                                                  **
***************************************************************************************************/
/*!
****************************************************************************************************
*
*   \brief         Initialise bxCAN1 at 500 kbit/s and configure an accept-all filter.
*                  No RX callback is registered - call HAL_CAN_set_rx_callback() separately if
*                  receive is needed, same pattern as HAL_USART2_set_rx_callback().
*
***************************************************************************************************/
void HAL_CAN_init( void )
{
    GPIO_InitTypeDef      gpio_init;
    CAN_InitTypeDef       can_init;
    CAN_FilterInitTypeDef filter_init;
    NVIC_InitTypeDef      nvic_init;

    /* CAN1 peripheral clock (APB1) */
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_CAN1, ENABLE );

    /* GPIOA clock (APB2) - CAN_TX_PORT/CAN_RX_PORT are both GPIOA (see HAL_config.h). Enabled
       here rather than left to the caller: APP happens to already enable it via HAL_BRD_init()
       before HAL_CAN_init() runs, but FBL calls this directly with no such prior step - without
       this, GPIO_Init() below writes to a clock-gated peripheral and silently has no effect,
       leaving PA11/PA12 as floating inputs instead of CAN_RX/CAN_TX. Idempotent/harmless if
       already enabled. */
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE );

    /* PA12 — CAN_TX: alternate function push-pull */
    gpio_init.GPIO_Pin   = CAN_TX_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( CAN_TX_PORT, &gpio_init );

    /* PA11 — CAN_RX: input with pull-up (bus recessive when idle) */
    gpio_init.GPIO_Pin  = CAN_RX_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init( CAN_RX_PORT, &gpio_init );

    /* bxCAN timing: APB1 is 36 MHz here (not 72 - see file header), /4 = 9 MHz TQ clock,
       1+14+3 = 18 TQ → 500 kbit/s, SP = 15/18 = 83.3% */
    CAN_StructInit( &can_init );
    can_init.CAN_TTCM      = DISABLE;
    can_init.CAN_ABOM      = ENABLE;   /* auto-recover from bus-off */
    can_init.CAN_AWUM      = DISABLE;
    can_init.CAN_NART      = ENABLE;   /* single-shot TX - callers already re-send (MSG_SCHED period, UDS req/resp), don't let one stuck ACK starve a mailbox */
    can_init.CAN_RFLM      = DISABLE;
    can_init.CAN_TXFP      = DISABLE;
    can_init.CAN_Mode      = CAN_Mode_Normal;
    can_init.CAN_SJW       = CAN_SJW_1tq;
    can_init.CAN_BS1       = CAN_BS1_14tq;
    can_init.CAN_BS2       = CAN_BS2_3tq;
    can_init.CAN_Prescaler = 4u;
    CAN_Init( CAN1, &can_init );

    /* Filter bank 0 — accept all standard frames */
    filter_init.CAN_FilterNumber         = 0u;
    filter_init.CAN_FilterMode           = CAN_FilterMode_IdMask;
    filter_init.CAN_FilterScale          = CAN_FilterScale_32bit;
    filter_init.CAN_FilterIdHigh         = 0x0000u;
    filter_init.CAN_FilterIdLow          = 0x0000u;
    filter_init.CAN_FilterMaskIdHigh     = 0x0000u;
    filter_init.CAN_FilterMaskIdLow      = 0x0000u;
    filter_init.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter_init.CAN_FilterActivation     = ENABLE;
    CAN_FilterInit( &filter_init );

    /* NVIC: enable FIFO0 message-pending interrupt */
    CAN_ITConfig( CAN1, CAN_IT_FMP0, ENABLE );

    nvic_init.NVIC_IRQChannel                   = USB_LP_CAN1_RX0_IRQn;
    /* Priority 1 — priority 0 is reserved for the board's highest-priority input ISR, see
       HAL_BRD_init()'s priority scheme comment */
    nvic_init.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic_init.NVIC_IRQChannelSubPriority        = 0u;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init( &nvic_init );
}

/*!
****************************************************************************************************
*
*   \brief         Register (or clear) the RX callback, independent of HAL_CAN_init().
*
*   \param         rx_callback_p  Called from the RX ISR with (id, data, dlc). Pass NULL_P to
*                                 stop receiving.
*
***************************************************************************************************/
void HAL_CAN_set_rx_callback( HAL_CAN_rx_callback_ft rx_callback_p )
{
    hal_can_rx_callback_s = rx_callback_p;
}

/*!
****************************************************************************************************
*
*   \brief         Transmit a CAN message, standard or extended.
*
*   \param         id       11-bit (standard) or 29-bit (extended) CAN identifier
*   \param         id_type  0 = Standard (11-bit), 1 = Extended (29-bit)
*   \param         data_p   Payload bytes (up to 8)
*   \param         dlc      Data length (clamped to 8 if larger)
*
*   \return        PASS if a TX mailbox was available, FAIL if all mailboxes are busy.
*
***************************************************************************************************/
pass_fail_et HAL_CAN_send_frame( u32_t id, u8_t id_type, const u8_t* data_p, u8_t dlc )
{
    pass_fail_et result = FAIL;
    CanTxMsg     tx_msg;
    u8_t         i;
    u8_t         mailbox;

    if( id_type == 0u )
    {
        tx_msg.StdId = id & 0x7FFu;
        tx_msg.IDE   = CAN_ID_STD;
    }
    else
    {
        tx_msg.ExtId = id & 0x1FFFFFFFu;
        tx_msg.IDE   = CAN_ID_EXT;
    }
    tx_msg.RTR   = CAN_RTR_DATA;
    tx_msg.DLC   = ( dlc <= 8u ) ? dlc : 8u;

    for( i = 0u; i < tx_msg.DLC; i++ )
    {
        tx_msg.Data[i] = data_p[i];
    }

    mailbox = CAN_Transmit( CAN1, &tx_msg );

    if( mailbox != CAN_TxStatus_NoMailBox )
    {
        result = PASS;
    }

    return( result );
}

/***************************************************************************************************
**                              ISR                                                               **
***************************************************************************************************/
void USB_LP_CAN1_RX0_IRQHandler( void )
{
    CanRxMsg rx_msg;
    u32_t    rx_id;
    u8_t     rx_id_type;

    CAN_Receive( CAN1, CAN_FIFO0, &rx_msg );

    if( rx_msg.IDE == CAN_ID_STD )
    {
        rx_id      = rx_msg.StdId;
        rx_id_type = 0u;
    }
    else
    {
        rx_id      = rx_msg.ExtId;
        rx_id_type = 1u;
    }

    if( hal_can_rx_callback_s != NULL_P )
    {
        hal_can_rx_callback_s( rx_id, rx_id_type, rx_msg.Data, (u8_t)rx_msg.DLC );
    }
}

/****************************** END OF FILE *******************************************************/
