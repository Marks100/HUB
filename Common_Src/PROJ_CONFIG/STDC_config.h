/*! \file
*
*   \brief      STDC Configuration header
*/
/* ========================= COPYRIGHT NOTICE ======================================================
* The contents of this document are protected under copyright and contain commercially and/or
* technically confidential information. The content of this document must not be used other than
* for the purpose for which it was provided nor may it be disclosed or copied (by the authorised
* recipient or otherwise) without the prior written consent of an authorised officer of Sensata
*
*   Copyright 2016 Sensata Technologies.
*
***************************************************************************************************/
#ifndef STDC_CONFIG_H
#define STDC_CONFIG_H

/***************************************************************************************************
**                      Version Control defines                                                   **
***************************************************************************************************/

/* If update of STDC_CONFIG_VERSION is required then ensure that any changes to
   STDC_config_template.h are merged into this configuration file */
#define STDC_CONFIG_VERSION              (1u)

/***************************************************************************************************
**                      Include Section                                                           **
***************************************************************************************************/
/* None */

/*!*************************************************************************************************
*   \brief          Time in micro secs taken to execute a single loop of STDC_poll_register() loop
*   If STDC_poll_register() is used, then it needs be calibrated for this given micro
***************************************************************************************************/
#define STDC_POLL_REGISTER_NUM_US_PER_LOOP          ( 0xFFu )

/*!*************************************************************************************************
*   \brief          Allows backwards compatibility for older projects that do not use SELMATH CM
*   Typically disable for any new projects that pull in SELMATH CM
***************************************************************************************************/
#define STDC_CFG_SELMATH_BACKWARDS_COMPATIBILITY    ( 0 )

/*!*************************************************************************************************
*   \brief          The Assert strategy should be defined here, They are mutually exclusive.
***************************************************************************************************/
#define STDC_AUTOGEN_ASSERTS_ENABLED    ( 0 )
#define STDC_BASIC_ASSERT_ENABLED       ( 1 )
#define STDC_FANCY_ASSERT_ENABLED       ( 0 )
#define STDC_NO_ASSERTS_ENABLED         ( 0 )

/*!*************************************************************************************************
*   \brief          Allows standard assert functionality to be overwritten and customised
*   This means that STDC_BASIC_ASSERT_ENABLED and STDC_FANCY_ASSERT_ENABLED have no effect
***************************************************************************************************/
#define STDC_CUSTOM_ASSERT_ENABLED   0

/*!*************************************************************************************************
* \note  The following enum values are used externally e.g. by CANoe Configurations, these are used
*        by STDC_assert and typically reported by MUX, when done in this way the enum values will be
*        auto generated into MUX dbc anyway, so keeping numbering consistent across projects should
*        not be an issue
*
*        Commonly used Ids for FANCY_ASSERT, please customise this for your project
***************************************************************************************************/
typedef enum
{
    /* Low level modules */
    STDC_MOD_LLC_ADC          = 0u,
    STDC_MOD_LLC_CRG          = 1u,
    STDC_MOD_LLC_FLASH        = 2u,
    STDC_MOD_LLC_I2C          = 3u,
    STDC_MOD_LLC_IO           = 4u,
    STDC_MOD_LLC_SCI          = 5u,
    STDC_MOD_LLC_SCI_LIN      = 6u,
    STDC_MOD_LLC_SPI          = 7u,
    STDC_MOD_LLC_TIM          = 8u,
    STDC_MOD_LLC_CAN          = 9u,
    STDC_MOD_LLC_PIT          = 10u,
    /* Add new LLC Modules here */

    /* Common modules and commonly used modules */
    STDC_MOD_HAL              = 15u,
    STDC_MOD_DC_COMM          = 16u,
    STDC_MOD_MUX_COMM         = 17u,
    STDC_MOD_PTT_FILTER       = 18u,
    STDC_MOD_RF_CONFIG        = 19u,
    STDC_MOD_RF_DECODE        = 20u,
    STDC_MOD_RF_FRAME_DECODE  = 21u,
    STDC_MOD_RF_FRAME_PROCESS = 22u,
    STDC_MOD_RF_HARVEST       = 23u,
    STDC_MOD_STDC             = 24u,
    STDC_MOD_WAL_LOC          = 25u,
    STDC_MOD_WAL_LRN          = 26u,
    STDC_MOD_WAL_POP          = 27u,
    STDC_MOD_WARN_COMM        = 28u,
    STDC_MOD_EE_MGR           = 29u,
    STDC_MOD_FREC             = 30u,
    STDC_MOD_LIN_LAL_COMM     = 31u,
    STDC_MOD_LIN_DLL_COMM     = 32u,
    STDC_MOD_LIN_DIAG_COMM    = 33u,
    STDC_MOD_LIN_NWM_COMM     = 34u,
    STDC_MOD_LIN_TP_COMM      = 35u,
    STDC_MOD_RAL              = 36u,
    STDC_MOD_NVM_MGR          = 37u,
    STDC_MOD_SEL_TP           = 38u,
    STDC_MOD_SEL_KWP2000      = 39u,
    STDC_MOD_TPMS_APP         = 40u,
    STDC_MOD_WARN_APP         = 41u,
    STDC_MOD_WAL_APP          = 42u,
    STDC_MOD_FAIL_MGR         = 43u,
    STDC_MOD_HMI              = 44u,
    STDC_MOD_LEARN            = 45u,
    STDC_MOD_DTC_MGR          = 46u,
    STDC_MOD_CAL              = 47u,
    STDC_MOD_CAL_RX           = 48u,
    STDC_MOD_CAL_TX           = 49u,
    STDC_MOD_RTOS_APP         = 50u,
    STDC_MOD_FREC_APP         = 51u,
    STDC_MOD_WU_MGR           = 52u,
    STDC_MOD_PWR_MGR          = 53u,
    STDC_MOD_WDG_MGR          = 54u,
    STDC_MOD_LIN_DIAG_APP     = 55u,
    STDC_MOD_LIN_LAL_APP      = 56u,
    STDC_MOD_LIN_MAS_COMM     = 57u,
    STDC_MOD_SEL_UDS          = 58u,
    STDC_MOD_CAN_APP          = 59u,
    STDC_MOD_DIAG             = 60u,
    STDC_MOD_SELMATH          = 61u,
    STDC_MOD_SCIP_APP         = 62u,
    STDC_MOD_SCIP             = 63u,
    STDC_MOD_RTOS             = 64u,
    STDC_MOD_CAL_FRFM         = 65u,
    STDC_MOD_WIFI             = 66u,
    STDC_MOD_PAL              = 67u,
    STDC_MOD_PAL_LLA          = 68u,
    STDC_MOD_PAL_APP          = 69u,
    STDC_MOD_PAL_STAT         = 70u,
    STDC_MOD_PAL_TOC          = 71u,
    STDC_MOD_PAL_LRN          = 72u,
    STDC_MOD_PAL_LOC          = 73u,
    STDC_MOD_SECURITY         = 74u,
    STDC_MOD_AUTOSAR_RTE      = 75u,
    STDC_MOD_SSP              = 76u,
    STDC_MOD_PAL_PDS          = 77u,
    STDC_MOD_TFA              = 78u,
    STDC_MOD_RF_HARVESTER     = 79u,
    STDC_MOD_FGL              = 80u

}STDC_A_module_id_et;

#endif // #ifndef STDC_CONFIG_H
