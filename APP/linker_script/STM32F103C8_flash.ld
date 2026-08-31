/*
** STM32F103C8T6 APP partition linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x08002BFF   11 KB
**   FBL  : 0x08002C00 - 0x08007FFF   21 KB  (grown twice from 17 KB to fit the NVM driver - FBL
**                                            persists a WriteDataByIdentifier fingerprint. See
**                                            that image's own linker script for the history. If
**                                            FBL needs more still, this image has slack to fund it.)
**   APP  : 0x08008000 - 0x0801F7FF   94 KB  <- this image (shrunk from 99 KB across two FBL/NVM
**                                             growths; this project had ~54 KB of flash slack, so
**                                             the room came from here rather than BM's earmarked
**                                             headroom)
**   NVM  : 0x0801F800 - 0x0801FFFF    2 KB  (reserved, not a linker section here; placed
**                                             dynamically by FLS_STM32F1_get_nvm_base_address()
**                                             at the top two pages of actual flash size. This
**                                             image must not grow into it. Both pages belong to
**                                             NVM_GEN2 as its two flip-flop partitions, and BOTH
**                                             images share both - FBL and APP no longer take a
**                                             page each. Blocks are packed into an append-only
**                                             log keyed by block ID, the images own separate ID
**                                             ranges, and compaction copies records it has no
**                                             config for across verbatim, so neither image can
**                                             overwrite the other. See xCOMMON_MODULES/Src/
**                                             NVM_GEN2/README.md.)
**
** APP internal layout:
**   .app_header : 0x08008000 - 0x080080FF  256 B  (presence pattern + CRC)
**   .isr_vector : 0x08008100 ...            code start (APP_CODE_START)
**
** RAM layout (20 KB total - fixed regardless of the 64/128 KB flash bin on this part family):
**   .noinit_shared : 0x20000000 - 0x2000001F   32 B  (BM/FBL/APP handoff - see SHARED_RAM.h)
**   everything else: 0x20000020 - 0x20004FFF
*/

ENTRY(Reset_Handler)

_estack = 0x20005000;   /* top of 20 KB SRAM - same as BM/FBL, see SHARED_RAM.h's fixed layout */

_Min_Heap_Size  = 0x200;   /* 512 B */
_Min_Stack_Size = 0x400;   /* 1 KB  */

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08008000, LENGTH = 94K
    m_data_shared  (xrw) : ORIGIN = 0x20000000, LENGTH = 0x20
    RAM            (xrw) : ORIGIN = 0x20000020, LENGTH = 0x4FE0
}

SECTIONS
{
    /* APP header: presence pattern + CRC, padded to 256 bytes.
       The CRC field at offset +4 is patched by app_crc_injector. */
    .app_header :
    {
        . = ALIGN(4);
        KEEP(*(.app_header))
        . = ALIGN(0x100);   /* pad to 256-byte boundary -> next addr = 0x08008100 */
    } >FLASH

    /* Vector table at APP_CODE_START = 0x08008100 */
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
       by startup the way .bss is - see SHARED_RAM.h. Not previously present in this script;
       added so APP can call SHARED_RAM_set_fbl_request() to request FBL entry on next boot. */
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
