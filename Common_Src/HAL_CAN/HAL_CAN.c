/*! \file
*               Author: mstewart
*   \brief      HAL_CAN — STM32F1 bxCAN1 driver, 500 kbit/s
*
*   Timing: 72 MHz APB1, prescaler=9, BS1=12tq, BS2=3tq → 16 TQ/bit → 500 kbit/s
*   Pins:   PA11=CAN_RX (IPU), PA12=CAN_TX (AF_PP)
*   RX ISR: USB_LP_CAN1_RX0_IRQn (shared with USB low-priority; USB not used here)
*/
/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "HAL_CAN.h"
#include "HAL_config.h"
#include "stm32f10x_can.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "misc.h"

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
*
*   \param         rx_callback_p  Called from the RX ISR with (id, data, dlc).
*                                 Pass NULL_P if receive is not needed.
*
***************************************************************************************************/
void HAL_CAN_init( HAL_CAN_rx_callback_ft rx_callback_p )
{
    GPIO_InitTypeDef      gpio_init;
    CAN_InitTypeDef       can_init;
    CAN_FilterInitTypeDef filter_init;
    NVIC_InitTypeDef      nvic_init;

    hal_can_rx_callback_s = rx_callback_p;

    /* CAN1 peripheral clock (APB1) */
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_CAN1, ENABLE );

    /* PA12 — CAN_TX: alternate function push-pull */
    gpio_init.GPIO_Pin   = CAN_TX_PIN;
    gpio_init.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( CAN_TX_PORT, &gpio_init );

    /* PA11 — CAN_RX: input with pull-up (bus recessive when idle) */
    gpio_init.GPIO_Pin  = CAN_RX_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init( CAN_RX_PORT, &gpio_init );

    /* bxCAN timing: 72 MHz / 9 = 8 MHz, 1+12+3 = 16 TQ → 500 kbit/s, SP = 81.25% */
    CAN_StructInit( &can_init );
    can_init.CAN_TTCM      = DISABLE;
    can_init.CAN_ABOM      = ENABLE;   /* auto-recover from bus-off */
    can_init.CAN_AWUM      = DISABLE;
    can_init.CAN_NART      = DISABLE;
    can_init.CAN_RFLM      = DISABLE;
    can_init.CAN_TXFP      = DISABLE;
    can_init.CAN_Mode      = CAN_Mode_Normal;
    can_init.CAN_SJW       = CAN_SJW_1tq;
    can_init.CAN_BS1       = CAN_BS1_12tq;
    can_init.CAN_BS2       = CAN_BS2_3tq;
    can_init.CAN_Prescaler = 9u;
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
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0u;
    nvic_init.NVIC_IRQChannelSubPriority        = 0u;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init( &nvic_init );
}

/*!
****************************************************************************************************
*
*   \brief         Transmit a standard-frame CAN message.
*
*   \param         id      11-bit CAN identifier
*   \param         data_p  Payload bytes (up to 8)
*   \param         dlc     Data length (clamped to 8 if larger)
*
*   \return        PASS if a TX mailbox was available, FAIL if all mailboxes are busy.
*
***************************************************************************************************/
pass_fail_et HAL_CAN_send_frame( u32_t id, const u8_t* data_p, u8_t dlc )
{
    pass_fail_et result = FAIL;
    CanTxMsg     tx_msg;
    u8_t         i;
    u8_t         mailbox;

    tx_msg.StdId = id & 0x7FFu;
    tx_msg.IDE   = CAN_ID_STD;
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

    CAN_Receive( CAN1, CAN_FIFO0, &rx_msg );

    if( hal_can_rx_callback_s != NULL_P )
    {
        hal_can_rx_callback_s( rx_msg.StdId, rx_msg.Data, (u8_t)rx_msg.DLC );
    }
}

/****************************** END OF FILE *******************************************************/
