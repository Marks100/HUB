####################################################################################################
#                              BM - Boot Manager Build System                                      #
####################################################################################################
.SUFFIXES: .c .o
SHELL := /bin/bash
.SHELLFLAGS := -ec -o pipefail

include ../rules.mk

####################################################################################################
# Project Configuration
####################################################################################################
PROJECT_NAME     := BM
BM_LINKER_SCRIPT := linker_script/STM32F103C8_BM_flash.ld
OBJ_DIR          := $(BUILD_OUT_DIR)/obj
OUTPUT_NAME      := $(BUILD_OUT_DIR)/$(PROJECT_NAME)
BM_ELF_FILE      := $(OUTPUT_NAME).elf
BM_MAP_FILE      := $(OUTPUT_NAME).map
BM_HEX_FILE      := $(OUTPUT_NAME).hex
BM_BIN_FILE      := $(OUTPUT_NAME).bin
BM_MEM_OUTPUT_FILE := $(BUILD_OUT_DIR)/memory_analysis.txt
BM_MEM_CSV_FILE     := $(BUILD_OUT_DIR)/memory_analysis.csv
BUILD_ENV_BIN       := ../BuildEnv/xBuildEnv/bin

# Secure boot HMAC secret - both files gitignored (see .gitignore). HMAC_SECRET_FILE is the one
# thing that needs provisioning out-of-band (openssl rand -hex 32); HMAC_SECRET_HEADER is
# generated from it below, same pattern as APP/Makefile's autoversion target - never hand-edited,
# never committed, can't drift from what app_signer actually signs with.
HMAC_SECRET_FILE    := ../Tool_cfg/SigningKeys/hmac_secret.txt
HMAC_SECRET_HEADER  := Src/secure_boot_hmac_secret.h

####################################################################################################
# Sources
####################################################################################################
COMMON_MODULES_DIR := ../xCOMMON_MODULES/Src

# Curated list, not a glob of the whole MCAL tree like APP/Makefile uses.
BM_C_SRCS := \
    Src/bm_main.c \
    $(COMMON_MODULES_DIR)/BM/BM.c \
    $(COMMON_MODULES_DIR)/SHARED_RAM/SHARED_RAM.c \
    $(COMMON_MODULES_DIR)/MCU_JUMP/MCU_JUMP.c \
    $(COMMON_MODULES_DIR)/CHKSUM/CHKSUM.c \
    $(COMMON_MODULES_DIR)/CHKSUM/crc_hw_driver.c \
    $(COMMON_MODULES_DIR)/CHKSUM/CRC_HW_STM32F103.c \
    $(COMMON_MODULES_DIR)/CLKS/CLK_STM32F1.c \
    $(COMMON_MODULES_DIR)/CRYPTO/HMAC_SHA256/SHA256.c \
    $(COMMON_MODULES_DIR)/CRYPTO/HMAC_SHA256/HMAC_SHA256.c \
    $(COMMON_MODULES_DIR)/CRYPTO/micro-ecc/uECC.c \
    $(COMMON_MODULES_DIR)/CRYPTO/micro-ecc/ECDSA_P256_verify.c

# Vendored ST/CMSIS MCAL sources - same split as APP/Makefile's MCAL_C_SRCS/MCAL_OBJ_DIR/
# MCAL_OPTIMISATION: this is code we never touch, so there's no reason to debug it, and it always
# builds at -Os regardless of whatever BM_OPTIMISATION ends up being (see Flags section below).
# startup_stm32f10x_md.c is used exactly as APP uses it - unmodified, providing the real vector
# table, Reset_Handler (.data/.bss init), SystemInit() and the call into main() (bm_main.c's
# void main(void), same app_main()/main() split as APP/Src/MAIN/main.c). BM used to hand-roll its
# own 2-entry vector table and reimplement Reset_Handler to avoid linking this file at all; now
# it's linked and used the same way for both partitions instead - one startup flow, not two.
MCAL_OBJ_DIR := $(BUILD_OUT_DIR)/obj/mcal
MCAL_C_SRCS  := \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core/startup/startup_stm32f10x_md.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_rcc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_flash.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_crc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core/system_stm32f10x.c

BM_C_OBJS   := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(BM_C_SRCS)))
MCAL_C_OBJS := $(patsubst %.c, $(MCAL_OBJ_DIR)/%.o, $(notdir $(MCAL_C_SRCS)))
BM_C_DEPS   := $(BM_C_OBJS:.o=.d) $(MCAL_C_OBJS:.o=.d)
VPATH       := $(sort $(dir $(BM_C_SRCS) $(MCAL_C_SRCS)))

INCLUDES  := \
    -I Src \
    -I $(COMMON_MODULES_DIR)/BM \
    -I $(COMMON_MODULES_DIR)/SHARED_RAM \
    -I $(COMMON_MODULES_DIR)/HEADER \
    -I $(COMMON_MODULES_DIR)/MCU_JUMP \
    -I $(COMMON_MODULES_DIR)/CHKSUM \
    -I $(COMMON_MODULES_DIR)/CLKS \
    -I $(COMMON_MODULES_DIR)/CRYPTO/HMAC_SHA256 \
    -I $(COMMON_MODULES_DIR)/CRYPTO/micro-ecc \
    -I $(COMMON_MODULES_DIR)/STDC \
    -I $(COMMON_MODULES_DIR)/MCAL \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/inc \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core \
    -I $(COMMON_MODULES_DIR)/MCAL/stm32/cmsis_core \
    -I $(COMMON_MODULES_DIR) \
    -I ../Common_Src/PROJ_CONFIG

####################################################################################################
# Flags
####################################################################################################
# micro-ecc build config - portable C only (uECC_PLATFORM=0/uECC_arch_other), never the ARM/Thumb2
# asm path (those .inc files aren't vendored - see xCOMMON_MODULES/Src/CRYPTO/micro-ecc), secp256r1
# (P-256) only, no compressed-point support needed since app_signer already emits raw X||Y.
UECC_DEFINES := \
    -DuECC_PLATFORM=0 \
    -DuECC_SUPPORTS_secp160r1=0 \
    -DuECC_SUPPORTS_secp192r1=0 \
    -DuECC_SUPPORTS_secp224r1=0 \
    -DuECC_SUPPORTS_secp256r1=1 \
    -DuECC_SUPPORTS_secp256k1=0 \
    -DuECC_SUPPORT_COMPRESSED_POINT=0

# BM has no RELEASE/DEBUG toggle (unlike APP/Makefile), so BM_OPTIMISATION is just -Os today - but
# MCAL_OPTIMISATION is named/split out separately anyway, same as APP does, so vendored code stays
# pinned to -Os independent of whatever BM_OPTIMISATION becomes if a debug mode is ever added here.
# No -DVECT_TAB_OFFSET override needed here (unlike FBL/Makefile and APP/Makefile) - BM's own
# vector table sits at FLASH_BASE (0x08000000) already, which is SystemInit()'s hardcoded default
# when VECT_TAB_OFFSET isn't overridden, so its unconditional SCB->VTOR write is already correct
# for BM. FBL and APP both sit at a nonzero offset from FLASH_BASE and do need the override - see
# FBL/Makefile's CFLAGS += -DVECT_TAB_OFFSET comment for what breaks without it.
BM_OPTIMISATION   := -Os
MCAL_OPTIMISATION := -Os

LDFLAGS := \
    $(LDFLAGS_COMMON) \
    -Os \
    --specs=nano.specs \
    --specs=nosys.specs \
    "-Wl,-Map=$(BM_MAP_FILE)" \
    -T$(BM_LINKER_SCRIPT) \
    -o $(BM_ELF_FILE)

####################################################################################################
# Targets
####################################################################################################
.PHONY: bm_build bm_clean bm_rebuild basic_memory_stats full_memory_stats memory_stats gen_hmac_secret_header

bm_build: gen_hmac_secret_header $(BM_ELF_FILE) basic_memory_stats

bm_rebuild: bm_clean bm_build

# HMAC_SECRET_FILE gets its own rule so a missing secret still fails with a clear "run this
# openssl command" message instead of Make's generic "No rule to make target ... Stop" - same
# concern the old always-phony version worked around, now handled by giving the missing-
# prerequisite case its own recipe instead of by skipping real dependency tracking on the header.
$(HMAC_SECRET_FILE):
	@echo "ERROR: secret file not found: $(HMAC_SECRET_FILE)" >&2
	@echo "       Generate one with: openssl rand -hex 32 > $(HMAC_SECRET_FILE)" >&2
	@exit 1

# Real prerequisite now - only regenerates the header (and therefore only forces bm_main.o to
# recompile) when hmac_secret.txt actually changes or the header doesn't exist yet, instead of
# unconditionally rewriting it - and recompiling bm_main.o - on every single build.
$(HMAC_SECRET_HEADER): $(HMAC_SECRET_FILE)
	@$(BUILD_ENV_BIN)/gen_secure_boot_hmac_header $(HMAC_SECRET_FILE) --output=$(HMAC_SECRET_HEADER)

gen_hmac_secret_header: $(HMAC_SECRET_HEADER)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(MCAL_OBJ_DIR):
	@mkdir -p $(MCAL_OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(BM_OPTIMISATION) $(INCLUDES) $(UECC_DEFINES) -c $< -o $@

$(MCAL_OBJ_DIR)/%.o: %.c | $(MCAL_OBJ_DIR)
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(MCAL_OPTIMISATION) $(INCLUDES) -c $< -o $@

$(BM_ELF_FILE): $(BM_C_OBJS) $(MCAL_C_OBJS)
	@mkdir -p $(BUILD_OUT_DIR)
	@echo "--> Linking BM..."
	@$(LD) $(LDFLAGS) $(BM_C_OBJS) $(MCAL_C_OBJS)
	@$(OBJCOPY) -O ihex   $(BM_ELF_FILE) $(BM_HEX_FILE)
	@$(OBJCOPY) -O binary $(BM_ELF_FILE) $(BM_BIN_FILE)
	@echo "--> BM image: $(realpath $(BM_HEX_FILE))"

bm_clean:
	@rm -fr $(BUILD_OUT_DIR)
	@echo "--> BM cleaned"

$(BM_MAP_FILE): $(BM_ELF_FILE)

basic_memory_stats: $(BM_ELF_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(BM_MAP_FILE) $(BM_ELF_FILE) $(BM_LINKER_SCRIPT) --out $(BM_MEM_OUTPUT_FILE) --csv $(BM_MEM_CSV_FILE)

full_memory_stats: $(BM_MAP_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(BM_MAP_FILE) $(BM_ELF_FILE) $(BM_LINKER_SCRIPT) --out $(BM_MEM_OUTPUT_FILE) --csv $(BM_MEM_CSV_FILE)

# Legacy alias - this is what tasks.json's "BM_memory_stats" task already calls
memory_stats: basic_memory_stats

-include $(BM_C_DEPS)
