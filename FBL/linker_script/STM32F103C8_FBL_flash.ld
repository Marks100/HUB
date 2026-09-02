/*
** STM32F103C8T6 Field Bootloader (FBL) linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x080027FF   10 KB
**   FBL  : 0x08002800 - 0x08007FFF   22 KB  <- this image (grown three times from 17 KB - the
**                                             first two to fit the NVM driver - FBL persists a
**                                             WriteDataByIdentifier fingerprint, see
**                                             fbl_nvm_gen2_hw_interface_s in this project's own
**                                             INT_STUBS/INTEGRATION_STUBS.c. 17->19 KB was NVM.c's
**                                             fixed page; 19->21 KB was NVM_GEN2, which had filled
**                                             the -Og debug build to 100%. Both taken from APP's
**                                             flash budget, not BM's. The third growth, 21->22 KB,
**                                             came out of BM's own free space instead (see
**                                             BM's own linker script) once a UDS 0x22
**                                             ReadDataByIdentifier boot-version DID left this
**                                             build with none of its own margin left.)
**     .app_header : 0x08002800 - 0x080028FF  256 B  (presence pattern + CRC, see HEADER.h -
**                                                     BM validates this before jumping here,
**                                                     same tooling/layout as APP's header)
**     .isr_vector : 0x08002900 ...            code start
**   APP  : 0x08008000 - 0x0801F7FF   94 KB
**   NVM  : 0x0801F800 - 0x0801FFFF    2 KB  (reserved, not a linker section here; fixed by
**                                             NVM_BASE_ADDRESS/NVM_TOTAL_SIZE in
**                                             Common_Src/PROJ_CONFIG/PROJ_config.h - keep this
**                                             comment in sync with those if either changes. Nothing
**                                             above may claim these pages. Both pages belong to
**                                             NVM_GEN2 as its two flip-flop partitions, and BOTH
**                                             images share both - FBL and APP no longer take a
**                                             page each. Blocks are packed into an append-only
**                                             log keyed by block ID, the images own separate ID
**                                             ranges, and compaction copies records it has no
**                                             config for across verbatim, so neither image can
**                                             overwrite the other. See xCOMMON_MODULES/Src/
**                                             NVM_GEN2/README.md.)
**
** RAM layout (20 KB total - fixed regardless of the 64/128 KB flash bin on this part family):
**   .noinit_shared : 0x20000000 - 0x2000001F   32 B  (BM/FBL/APP handoff - see SHARED_RAM.h)
**   everything else: 0x20000020 - 0x20004FFF
**
** Must stay numerically consistent with BM/linker_script/STM32F103C8_BM_flash.ld and
** APP/linker_script/STM32F103C8_flash.ld / APP/Makefile's FBL_FLASH_START/SIZE - all three
** encode the same three-way partition.
*/

ENTRY(Reset_Handler)

_estack = 0x20005000;   /* top of 20 KB SRAM - same as BM/APP, so a soft reset between stages
                            never needs to reinitialise the stack pointer differently */

_Min_Heap_Size  = 0x0;
_Min_Stack_Size = 0x400;   /* 1 KB */

/* App region FBL writes into - not occupied by any section here, just constants fbl_main.c's
   fbl_config_s is built from. Must match BM's __app_header_start__/__app_code_end__ (BM/
   linker_script/STM32F103C8_BM_flash.ld) - same names, same values, both describing the
   identical physical addresses. __app_code_end__ is inclusive (last valid byte). */
__app_header_start__  = 0x08008000;
__app_code_end__      = 0x0801F7FF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08002800, LENGTH = 22K
    m_data_shared  (xrw) : ORIGIN = 0x20000000, LENGTH = 0x20
    RAM            (xrw) : ORIGIN = 0x20000020, LENGTH = 0x4FE0
}

SECTIONS
{
    /* FBL header: presence pattern + CRC, padded to 256 bytes - same layout/tooling as APP's,
       see xCOMMON_MODULES/Src/HEADER/HEADER.h. The CRC field is patched by app_crc_injector,
       BM validates it before jumping here (see BM.c's BM_jump_to_fbl()). */
    .app_header :
    {
        . = ALIGN(4);
        KEEP(*(.app_header))
        . = ALIGN(0x100);   /* pad to 256-byte boundary -> next addr = 0x08002900 */
    } >FLASH

    .isr_vector :
    {
        . = ALIGN(4);
        __isr_vector_start = .;   /* Read by Reset_Handler to self-relocate VTOR - see
                                     MCU_JUMP_set_vector_table() */
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* BM/FBL/APP handoff - NOLOAD, must survive a soft reset, so it is deliberately not zeroed
       by startup the way .bss is - see SHARED_RAM.h. */
    .noinit_shared (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.noinit_shared))
        . = ALIGN(4);
    } >m_data_shared

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    . = ALIGN(4);
    .bss :
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    ._user_heap_stack :
    {
        . = ALIGN(4);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(4);
    } >RAM

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
