/*
** STM32F103C8T6 Boot Manager linker script
**
** Flash layout (64 KB total):
**   BM   : 0x08000000 - 0x08000FFF   4 KB  <- this image
**   FBL  : 0x08001000 - 0x08004FFF  16 KB
**   APP  : 0x08005000 - 0x0800FFFF  44 KB
**     .app_header : 0x08005000 - 0x080050FF  256 B
**     code start  : 0x08005100               (APP_CODE_START)
**
** RAM layout (20 KB total):
**   .noinit_shared : 0x20000000 - 0x2000001F   32 B  (BM/FBL/APP handoff - see SHARED_RAM.h)
**   everything else: 0x20000020 - 0x20004FFF
*/

ENTRY(BM_Reset_Handler)

_estack = 0x20005000;   /* top of 20 KB SRAM */

_Min_Heap_Size  = 0x0;     /* BM does not use a heap */
_Min_Stack_Size = 0x400;   /* 1 KB */

/* Flash addresses BM validates but does not itself occupy any section in - just constants
   MAIN.c's bm_config_s is built from. Must match APP/Makefile's APP_HEADER_ADDR/APP_CODE_START/
   APP_CODE_END (BM_FLASH_START + BM_FLASH_SIZE + FBL_FLASH_SIZE = 0x08005000) and
   xCOMMON_MODULES/Src/HEADER/HEADER.h's 256-byte header (code start = header + 0x100). */
__bm_start__         = 0x08000000;
__fbl_start__        = 0x08001000;
__app_start__         = 0x08005000;
__app_header_start__  = 0x08005000;
__app_code_start__    = 0x08005100;
__app_code_end__      = 0x0800FFFF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08000000, LENGTH = 4K
    m_data_shared  (xrw) : ORIGIN = 0x20000000, LENGTH = 0x20
    RAM            (xrw) : ORIGIN = 0x20000020, LENGTH = 0x4FE0
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
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

    /* BM/FBL/APP handoff - NOLOAD, must survive a soft reset (BM jumping to APP and back), so
       it is deliberately not zeroed by startup the way .bss is - see SHARED_RAM.h. */
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
