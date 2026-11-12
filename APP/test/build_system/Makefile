####################################################################################################
#  build_system Makefile - builds this project's source and the xCOMMON_MODULES source into an x86 (MinGW)
#  static library, for the end to end tests, which link it to run the real modules on the Windows host.
#
#    mingw32-make -C APP/test/build_system -j8       build APP/test/build/artifacts/libHUB_APP_x86.a
#                                           (objects in APP/test/build/artifacts/obj), always built with coverage
#                                           instrumentation and without optimisation: gcov counts lines accurately only at -O0
#    mingw32-make -C APP/test/build_system clean            removes it
#
#  What goes in is decided by two text files, one entry per line, paths relative to the project root:
#
#    lib_sources.txt    a .c file to compile - add a file here to put it in the library
#    lib_includes.txt   a folder for the include path, for the headers that live in a folder with no
#                       listed source. The folder of every listed source is on the path already, ahead
#                       of these, so the project's own headers win over xCOMMON_MODULES/cfg as they do in
#                       the firmware build
#
#  The flags are COMMON_CFLAGS and APP_CFLAGS from rules.mk, the same ones the firmware uses, plus HOST_CFLAGS below.
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
#  Compiled with the firmware's struct layout (-fpack-struct=1 -fshort-enums), so the records, frames and
#  headers are the bytes they are on the target. A test that links the library must use them too, or the layouts
#  would differ: sw_test_project.yml has them too.
####################################################################################################
.SUFFIXES:

ROOT := ../../..

include $(ROOT)/rules.mk

# The output goes to ../build/artifacts, with Ceedling's own. Ceedling checks a library's folder exists when it
# loads its project file, before any build hook could create it - see the Makefile that runs the tests.
BUILD     := ../build/artifacts
LIB       := $(BUILD)/libHUB_APP_x86.a

SRCS    := $(file < lib_sources.txt)
INC_REL := $(patsubst %/,%,$(sort $(dir $(SRCS)))) $(file < lib_includes.txt)
OBJS    := $(addprefix $(BUILD)/obj/,$(SRCS:.c=.o))

# What is hardware and what is firmware. The virtual hardware and its models keep their state across a power on, and are not
# a place the firmware can be interrupted. Everything else is firmware:
#   - its RAM goes to the sections TMD_STARTUP resets at every power on (fw_ram_sections.sh)
#   - every function of it reports its entry, which is where TMD_TIME moves the clock and ticks the models (-finstrument-functions)
#   The models are hardware, and so is the test's own start of the ECU (sw_stubs: it is the models callback, which must not report its own entry)
HARDWARE_DIR  := xCOMMON_MODULES/test/support
STUBS_DIR     := APP/test/sw_stubs
MINGW_OBJCOPY ?= $(MINGW)-objcopy
MINGW_OBJDUMP ?= $(MINGW)-objdump

# Host build. -O0 -fprofile-arcs -ftest-coverage are the coverage build, which every test run uses: gcov counts
# lines accurately only at -O0
HOST_CFLAGS := \
	-m32 \
	-fshort-enums \
	-fpack-struct=1 \
	-std=gnu11 \
	-O0 \
	-g \
	-Wno-incompatible-pointer-types \
	-fprofile-arcs \
	-ftest-coverage \
	-finstrument-functions \
	-finstrument-functions-exclude-file-list=$(HARDWARE_DIR),$(STUBS_DIR)

CFLAGS := $(COMMON_CFLAGS) $(APP_CFLAGS) $(HOST_CFLAGS) -MMD -MP $(addprefix -I$(ROOT)/,$(INC_REL))

.PHONY: all clean
all: $(LIB)

$(LIB): $(OBJS)
	@mkdir -p $(dir $@)
	@rm -f $@
	@$(MINGW_AR) rcs $@ $^
	@echo "--> built $@ ($(words $(OBJS)) objects)"

# An object's path under obj/ is its source's path under the project root, so equal file names
# in different modules cannot collide
$(BUILD)/obj/%.o: $(ROOT)/%.c Makefile lib_sources.txt lib_includes.txt fw_ram_sections.sh
	@mkdir -p $(dir $@)
	@$(MINGW_CC) $(CFLAGS) -c $< -o $@
	@$(if $(filter $(HARDWARE_DIR)/% $(STUBS_DIR)/%,$*),,OBJCOPY=$(MINGW_OBJCOPY) OBJDUMP=$(MINGW_OBJDUMP) sh fw_ram_sections.sh $@)

clean:
	@rm -rf $(BUILD)/obj $(BUILD)/*.a
	@echo "--> library cleaned"

-include $(OBJS:.o=.d)

####################################################################################################
# END OF FILE
####################################################################################################
