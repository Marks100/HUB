#!/bin/sh
####################################################################################################
#  fw_ram_sections.sh OBJECT
#
#  Moves the RAM of one firmware object into two sections of its own:
#
#      .data and .data<sep>name   ->  fwd     the initialised variables
#      .bss  and .bss<sep>name    ->  fwb     the zero-initialised variables
#
#  Every firmware object of the host library gets the same two names, so the linker puts all the firmware's
#  RAM together, apart from the test framework's, the models' and the virtual hardware's. That is what lets
#  TMD_STARTUP (xCOMMON_MODULES/test/support/models/TMD_STARTUP) do what the target's startup code does at
#  every reset - copy the initial values to fwd and zero fwb - without touching anything that is not firmware.
#
#  Not moved: the coverage counters (gcov), which must keep counting across a power on, and sections the firmware
#  places itself (.noinit_shared), which keep their contents across a reset on the target as well.
#
#  Section names:  COFF (Windows)  .fwd .fwb   - the object uses name$variable, flattened here to one name
#                  ELF  (Linux)     fwd  fwb   - a name without a dot, so the linker defines __start_fwd / __stop_fwd
#
#  Environment: OBJCOPY and OBJDUMP, the host toolchain's.
####################################################################################################
object="$1"

if $OBJDUMP -f "$object" | grep -q 'file format pe-'; then
    data=.fwd
    bss=.fwb
else
    data=fwd
    bss=fwb
fi

# One --rename-section for every .data / .bss section of the object, except the coverage counters
renames=$($OBJDUMP -h "$object" | awk -v data="$data" -v bss="$bss" '
    $2 ~ /^\.(data|bss)([$.]|$)/ && $2 !~ /gcov/ {
        new = ($2 ~ /^\.data/) ? data : bss
        printf "--rename-section %s=%s ", $2, new
    }')

if [ -n "$renames" ]; then
    $OBJCOPY $renames "$object"
fi
