####################################################################################################
#  build_system Makefile - builds this project's source and the xCOMMON_MODULES source into an x86 (MinGW)
#  static library, for the end to end tests, which link it to run the real modules on the Windows host.
#
#    mingw32-make -C APP/test/build_system -j8       build APP/test/build_system/libHUB_APP_x86.a
#                                           (objects in APP/test/build_system/build/obj), always built with coverage
#                                           instrumentation and without optimisation: gcov counts lines accurately only at -O0
#    mingw32-make -C APP/test/build_system clean            removes it
#
#  What goes in is decided by four text files, one entry per line, paths relative to the project root:
#
#    lib_sources.txt    a .c file to compile - add a file here to put it in the library
#    lib_includes.txt   a folder for the include path, for the headers that live in a folder with no
#                       listed source. The folder of every listed source is on the path already, ahead
#                       of these, so the project's own headers win over xCOMMON_MODULES/cfg as they do in
#                       the firmware build
#    lib_defines.txt    a preprocessor symbol, NAME or NAME=VALUE
#    lib_flags.txt      a compiler flag. -ffunction-sections / -fdata-sections let the linker drop
#                       unreferenced code from a member it pulls in; -O0 -fprofile-arcs -ftest-coverage are the
#                       coverage build, which every test run uses. The Makefile adds -MMD -MP itself
#
#  The STM32 is xCOMMON_MODULES/test/support/VHW_STM32F10X: its folder is first in lib_includes.txt, ahead of
#  the real stm32f10x.h / core_cm3.h / nvic_driver.h, and the SPL files (except misc.c and fsmc) are its host
#  copies, listed in lib_sources.txt in place of the originals, each holding the virtual hardware of its peripheral.
#
#  Not in the list on purpose: main.c, LOW_POWER_STM32F1.c and MCU_JUMP.c (target-only; the virtual
#  hardware has MCU_JUMP_software_reset), FREERTOS (target code), and CRYPTO (its third-party code is
#  Src/CRYPTO/libCRYPTO_x86.a in xCOMMON_MODULES, the facade is compiled by the test with its own
#  CRYPTO_config.h). TEST is not defined, so STATIC stays static - the library is the production code.
#
#  Compiled for the host, so without the firmware's -fpack-struct / -fshort-enums: a test that links the
#  library must not use them either, or the struct layouts would differ.
####################################################################################################
.SUFFIXES:

ROOT := ../../..

include $(ROOT)/rules.mk

# The library sits in this folder, not under build/: Ceedling checks a library's folder exists when it
# loads its project file, before any build hook could create it.
BUILD     := build
LIB       := libHUB_APP_x86.a

SRCS    := $(file < lib_sources.txt)
INC_REL := $(patsubst %/,%,$(sort $(dir $(SRCS)))) $(file < lib_includes.txt)
DEFINES := $(file < lib_defines.txt)
OBJS    := $(addprefix $(BUILD)/obj/,$(SRCS:.c=.o))

FLAGS  := $(file < lib_flags.txt)
CFLAGS := $(foreach f,$(FLAGS),$f) -MMD -MP $(addprefix -I$(ROOT)/,$(INC_REL)) $(addprefix -D,$(DEFINES))

.PHONY: all clean
all: $(LIB)

$(LIB): $(OBJS)
	@rm -f $@
	@$(MINGW_AR) rcs $@ $^
	@echo "--> built $@ ($(words $(OBJS)) objects)"

# An object's path under obj/ is its source's path under the project root, so equal file names
# in different modules cannot collide
$(BUILD)/obj/%.o: $(ROOT)/%.c Makefile lib_sources.txt lib_includes.txt lib_defines.txt lib_flags.txt
	@mkdir -p $(dir $@)
	@$(MINGW_CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf build build_cov libHUB_APP_x86.a libHUB_APP_x86_cov.a    # build_cov and _cov: what an older Makefile left
	@echo "--> library cleaned"

-include $(OBJS:.o=.d)

####################################################################################################
# END OF FILE
####################################################################################################
