/*
** STM32F103C8T6 Field Bootloader (FBL) linker script
**
** Flash layout (64 KB total):
**   BM   : 0x08000000 - 0x08001FFF   8 KB
**   FBL  : 0x08002000 - 0x08004FFF  12 KB  <- this image
**     .app_header : 0x08002000 - 0x080020FF  256 B  (presence pattern + CRC, see HEADER.h -
**                                                     BM validates this before jumping here,
**                                                     same tooling/layout as APP's header)
**     .isr_vector : 0x08002100 ...            code start
**   APP  : 0x08005000 - 0x0800FFFF  44 KB
**
** RAM layout (20 KB total):
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
   fbl_config_s is built from. Must match BM's/APP's __app_start__/__app_code_end__. */
__app_start__     = 0x08005000;
__app_code_end__  = 0x0800FFFF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08002000, LENGTH = 12K
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
        . = ALIGN(0x100);   /* pad to 256-byte boundary -> next addr = 0x08002100 */
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
