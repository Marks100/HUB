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

CFLAGS :=  \
	-mcpu=cortex-m3 \
	-mthumb \
	-Wall \
	-Wno-unknown-pragmas \
	-Wno-main \
	-trigraphs \
	-fshort-enums \
	-fpack-struct=1 \
	-fstack-usage \
	-Wno-pragmas \
	-Wno-pointer-sign \
	-ffunction-sections \
	-Wno-format \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-unused-but-set-variable \
	-Wswitch-default \
	-g3 \
	-gdwarf-4 \
	-c \
	-Wl,--print-memory-usage \
	-MMD \
	-MP \
	-DSTM32F103xB \
	-DTARGET_STM32F103C8T6 \
	-DTARGET_STM32F1 \
	-DSTM32F10X_MD \
	-D__ASSEMBLY__ \

#either s32k or stm32 currently supported
CPU_TYPE                 := stm32f103
BUILD_OUT_DIR            := Build
COMMON_SRC_DIRS          := ../Common_Src ../xCOMMON_MODULES/Src ../xCOMMON_MODULES/cfg
RELEASE_PACKAGE_BASE_DIR := ../Release_package

# Build timing - captured at Makefile parse time so all targets share one start point
START_TIME               := $(shell date +%s)

# Parallel job count - set explicitly or leave unset to auto-detect via nproc
#JOBS                     := 1
JOBS                     ?= $(shell nproc 2>/dev/null || echo 6)
