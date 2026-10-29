/*
** STM32F103C8T6 APP partition linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x080023FF    9 KB
**   FBL  : 0x08002400 - 0x08007FFF   23 KB
**   APP  : 0x08008000 - 0x0801E7FF   90 KB  <- this image
**   DATASET0 : 0x0801E800 - 0x0801F7FF   4 KB  (field-downloadable via FBL_download_request().
**                                               Same header fields as APP but a 96 B header - no
**                                               vector table, so no 256 B VTOR alignment. BM
**                                               validates it informationally only - check bit 0
**                                               of SHARED_RAM_get_dataset_valid_mask() before
**                                               trusting its contents. Holds the tyre pressure
**                                               table - see TYRE_DATA.h.)
**     .app_header : 0x0801E800 - 0x0801E85F    96 B
**     payload     : 0x0801E860 - 0x0801F7FF  4000 B
**   NVM  : 0x0801F800 - 0x0801FFFF    2 KB  (reserved, not a linker section; fixed by
**                                             NVM_BASE_ADDRESS/NVM_TOTAL_SIZE in
**                                             Common_Src/PROJ_CONFIG/PROJ_config.h - keep in
**                                             sync. NVM_GEN2's two flip-flop partitions, shared
**                                             by FBL and APP via separate block ID ranges - see
**                                             xCOMMON_MODULES/Src/NVM_GEN2/README.md.)
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

_Min_Heap_Size  = 0x0;     /* no heap: nothing in the image allocates (no malloc/_sbrk linked) */
_Min_Stack_Size = 0x400;   /* 1 KB  */

/* DATASET0's payload - APP reads it (TYRE_DATA.c), it does not occupy it. Same names and values as
   BM's/FBL's linker scripts. *_code_end__ is inclusive (last valid byte). */
__dataset0_code_start__   = 0x0801E860;
__dataset0_code_end__     = 0x0801F7FF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08008000, LENGTH = 90K
    m_data_shared  (xrw) : ORIGIN = 0x20000000, LENGTH = 0x20
    RAM            (xrw) : ORIGIN = 0x20000020, LENGTH = 0x4FE0
}

SECTIONS
{
    /* APP header: presence pattern + CRC, padded to 256 bytes - required, not cosmetic: the
       vector table immediately below needs a 256-byte-aligned VTOR (59 exception-table entries
       on this part round up to that), see HEADER.h.
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
