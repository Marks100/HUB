PROJECT_TYPE := HUB

MAKEFLAGS :=  --no-print-directory \
			  --no-builtin-rules \
			  --silent \

####################################################################################################
# OS Detection - TTLD is set by setup_paths.bat on Windows (C:\SVN_source\Tools)
# On Linux/CI, TTLD is not set so we fall back to system-installed tools
# All tools are expected to be on PATH (set by setup_paths.bat on Windows, installed on Linux)
####################################################################################################
ifdef TTLD
    OPEN_CMD    := start
else
    OPEN_CMD    := xdg-open
endif

GCOVR_EXE  := gcovr

# ARM toolchain
ARM		    := arm-none-eabi
CC		    := $(ARM)-gcc
LD		    := $(ARM)-gcc
OBJCOPY	    := $(ARM)-objcopy
AS          := $(ARM)-as
AR          := $(ARM)-ar
NM          := $(ARM)-nm
STRIP       := $(ARM)-strip

# MinGW toolchain (for test environment)
MINGW       := i686-w64-mingw32
MINGW_CC    := $(MINGW)-gcc
MINGW_AR    := $(MINGW)-ar
MINGW_LD    := $(MINGW)-gcc

# Common tools
SREC        := srec_cat
ZIP         := 7za

LDFLAGS_COMMON := \
	-mcpu=cortex-m3 \
	-mthumb \
	-g \
	-nostartfiles \
	-Wl,--gc-sections \

# Shared by the firmware and the x86 test library (APP/test/build_system), so both compile the code the same way.
# Nothing that changes a struct layout or needs the ARM compiler belongs here.
COMMON_CFLAGS := \
	-Wall \
	-Wno-unknown-pragmas \
	-Wno-main \
	-trigraphs \
	-Wno-pragmas \
	-Wno-pointer-sign \
	-ffunction-sections \
	-fdata-sections \
	-Wno-format \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-unused-but-set-variable \
	-Wswitch-default \
	-DSTM32F103xB \
	-DTARGET_STM32F103C8T6 \
	-DTARGET_STM32F1 \
	-DSTM32F10X_MD \

# Firmware only: the Cortex-M3 target, its packed layout and its debug format
CFLAGS :=  \
	$(COMMON_CFLAGS) \
	-mcpu=cortex-m3 \
	-mthumb \
	-fshort-enums \
	-fpack-struct=1 \
	-fstack-usage \
	-g3 \
	-gdwarf-4 \
	-c \
	-Wl,--print-memory-usage \
	-MMD \
	-MP \
	-D__ASSEMBLY__ \

# APP only, shared with the x86 test library: FAULT_MGR slot cap. APP defines no faults yet (no fault table, no
# monitors), so the module's default of 64 slots only costs RAM. 8 is a placeholder - raise it when APP gets a real
# fault table; init rejects a table larger than this, so it cannot silently overflow.
APP_CFLAGS := -DFAULT_MGR_MAX_FAULTS=8

#either s32k or stm32 currently supported
CPU_TYPE                 := stm32f103

# Hardware variant, SUPER_PILL or BLUE_PILL (Common_Src/PROJ_CONFIG/HAL_config.h), for APP, BM and FBL alike: a build
# takes it from the command line (make variant_<name> in APP/) and the sub-makes APP starts for BM and FBL inherit it.
# Every variant builds into its own Build/<VARIANT>, so one never links with another's objects.
HW_VARIANT               ?= SUPER_PILL
BUILD_OUT_DIR            := Build/$(HW_VARIANT)
CFLAGS                   += -DHW_VARIANT=$(HW_VARIANT)

COMMON_SRC_DIRS          := ../Common_Src ../xCOMMON_MODULES/Src ../xCOMMON_MODULES/cfg
RELEASE_PACKAGE_BASE_DIR := ../Release_package

# Build timing - captured at Makefile parse time so all targets share one start point
START_TIME               := $(shell date +%s)

# Parallel job count - set explicitly or leave unset to auto-detect via nproc
#JOBS                     := 1
JOBS                      ?= $(shell nproc 2>/dev/null || echo 6)
