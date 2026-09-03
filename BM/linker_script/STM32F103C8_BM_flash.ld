/*
** STM32F103C8T6 Boot Manager linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x080027FF   10 KB  <- this image (grown from 8 KB to 12 KB, then given
**                                             back 1 KB twice: 12->11 KB when FBL first ran out
**                                             of margin, and 11->10 KB when FBL's -Og debug build
**                                             hit 96.9% after gaining a UDS 0x22
**                                             ReadDataByIdentifier boot-version DID. BM still has
**                                             ~6 KB free)
**   FBL  : 0x08002800 - 0x08007FFF   22 KB  (grown three times from 17 KB - the first two to fit
**                                            the NVM driver - FBL persists a WriteDataByIdentifier
**                                            fingerprint (see fbl_nvm_gen2_hw_interface_s in
**                                            FBL/Src/INT_STUBS/INTEGRATION_STUBS.c). 17->19 KB
**                                            was NVM.c's fixed page; 19->21 KB was NVM_GEN2, which
**                                            had filled the -Og debug build to 100%. Both of those
**                                            came out of APP's flash budget, not BM's. The third
**                                            growth, 21->22 KB, came out of BM's own free space
**                                            instead, once FBL's build had none of its own margin
**                                            left)
**     .app_header : 0x08002800 - 0x080028FF  256 B
**     code start  : 0x08002900               (FBL_CODE_START)
**   APP  : 0x08008000 - 0x0801EFFF   92 KB  (shrunk from 94 KB to make room for DATASET0/1 below)
**     .app_header : 0x08008000 - 0x080080FF  256 B
**     code start  : 0x08008100               (APP_CODE_START)
**   DATASET0 : 0x0801F000 - 0x0801F3FF   1 KB  (independently field-downloadable via FBL - see
**                                               FBL_download_request(). Same header/CRC/signature
**                                               FIELDS as APP (presence_pattern/crc/code_size/
**                                               signature, see app_signer/app_crc_injector's
**                                               hardcoded 0x08/0x0C offsets) but only a 96 B
**                                               header - no vector table here, so none of
**                                               APP/FBL's 256-byte VTOR-alignment requirement
**                                               applies (see this file's __dataset0_code_start__
**                                               comment above); 76 B (0x4C) is every byte the
**                                               tooling actually reads/writes, +20 B reserved
**                                               headroom for future fields (e.g. a version byte)
**                                               without having to move code_start again. Validated
**                                               by BM the same 3-stage way but informationally
**                                               only - see BM_run()'s dataset step. Carved out of
**                                               APP's own budget above.)
**     .app_header : 0x0801F000 - 0x0801F05F  96 B
**     payload     : 0x0801F060 - 0x0801F3FF  928 B
**   DATASET1 : 0x0801F400 - 0x0801F7FF   1 KB  (same as DATASET0, a second independent slot)
**     .app_header : 0x0801F400 - 0x0801F45F  96 B
**     payload     : 0x0801F460 - 0x0801F7FF  928 B
**   NVM  : 0x0801F800 - 0x0801FFFF   2 KB  (reserved, not a linker section here; fixed by
**                                            NVM_BASE_ADDRESS/NVM_TOTAL_SIZE in
**                                            Common_Src/PROJ_CONFIG/PROJ_config.h - keep this
**                                            comment in sync with those if either changes. Nothing
**                                            above may claim these pages. Both are NVM_GEN2's two
**                                            flip-flop partitions -
**                                            partition A at 0x0801F800, partition B at 0x0801FC00 -
**                                            and FBL and APP SHARE both rather than taking a page
**                                            each. Blocks are packed into an append-only log keyed
**                                            by block ID, the two images own separate ID ranges
**                                            (NVM_GEN2_BLOCK_ID_FBL_* / _APP_*), and compaction
**                                            copies any record it has no config for across
**                                            verbatim, so neither image can overwrite the other's
**                                            data. BM links none of this. See xCOMMON_MODULES/Src/
**                                            NVM_GEN2/README.md.)
**
** RAM layout (20 KB total - fixed regardless of the 64/128 KB flash bin on this part family):
**   .noinit_shared : 0x20000000 - 0x2000001F   32 B  (BM/FBL/APP handoff - see SHARED_RAM.h)
**   everything else: 0x20000020 - 0x20004FFF
*/

ENTRY(Reset_Handler)

_estack = 0x20005000;   /* top of 20 KB SRAM */

_Min_Heap_Size  = 0x0;     /* BM does not use a heap */
_Min_Stack_Size = 0x400;   /* 1 KB */

/* Flash addresses BM validates but does not itself occupy any section in - just constants
   MAIN.c's bm_config_s is built from. Must match APP/Makefile's APP_HEADER_ADDR/APP_CODE_START/
   APP_CODE_END (BM_FLASH_START + BM_FLASH_SIZE + FBL_FLASH_SIZE = 0x08008000) and
   xCOMMON_MODULES/Src/HEADER/HEADER.h's 256-byte header (code start = header + 0x100) - required,
   not cosmetic, since APP's/FBL's own vector tables need VTOR aligned to that boundary (see
   HEADER.h's comment).
   __app_code_end__ in particular MUST exactly equal APP/Makefile's APP_CODE_END - that's the
   range app_crc_injector/app_signer actually sign over, so a mismatch here makes every
   legitimately-signed APP image fail validation. Likewise __fbl_code_end__ must exactly equal
   FBL/Makefile's FBL_CODE_END. Same contract for __dataset0_code_end__/__dataset1_code_end__
   against APP/Makefile's DATASET0_FLASH_END/DATASET1_FLASH_END.

   Convention (used identically everywhere in the boot chain - BM.h, FBL.h, the Makefiles):
   *_header_start__ is the first byte of a region; *_code_start__ is the first byte of code;
   *_code_end__ is the LAST VALID BYTE of the region (inclusive), never one-past-the-end - see
   BM.c's bm_code_length()/FBL.c's FBL_calc_region_length(), both of which add +1 to get a byte
   count from these. FBL's own linker script defines __app_header_start__/__app_code_end__ with
   the same names and the same values, for the same reason.

   The header-to-code-start GAP is NOT uniform, though: APP/FBL's own headers are 256 bytes
   (code starts at header+0x100) because each owns a Cortex-M vector table that needs VTOR
   256-byte-aligned (see HEADER.h). Datasets carry the same APP_header_st tooling/fields
   (presence_pattern/crc/code_size/signature - see app_signer/app_crc_injector) but own no
   vector table and nothing ever points VTOR at one, so nothing requires that alignment for
   them - their header is only 96 bytes (code starts at header+0x60): 76 B (0x4C) is every byte
   app_crc_injector/app_signer actually read or write (their offsets are hardcoded, so that part
   can't shrink further without changing those tools), +20 B reserved for future fields (e.g. a
   version byte) without having to move code_start again. Built by hand-crafting the hex directly
   with those addresses rather than through this project's HEADER.h/linker convention (there is
   no compiled .app_header section for a dataset in any image). */
__fbl_header_start__  = 0x08002800;
__fbl_code_start__    = 0x08002900;
__fbl_code_end__      = 0x08007FFF;
__app_header_start__  = 0x08008000;
__app_code_start__    = 0x08008100;
__app_code_end__      = 0x0801EFFF;
__dataset0_header_start__ = 0x0801F000;
__dataset0_code_start__   = 0x0801F060;
__dataset0_code_end__     = 0x0801F3FF;
__dataset1_header_start__ = 0x0801F400;
__dataset1_code_start__   = 0x0801F460;
__dataset1_code_end__     = 0x0801F7FF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08000000, LENGTH = 10K
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
