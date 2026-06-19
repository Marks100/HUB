/* BM (Boot Manager) - Minimal jump loader at 0x08000000.
**
** Jumps to APP via MCU_JUMP_to_address() which handles: interrupt disable,
** SysTick stop, VTOR update, barriers, MSP reload, then branch.
**
** Jump target: APP partition (0x08005000) + 256-byte app_header = 0x08005100
** To target FBL instead, change APP_VEC_ORIGIN to 0x08001000.
*/

#include <stdint.h>
#include "MCU_JUMP.h"

#define APP_VEC_ORIGIN  0x08005100UL

void BM_Reset_Handler( void );

__attribute__((section(".isr_vector"), used))
const uint32_t bm_vector_table[2] = {
    0x20005000UL,               /* Initial SP: top of 20 KB SRAM */
    (uint32_t)BM_Reset_Handler,
};

void BM_Reset_Handler( void )
{
    MCU_JUMP_to_address( APP_VEC_ORIGIN );
}
