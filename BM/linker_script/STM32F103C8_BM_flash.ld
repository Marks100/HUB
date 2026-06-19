/*
** STM32F103C8T6 Boot Manager linker script
**
** Flash layout (64 KB total):
**   BM   : 0x08000000 - 0x08000FFF   4 KB  <- this image
**   FBL  : 0x08001000 - 0x08004FFF  16 KB
**   APP  : 0x08005000 - 0x0800FFFF  44 KB
*/

ENTRY(BM_Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 4K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
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
        . = ALIGN(4);
    } >FLASH

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
