/*
** STM32F103C8T6 Boot Manager linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x080023FF    9 KB  <- this image
**   FBL  : 0x08002400 - 0x08007FFF   23 KB
**     .app_header : 0x08002400 - 0x080024FF  256 B
**     code start  : 0x08002500               (FBL_CODE_START)
**   APP  : 0x08008000 - 0x0801E7FF   90 KB
**     .app_header : 0x08008000 - 0x080080FF  256 B
**     code start  : 0x08008100               (APP_CODE_START)
**   DATASET0 : 0x0801E800 - 0x0801F7FF   4 KB  (field-downloadable via FBL_download_request().
**                                               Same header fields as APP but a 96 B header - see
**                                               the region constants comment below. BM validates
**                                               it informationally only - see BM_run()'s dataset
**                                               step.)
**     .app_header : 0x0801E800 - 0x0801E85F    96 B
**     payload     : 0x0801E860 - 0x0801F7FF  4000 B
**   NVM  : 0x0801F800 - 0x0801FFFF    2 KB  (reserved, not a linker section; fixed by
**                                             NVM_BASE_ADDRESS/NVM_TOTAL_SIZE in
**                                             Common_Src/PROJ_CONFIG/PROJ_config.h - keep in
**                                             sync. NVM_GEN2's two flip-flop partitions (A at
**                                             0x0801F800, B at 0x0801FC00), shared by FBL and APP
**                                             via separate block ID ranges. BM links none of
**                                             this. See xCOMMON_MODULES/Src/NVM_GEN2/README.md.)
**
** RAM layout (20 KB total - fixed regardless of the 64/128 KB flash bin on this part family):
**   .noinit_shared : 0x20000000 - 0x2000001F   32 B  (BM/FBL/APP handoff - see SHARED_RAM.h)
**   everything else: 0x20000020 - 0x20004FFF
*/

ENTRY(Reset_Handler)

_estack = 0x20005000;   /* top of 20 KB SRAM */

_Min_Heap_Size  = 0x0;     /* BM does not use a heap */
_Min_Stack_Size = 0x400;   /* 1 KB */

/* Flash regions BM validates but does not occupy - constants MAIN.c's bm_config_s is built
   from. Each *_code_end__ MUST exactly equal the matching Makefile value (APP_CODE_END,
   FBL_CODE_END, DATASET0_FLASH_END) - that's the range app_crc_injector/
   app_signer sign over, so a mismatch fails validation of every correctly-signed image. FBL's
   linker script defines the APP/dataset symbols with the same names and values.

   Convention (shared by BM.h, FBL.h and the Makefiles): *_header_start__ is the first byte of
   a region, *_code_start__ the first byte of code, *_code_end__ the LAST VALID BYTE
   (inclusive) - bm_code_length()/FBL_calc_region_length() add +1 to get a byte count.

   Header size differs by region: APP/FBL headers are 256 B (code at header+0x100) because
   their vector tables need a 256-byte-aligned VTOR (see HEADER.h). Datasets have no vector
   table, so their header is 96 B (code at header+0x60): 76 B (0x4C) used by app_crc_injector/
   app_signer's hardcoded offsets, +20 B reserved for future fields. The dataset hex is built by
   APP/Makefile's `dataset` target with these addresses - no image compiles a dataset
   .app_header section. */
__fbl_header_start__  = 0x08002400;
__fbl_code_start__    = 0x08002500;
__fbl_code_end__      = 0x08007FFF;
__app_header_start__  = 0x08008000;
__app_code_start__    = 0x08008100;
__app_code_end__      = 0x0801E7FF;
__dataset0_header_start__ = 0x0801E800;
__dataset0_code_start__   = 0x0801E860;
__dataset0_code_end__     = 0x0801F7FF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08000000, LENGTH = 9K
    m_data_shared  (xrw) : ORIGIN = 0x20000000, LENGTH = 0x20
    RAM            (xrw) : ORIGIN = 0x20000020, LENGTH = 0x4FE0
}

SECTIONS
{
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
