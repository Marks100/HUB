####################################################################################################
#                              FBL - Field Bootloader Build System                                 #
####################################################################################################
.SUFFIXES: .c .o
SHELL := /bin/bash
.SHELLFLAGS := -ec -o pipefail

include ../rules.mk

####################################################################################################
# Project Configuration
####################################################################################################
PROJECT_NAME      := FBL
FBL_LINKER_SCRIPT := linker_script/STM32F103C8_FBL_flash.ld
OBJ_DIR            := $(BUILD_OUT_DIR)/obj
OUTPUT_NAME        := $(BUILD_OUT_DIR)/$(PROJECT_NAME)
FBL_ELF_FILE       := $(OUTPUT_NAME).elf
FBL_MAP_FILE       := $(OUTPUT_NAME).map
FBL_HEX_FILE       := $(OUTPUT_NAME).hex
FBL_BIN_FILE       := $(OUTPUT_NAME).bin
FBL_HEX_WITH_CRC_FILE := $(OUTPUT_NAME)_with_CRC.hex
FBL_HEX_SIGNED_FILE   := $(OUTPUT_NAME)_signed.hex
FBL_MEM_OUTPUT_FILE := $(BUILD_OUT_DIR)/memory_analysis.txt
FBL_MEM_CSV_FILE     := $(BUILD_OUT_DIR)/memory_analysis.csv
BUILD_ENV_BIN         := ../BuildEnv/xBuildEnv/bin

# FBL carries the same header/CRC/signature layout as APP (see xCOMMON_MODULES/Src/HEADER/
# HEADER.h) at the start of its own 12K region - BM validates it the same way before jumping
# here (see BM.c's BM_jump_to_fbl()). Must match FBL/linker_script/STM32F103C8_FBL_flash.ld and
# APP/Makefile's FBL_FLASH_START (0x08002000).
FBL_HEADER_ADDR := 0x08002000
FBL_CODE_START  := 0x08002100
FBL_CODE_END    := 0x08004FFF
FBL_CRC_TYPE    := stm32-hw-crc32

# Signing key - must stay in lockstep with APP/Makefile's SIGN_ALGORITHM/SIGNING_KEY and with
# whatever BM/Src/bm_main.c verifies with (currently HMAC-SHA256, same secret for both stages).
SIGN_ALGORITHM  := hmac-sha256
SIGNING_KEY     := ../Tool_cfg/SigningKeys/hmac_secret.txt

####################################################################################################
# Sources
####################################################################################################
COMMON_MODULES_DIR := ../xCOMMON_MODULES/Src

# Curated list, not a glob of the whole MCAL tree like APP/Makefile uses - same reason BM/Makefile
# avoids it (APP's MCAL glob pulls in startup_stm32f10x_md.c, which defines its own full
# .isr_vector; FBL defines its own minimal 2-entry vector table in fbl_main.c and must not link
# that startup file too). Also deliberately NOT a directory glob over CAN_TRCV/CANTP/UDS/etc. -
# the source project's FBL Makefile globbed its entire MCAL tree and pulled in ~47KB of unused
# NXP SDK drivers as a result; every file below is one FBL actually calls.
FBL_C_SRCS := \
    Src/fbl_main.c \
    Src/UDS_CFG/UDS_config.c \
    $(COMMON_MODULES_DIR)/FBL/FBL.c \
    $(COMMON_MODULES_DIR)/HEADER/HEADER.c \
    $(COMMON_MODULES_DIR)/UDS/UDS.c \
    $(COMMON_MODULES_DIR)/CANTP/CANTP.c \
    $(COMMON_MODULES_DIR)/PDUR/PDUR.c \
    $(COMMON_MODULES_DIR)/CAN_TRCV/TJA1051/TJA1051.c \
    $(COMMON_MODULES_DIR)/SHARED_RAM/SHARED_RAM.c \
    $(COMMON_MODULES_DIR)/MCU_JUMP/MCU_JUMP.c \
    $(COMMON_MODULES_DIR)/CHKSUM/CHKSUM.c \
    $(COMMON_MODULES_DIR)/CHKSUM/crc_hw_driver.c \
    $(COMMON_MODULES_DIR)/CHKSUM/CRC_HW_STM32F103.c \
    $(COMMON_MODULES_DIR)/CLKS/CLK_STM32F1.c \
    $(COMMON_MODULES_DIR)/FLS/FLS_STM32F1.c \
    $(COMMON_MODULES_DIR)/SYSTICK/SYSTICK.c \
    $(COMMON_MODULES_DIR)/TIME_MGR/TIME_MGR.c \
    $(COMMON_MODULES_DIR)/STDC/STDC.c \
    $(COMMON_MODULES_DIR)/SELMATH/SELMATH.c \
    $(COMMON_MODULES_DIR)/stdio/printf.c \
    ../Common_Src/HAL_CAN/HAL_CAN.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_rcc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_flash.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_crc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_can.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_gpio.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/misc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core/system_stm32f10x.c

FBL_C_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(FBL_C_SRCS)))
FBL_C_DEPS := $(FBL_C_OBJS:.o=.d)
VPATH      := $(sort $(dir $(FBL_C_SRCS)))

INCLUDES  := \
    -I Src \
    -I Src/UDS_CFG \
    -I $(COMMON_MODULES_DIR)/FBL \
    -I $(COMMON_MODULES_DIR)/UDS \
    -I $(COMMON_MODULES_DIR)/CANTP \
    -I $(COMMON_MODULES_DIR)/PDUR \
    -I $(COMMON_MODULES_DIR)/CAN_TRCV/TJA1051 \
    -I $(COMMON_MODULES_DIR)/SHARED_RAM \
    -I $(COMMON_MODULES_DIR)/HEADER \
    -I $(COMMON_MODULES_DIR)/MCU_JUMP \
    -I $(COMMON_MODULES_DIR)/CHKSUM \
    -I $(COMMON_MODULES_DIR)/CLKS \
    -I $(COMMON_MODULES_DIR)/FLS \
    -I $(COMMON_MODULES_DIR)/SYSTICK \
    -I $(COMMON_MODULES_DIR)/TIME_MGR \
    -I $(COMMON_MODULES_DIR)/STDC \
    -I $(COMMON_MODULES_DIR)/SELMATH \
    -I $(COMMON_MODULES_DIR)/stdio \
    -I $(COMMON_MODULES_DIR)/MCAL \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/inc \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/cmsis_core \
    -I $(COMMON_MODULES_DIR) \
    -I ../Common_Src/HAL_CAN \
    -I ../APP/Src \
    -I ../Common_Src/PROJ_CONFIG

####################################################################################################
# Flags
####################################################################################################
LDFLAGS := \
    $(LDFLAGS_COMMON) \
    -Os \
    --specs=nano.specs \
    --specs=nosys.specs \
    "-Wl,-Map=$(FBL_MAP_FILE)" \
    -T$(FBL_LINKER_SCRIPT) \
    -o $(FBL_ELF_FILE)

####################################################################################################
# Targets
####################################################################################################
.PHONY: fbl_build fbl_clean fbl_rebuild basic_memory_stats full_memory_stats memory_stats

fbl_build: $(FBL_ELF_FILE) basic_memory_stats

fbl_rebuild: fbl_clean fbl_build

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) -Os $(INCLUDES) -c $< -o $@

$(FBL_ELF_FILE): $(FBL_C_OBJS)
	@mkdir -p $(BUILD_OUT_DIR)
	@echo "--> Linking FBL..."
	@$(LD) $(LDFLAGS) $(FBL_C_OBJS)
	@$(OBJCOPY) -O ihex   $(FBL_ELF_FILE) $(FBL_HEX_FILE)
	@$(OBJCOPY) -O binary $(FBL_ELF_FILE) $(FBL_BIN_FILE)
	@echo "--> Filling gaps in HEX file with 0xFF (from code end to region end)..."
	@$(SREC) $(FBL_HEX_FILE) -Intel --fill 0xFF $(FBL_CODE_START) $(FBL_CODE_END) -Output $(FBL_HEX_FILE) -Intel -address-length=4
	@echo "--> Injecting CRC32 into FBL header (HEX file)..."
	@$(BUILD_ENV_BIN)/app_crc_injector $(FBL_HEX_FILE) --output=$(FBL_HEX_WITH_CRC_FILE) --header-addr=$(FBL_HEADER_ADDR) --code-start=$(FBL_CODE_START) --code-end=$(FBL_CODE_END) --crc-type=$(FBL_CRC_TYPE)
	@echo "--> Signing firmware ($(SIGN_ALGORITHM))..."
	@$(BUILD_ENV_BIN)/app_signer $(FBL_HEX_WITH_CRC_FILE) --output=$(FBL_HEX_SIGNED_FILE) --header-addr=$(FBL_HEADER_ADDR) --code-start=$(FBL_CODE_START) --code-end=$(FBL_CODE_END) --algorithm=$(SIGN_ALGORITHM) --signing-key=$(SIGNING_KEY)
	@echo "--> FBL image: $(realpath $(FBL_HEX_SIGNED_FILE))  (FLASH THIS!)"

fbl_clean:
	@rm -fr $(BUILD_OUT_DIR)
	@echo "--> FBL cleaned"

$(FBL_MAP_FILE): $(FBL_ELF_FILE)

basic_memory_stats: $(FBL_ELF_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(FBL_MAP_FILE) $(FBL_ELF_FILE) $(FBL_LINKER_SCRIPT) --out $(FBL_MEM_OUTPUT_FILE) --csv $(FBL_MEM_CSV_FILE)

full_memory_stats: $(FBL_MAP_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(FBL_MAP_FILE) $(FBL_ELF_FILE) $(FBL_LINKER_SCRIPT) --out $(FBL_MEM_OUTPUT_FILE) --csv $(FBL_MEM_CSV_FILE)

memory_stats: basic_memory_stats

-include $(FBL_C_DEPS)
