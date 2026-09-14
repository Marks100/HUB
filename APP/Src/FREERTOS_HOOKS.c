/*! \file
*   \brief      FreeRTOS application hooks required by APP/Src/FreeRTOSConfig.h
*
*   \details    Mandatory callbacks the kernel links against directly (not via a config
*               function pointer) whenever the corresponding configUSE_ or configCHECK_
*               option is enabled - FreeRTOS.h itself only declares their prototypes, the
*               application must supply the bodies. Halt-on-fault bodies here match this
*               project's existing configASSERT() idiom (FreeRTOSConfig.h) rather than
*               attempting recovery.
*/
#include "FreeRTOS.h"
#include "task.h"

/*!
****************************************************************************************************
*
*   \brief         configCHECK_FOR_STACK_OVERFLOW - called when a task's stack has overflowed
*
*   \param[in]     xTask       Handle of the offending task (unused - see note)
*   \param[in]     pcTaskName  Name of the offending task (unused - see note)
*
*   \return        none
*
*   \note          Parameters intentionally unused: by the time this fires, the offending
*                  task's own stack (and possibly pcTaskName, which lives in its TCB) may
*                  already be corrupted - halting immediately, without touching either
*                  argument, is deliberate.
*
***************************************************************************************************/
void vApplicationStackOverflowHook( TaskHandle_t xTask, char* pcTaskName )
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    for( ;; );
}

/*!
****************************************************************************************************
*
*   \brief         configUSE_IDLE_HOOK - called on every iteration of the idle task
*
*   \return        none
*
*   \warning       Must never block or call any FreeRTOS API that can block (see FreeRTOS's
*                  own documentation for vApplicationIdleHook()).
*
***************************************************************************************************/
void vApplicationIdleHook( void )
{
    /* Nothing yet - placeholder required because configUSE_IDLE_HOOK == 1 */
}

/*!
****************************************************************************************************
*
*   \brief         configUSE_TICK_HOOK - called from the RTOS tick interrupt (SysTick_Handler,
*                  xCOMMON_MODULES/Src/FREERTOS/portable/GCC/ARM_CM3/port.c)
*
*   \return        none
*
*   \warning       Runs in interrupt context on every tick - must be fast, same constraints as
*                  any other ISR in this project.
*
***************************************************************************************************/
void vApplicationTickHook( void )
{
    /* Nothing yet - placeholder required because configUSE_TICK_HOOK == 1 */
}

/****************************** END OF FILE *******************************************************/
