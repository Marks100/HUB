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

####################################################################################################
# Sources
####################################################################################################
COMMON_MODULES_DIR := ../xCOMMON_MODULES/Src

BM_C_SRCS := \
    Src/bm_main.c \
    $(COMMON_MODULES_DIR)/MCU_JUMP/MCU_JUMP.c

BM_C_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(BM_C_SRCS)))
VPATH     := $(sort $(dir $(BM_C_SRCS)))

INCLUDES  := \
    -I Src \
    -I $(COMMON_MODULES_DIR)/MCU_JUMP \
    -I $(COMMON_MODULES_DIR)/STDC \
    -I $(COMMON_MODULES_DIR)/MCAL \
    -I $(COMMON_MODULES_DIR) \
    -I ../Common_Src/PROJ_CONFIG

####################################################################################################
# Flags
####################################################################################################
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
.PHONY: bm_build bm_clean

bm_build: $(BM_ELF_FILE)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) -Os $(INCLUDES) -c $< -o $@

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
