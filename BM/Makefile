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

# Curated list, not a glob of the whole MCAL tree like APP/Makefile uses - APP's MCAL glob
# pulls in startup_stm32f10x_md.c, which defines its own full .isr_vector; BM defines its own
# minimal 2-entry vector table (bm_vector_table in bm_main.c) and must not link that startup
# file too, or .isr_vector ends up with two vector tables concatenated in it.
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
    $(COMMON_MODULES_DIR)/CRYPTO/micro-ecc/ECDSA_P256_verify.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_rcc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_flash.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/src/stm32f10x_crc.c \
    $(COMMON_MODULES_DIR)/MCAL/stm32/stm32f10x/core/system_stm32f10x.c

BM_C_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(BM_C_SRCS)))
VPATH     := $(sort $(dir $(BM_C_SRCS)))

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

# Deliberately phony (always runs), not a real $(HMAC_SECRET_HEADER): $(HMAC_SECRET_FILE) file
# rule - if HMAC_SECRET_FILE doesn't exist, Make's own dependency resolution would refuse to even
# invoke the recipe ("No rule to make target ... Stop"), burying gen_secure_boot_hmac_header's
# actual "here's the openssl command to fix this" error before it can ever print.
gen_hmac_secret_header:
	@$(BUILD_ENV_BIN)/gen_secure_boot_hmac_header $(HMAC_SECRET_FILE) --output=$(HMAC_SECRET_HEADER)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) -Os $(INCLUDES) $(UECC_DEFINES) -c $< -o $@

$(BM_ELF_FILE): $(BM_C_OBJS)
	@mkdir -p $(BUILD_OUT_DIR)
	@echo "--> Linking BM..."
	@$(LD) $(LDFLAGS) $(BM_C_OBJS)
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
