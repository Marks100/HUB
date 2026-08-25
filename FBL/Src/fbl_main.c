/*! \file
*               Author: mstewart
*   \brief      FBL (Field Bootloader) entry point for STM32F103C8
*
*   Board wiring - including UDS bring-up (fbl_config_st's uds_init) - lives in
*   INT_STUBS/INTEGRATION_STUBS.c, same split as APP/Src/MAIN/main.c vs APP/Src/INT_STUBS/. This
*   file only sequences the boot: point VTOR at FBL's own vector table, bring up timekeeping, hand
*   off to FBL_init()/FBL_run().
*/

/***************************************************************************************************
**                              Includes                                                          **
***************************************************************************************************/
#include "FBL.h"
#include "TIME_MGR.h"
#include "MCU_JUMP.h"
#include "nvic_driver.h"
#include "INTEGRATION_STUBS.h"

/***************************************************************************************************
**                              Entry Point                                                      **
***************************************************************************************************/
/* Same fbl_main()/main() split as BM's bm_main() and APP's app_main(). The vector table,
   Reset_Handler (.data/.bss init) and SystemInit() all come from startup_stm32f10x_md.c, unmodified
   - FBL needs the real table because it enables SysTick and CAN RX interrupts, and the hand-rolled
   2-entry table this used to carry had no vectors for either (both handlers were defined but
   unreachable, and --gc-sections stripped them). With the vendor table, those strong symbols
   override its weak per-IRQ aliases automatically. */
extern u32_t __isr_vector_start;   /* FBL/linker_script/STM32F103C8_FBL_flash.ld */

void fbl_main( void )
{
    /* Points VTOR at FBL's own vector table, read from the linker symbol. */
    MCU_JUMP_set_vector_table( (u32_t)&__isr_vector_start );

    TIME_init( &time_cfg_s );

    FBL_init( &fbl_config_s );

    /* BM's MCU_JUMP_to_address() set PRIMASK before jumping here, masking every interrupt at the
       core regardless of NVIC state - FBL_init() enabled SysTick and CAN RX above, but neither
       fires until this clears it. APP does the same after its own init, for the same reason. */
    NVIC_EnableGlobalIRQ();

    FBL_run();
}

void main( void )
{
    fbl_main();
}

/****************************** END OF FILE *******************************************************/
