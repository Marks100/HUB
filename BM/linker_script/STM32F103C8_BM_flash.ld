/*
** STM32F103C8T6 Boot Manager linker script
**
** Flash layout (128 KB total - confirmed via live F_SIZE register read at 0x1FFFF7E0; this part
** is a "C8" (64 KB nominal) that's physically 128 KB silicon, a known STM32F103 binning quirk):
**   BM   : 0x08000000 - 0x08002BFF   11 KB  <- this image (grown from 8 KB for future secure
**                                             boot headroom - e.g. ECDSA P-256 verify via
**                                             xCOMMON_MODULES/Src/CRYPTO/micro-ecc - then given
**                                             back 1 KB of that headroom to FBL, which had none
**                                             left, since BM still had ~8 KB free either way)
**   FBL  : 0x08002C00 - 0x08007FFF   21 KB  (grown twice from 17 KB, both times to fit the NVM
**                                            driver - FBL persists a WriteDataByIdentifier
**                                            fingerprint (see fbl_nvm_gen2_hw_interface_s in
**                                            FBL/Src/INT_STUBS/INTEGRATION_STUBS.c). 17->19 KB
**                                            was NVM.c's fixed page; 19->21 KB was NVM_GEN2, which
**                                            had filled the -Og debug build to 100%. Both times
**                                            taken from APP's flash budget, not BM's, since APP
**                                            has slack and BM's ~7 KB is earmarked for the ECDSA
**                                            upgrade above)
**     .app_header : 0x08002C00 - 0x08002CFF  256 B
**     code start  : 0x08002D00               (FBL_CODE_START)
**   APP  : 0x08008000 - 0x0801F7FF   94 KB  (shrunk from 99 KB across two FBL/NVM growths above)
**     .app_header : 0x08008000 - 0x080080FF  256 B
**     code start  : 0x08008100               (APP_CODE_START)
**   NVM  : 0x0801F800 - 0x0801FFFF   2 KB  (reserved, not a linker section here; placed
**                                            dynamically by FLS_STM32F1_get_nvm_base_address() at
**                                            the top two pages of whatever flash size the device
**                                            actually reports. Nothing above may claim these
**                                            pages. Both are NVM_GEN2's two flip-flop partitions -
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
   xCOMMON_MODULES/Src/HEADER/HEADER.h's 256-byte header (code start = header + 0x100).
   __app_code_end__ in particular MUST exactly equal APP/Makefile's APP_CODE_END - that's the
   range app_crc_injector/app_signer actually sign over, so a mismatch here makes every
   legitimately-signed APP image fail validation. Likewise __fbl_code_end__ must exactly equal
   FBL/Makefile's FBL_CODE_END.

   Convention (used identically everywhere in the boot chain - BM.h, FBL.h, the Makefiles):
   *_header_start__ is the first byte of a region; *_code_start__ is the first byte of code,
   256 bytes later; *_code_end__ is the LAST VALID BYTE of the region (inclusive), never
   one-past-the-end - see BM.c's bm_code_length()/FBL.c's FBL_calc_region_length(), both of which
   add +1 to get a byte count from these. FBL's own linker script defines __app_header_start__/
   __app_code_end__ with the same names and the same values, for the same reason. */
__fbl_header_start__  = 0x08002C00;
__fbl_code_start__    = 0x08002D00;
__fbl_code_end__      = 0x08007FFF;
__app_header_start__  = 0x08008000;
__app_code_start__    = 0x08008100;
__app_code_end__      = 0x0801F7FF;

MEMORY
{
    FLASH          (rx)  : ORIGIN = 0x08000000, LENGTH = 11K
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
