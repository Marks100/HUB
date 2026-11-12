####################################################################################################
#                                  Configuration Variables                                         #
####################################################################################################
.SUFFIXES: .c .S .o .d
SHELL := /bin/bash
.SHELLFLAGS := -ec -o pipefail

include ../Tool_cfg/GMSL/gmsl
include ../rules.mk

####################################################################################################
# Project Configuration
####################################################################################################
PROJECT_NAME                 := APP
VERSION                      := $(subst .,_,$(shell sVersion -l --yml project.yml))
export VERSION

####################################################################################################
# Directory Structure
####################################################################################################
APP_SRC_DIRS                 := Src
APP_LINKER_SCRIPT            := linker_script/STM32F103C8_flash.ld
APP_OBJ_DIR                  := $(BUILD_OUT_DIR)/obj
COMMON_OBJ_DIR               := $(BUILD_OUT_DIR)/obj/common
MCAL_OBJ_DIR                 := $(BUILD_OUT_DIR)/obj/mcal

####################################################################################################
# Output Files
####################################################################################################
OUTPUT_NAME                  := $(BUILD_OUT_DIR)/$(PROJECT_NAME)
APP_ELF_FILE                 := $(OUTPUT_NAME).elf
APP_MAP_FILE                 := $(OUTPUT_NAME).map
APP_HEX_FILE                 := $(OUTPUT_NAME).hex
APP_BIN_FILE                 := $(OUTPUT_NAME).bin
APP_SREC_FILE                := $(OUTPUT_NAME).srec
APP_HEX_PADDED_FILE          := $(OUTPUT_NAME)_padded.hex
APP_HEX_WITH_CRC_FILE        := $(OUTPUT_NAME)_with_CRC.hex
APP_HEX_SIGNED_FILE          := $(OUTPUT_NAME)_signed.hex
APP_S19_SIGNED_FILE          := $(OUTPUT_NAME)_signed.s19
DATASET0_CSV                 := ../Tool_cfg/TYRE_DATA/tyre_pressures.csv
DATASET0_GEN                 := ../scripts/tyre_dataset_gen
DATASET0_HEX_FILE            := $(BUILD_OUT_DIR)/DATASET0.hex
DATASET0_HEX_WITH_CRC_FILE   := $(BUILD_OUT_DIR)/DATASET0_with_CRC.hex
DATASET0_HEX_SIGNED_FILE     := $(BUILD_OUT_DIR)/DATASET0_signed.hex
OBJECT_SIZE_FILE             := $(BUILD_OUT_DIR)/object_sizes.txt
APP_COMPILER_OUTPUT          := $(BUILD_OUT_DIR)/compiler_warnings.txt
APP_MEM_OUTPUT_FILE          := $(BUILD_OUT_DIR)/memory_analysis.txt
APP_MEM_CSV_FILE             := $(BUILD_OUT_DIR)/memory_analysis.csv
# The objects to link, in a response file: all of them on the command line is close to the length the shell here allows,
# and a longer output directory (Build/<VARIANT>/) goes over it
LINK_OBJECT_LIST             := $(BUILD_OUT_DIR)/link_objects.rsp
AUTOVERSION_FILE             := Src/autoversion.h
RELEASE_PACKAGE_NAME         := $(RELEASE_PACKAGE_BASE_DIR)/$(PROJECT_TYPE)_$(VERSION)

# Flash layout - must match the linker scripts in BM/, FBL/, APP/. Change a size in all three.
# STM32F103C8T6: 128KB at 0x08000000 (a "C8" is 64KB nominal but 128KB silicon, confirmed by reading
# F_SIZE at 0x1FFFF7E0). BM=8KB  FBL=24KB  APP=90KB  DATASET0=4KB  NVM=2KB.
# The top 2KB (0x0801F800-0x0801FFFF) is the NVM_GEN2 pages, fixed in Common_Src/PROJ_CONFIG/PROJ_config.h:
# APP and DATASET0 must stay below them. FBL and APP share those pages, see NVM_GEN2/README.md.
# DATASET0 is downloaded through FBL and validated by BM at boot.
BM_FLASH_START               := 0x08000000
BM_FLASH_SIZE                := 0x2000
BM_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) - 1 )))

FBL_FLASH_START               := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) )))
FBL_FLASH_SIZE                := 0x6000
FBL_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) - 1 )))

APP_FLASH_START               := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) )))
APP_FLASH_SIZE                := 0x16800
APP_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) + $(APP_FLASH_SIZE) - 1 )))

# One past APP_FLASH_END - the exclusive upper bound srec_cat's -fill wants. See the padding step in
# $(APP_ELF_FILE)'s recipe below for why the built hex is padded to the whole region.
APP_FLASH_LIMIT               := $(shell printf '0x%08X' $$(( $(APP_FLASH_START) + $(APP_FLASH_SIZE) )))

# DATASET0 - a 4KB slot after APP, before the NVM pages: 96B header + 4000B payload. The header is the
# APP_header_st of HEADER.h, but smaller than APP's: no vector table, so no 256B VTOR alignment (see the
# region constants in BM/linker_script/STM32F103C8_BM_flash.ld). Built by the `dataset` target, not the APP build.
# Must match the __dataset0_* constants in the BM, FBL and APP linker scripts, as FBL_CODE_END does for the FBL.
DATASET0_FLASH_START          := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) + $(APP_FLASH_SIZE) )))
DATASET0_FLASH_SIZE           := 0x1000
DATASET0_FLASH_END            := $(shell printf '0x%08X' $$(( $(DATASET0_FLASH_START) + $(DATASET0_FLASH_SIZE) - 1 )))
DATASET0_HEADER_SIZE          := 0x60
DATASET0_CODE_START           := $(shell printf '0x%08X' $$(( $(DATASET0_FLASH_START) + $(DATASET0_HEADER_SIZE) )))

APP_HEADER_ADDR               := $(APP_FLASH_START)
APP_CODE_START                := $(shell printf '0x%08X' $$(( $(APP_FLASH_START) + 0x100 )))
APP_CODE_END                  := $(APP_FLASH_END)

# Must match what BM's hardware CRC peripheral actually computes, not just be "a CRC32" - see
# app_crc_injector's --crc-type help text and xCOMMON_MODULES/Src/CHKSUM/CRC_HW_STM32F103.c.
# STM32F1's CRC unit has no reflect/complement capability, unlike S32K144's (which is configured
# to match plain crc32 - see CRC_HW_S32K144.c), so this target needs its own algorithm.
APP_CRC_TYPE                 := stm32-hw-crc32

BM_IMAGE                     := ../BM/$(BUILD_OUT_DIR)/BM.hex
FBL_IMAGE                    := ../FBL/$(BUILD_OUT_DIR)/FBL_signed.hex
COMBINED_IMAGE               := $(BUILD_OUT_DIR)/BM_FBL_APP_combined.hex

# EOL (End-of-Line) data injection
EOL_CONFIG                   := ../Tool_cfg/EOL/eol_config.yml
EOL_INJECTOR                 := ../Tool_cfg/EOL/inject_eol_data

# Signing keys - SIGN_ALGORITHM must match whatever key/secret type SIGNING_KEY actually is
# (see BuildEnv/xBuildEnv/bin/app_signer --help for the ecdsa-p256/ed25519/hmac-sha256 options).
# BM currently verifies with HMAC-SHA256 (see BM/Src/bm_main.c/secure_boot_hmac_secret.h) - these
# two must stay in lockstep, or every APP build signs with a scheme BM will reject at boot.
SIGN_ALGORITHM               := hmac-sha256
SIGNING_KEY                  := ../Tool_cfg/SigningKeys/hmac_secret.txt
FINAL_IMAGE_WITH_EOL         := $(BUILD_OUT_DIR)/BM_FBL_APP_with_EOL.hex

# Flash pack
FLASH_PACK_SCRIPT            := ../scripts/create_flash_pack
FLASH_PACK_PROFILE           := ../Tool_cfg/CANFLASH/hub_bootloader.json
FLASH_PACK_SEEDKEY           := ../Tool_cfg/CANFLASH/seedkeydll/seedkey.dll
FLASH_PACK_OUTPUT            := $(BUILD_OUT_DIR)/$(PROJECT_NAME)_$(VERSION).flashpack

####################################################################################################
# Local build-tool paths (avoids relying on which project's BuildEnv is first on PATH)
####################################################################################################
BUILD_ENV_BIN                := ../BuildEnv/xBuildEnv/bin

####################################################################################################
# MCAL Sources (from xCOMMON_MODULES)
####################################################################################################
MCAL_STM32_DIR               := ../xCOMMON_MODULES/Src/MCAL/stm32
MCAL_SRC_DIR                 := $(MCAL_STM32_DIR)/stm32f10x
# STM32F103C8T6 is medium-density — exclude low-density (ld) and high-density (hd) startup variants
MCAL_C_SRCS                  := $(shell find $(MCAL_SRC_DIR) -type f -name '*.c' ! -name 'startup_stm32f10x_ld.c' ! -name 'startup_stm32f10x_hd.c')
MCAL_S_SRCS                  := $(shell find $(MCAL_SRC_DIR) -type f \( -name '*.S' -o -name '*.s' \))
MCAL_INCLUDES                := $(addprefix -I ,$(shell find $(MCAL_SRC_DIR) $(MCAL_STM32_DIR)/cmsis_core -type d)) \
                                -I ../xCOMMON_MODULES/Src/MCAL

####################################################################################################
# Source File Discovery
####################################################################################################
APP_C_SRCS                   := $(shell find $(APP_SRC_DIRS) -type f -name '*.c')
APP_S_SRCS                   := $(shell find $(APP_SRC_DIRS) -type f -name '*.S')
# FREERTOS (the shared tree, which APP does not use) is left out of the sources and the include paths
COMMON_SRCS                  := $(shell find $(COMMON_SRC_DIRS) -type f -name '*.c' ! -path '*/test/*' ! -path '*/MCAL/*' ! -path '*/CRYPTO/*' ! -path '*/xCOMMON_MODULES/cfg/*' ! -path '*/FREERTOS/*')

# Include paths - add "-I" prefix to all discovered directories
# Exclude MCAL subdirs from the broad search; their headers come exclusively from MCAL_INCLUDES
INCLUDES                     := $(addprefix -I ,$(shell find $(APP_SRC_DIRS) $(COMMON_SRC_DIRS) -type d ! -path '*/MCAL/*' ! -path '*/FREERTOS/*')) \
                                 $(MCAL_INCLUDES)

####################################################################################################
# Object File Generation
####################################################################################################
VPATH                        := $(dir $(APP_C_SRCS) $(APP_S_SRCS) $(COMMON_SRCS) $(MCAL_C_SRCS) $(MCAL_S_SRCS))
APP_C_OBJS                   := $(patsubst %.c, $(APP_OBJ_DIR)/%.o, $(notdir $(APP_C_SRCS)))
APP_S_OBJS                   := $(patsubst %.S, $(APP_OBJ_DIR)/%.o, $(notdir $(APP_S_SRCS)))
COMMON_OBJS                  := $(patsubst %.c, $(COMMON_OBJ_DIR)/%.o, $(notdir $(COMMON_SRCS)))
MCAL_C_OBJS                  := $(patsubst %.c, $(MCAL_OBJ_DIR)/%.o, $(notdir $(MCAL_C_SRCS)))
MCAL_S_OBJS                  := $(patsubst %.S, $(MCAL_OBJ_DIR)/%.o, $(notdir $(MCAL_S_SRCS)))
TARGET_DEPS                  := $(APP_C_OBJS:.o=.d) $(COMMON_OBJS:.o=.d) $(MCAL_C_OBJS:.o=.d)

####################################################################################################
# Compilation Settings
####################################################################################################
ifeq ($(RELEASE),y)
    APP_OPTIMISATION         := -Os
    COMMON_OPTIMISATION      := -Os
    MCAL_OPTIMISATION        := -Os
else
    APP_OPTIMISATION         := -Og
    COMMON_OPTIMISATION      := -Og
    MCAL_OPTIMISATION        := -Os
endif

# APP vector table offset: APP_CODE_START (APP_FLASH_START + 256-byte app_header). Purely
# cosmetic - system_stm32f10x.c defines VECT_TAB_OFFSET but never reads SCB->VTOR from it; the
# real relocation happens at runtime via MCU_JUMP_set_vector_table() in APP's own main.c, from the
# linker's __isr_vector_start symbol, which always tracks the linker script correctly regardless
# of this define. Kept in sync anyway so it doesn't sit there silently wrong.
CFLAGS += -DVECT_TAB_OFFSET=$(APP_CODE_START)
CFLAGS += $(APP_CFLAGS)

# Linker flags configuration
LDFLAGS := \
    $(LDFLAGS_COMMON) \
    $(APP_OPTIMISATION) \
    --specs=nano.specs \
    --specs=nosys.specs \
    "-Wl,-Map=$(APP_MAP_FILE)" \
    -L$(dir $(APP_LINKER_SCRIPT)) \
    -T$(APP_LINKER_SCRIPT) \
    -o $(APP_ELF_FILE)

####################################################################################################
#                                  Phony Targets Declaration                                       #
####################################################################################################
.PHONY: all rebuild build_clean total_clean app_build app_rebuild gen_lcov_report
.PHONY: $(BM_IMAGE) $(FBL_IMAGE) full_image_with_eol
.PHONY: test_clean test_unit_all test_sw_all cov_report
.PHONY: doxygen doxygen_clean
.PHONY: doc_functions doc_functions_clean
.PHONY: list analyse_app_mapfile tree files
.PHONY: build_banner autoversion warnings_parser memory_stats object_stats
.PHONY: release_package mux_dbc dataset

####################################################################################################
#                      All (Default) Target and other Build Targets                                #
####################################################################################################
MUX_DBC_SCRIPT  := ../scripts/generate_mux_dbc.rb
MUX_DBC_OUTPUT  := ../Tool_cfg/DBC/mux_debug.dbc

####################################################################################################
#                                  Hardware Variant Targets                                        #
####################################################################################################
# Each variant builds into its own Build/<VARIANT> of APP, BM and FBL.
#   app_build            APP only, every variant
#   all                  everything for every variant: BM, FBL, APP, the dataset, the combined image and the flash pack
#   variant_<name>       everything for the one variant
#   app_variant          APP only for the variant of this make (SUPER_PILL unless HW_VARIANT is given)
HW_VARIANTS := SUPER_PILL BLUE_PILL
.PHONY: app_variant variant_super_pill variant_blue_pill variant_image

app_variant: $(APP_ELF_FILE) memory_stats warnings_parser

app_build:
	@for v in $(HW_VARIANTS); do $(MAKE) app_variant HW_VARIANT=$$v || exit 1; done

app_rebuild: build_clean app_build

all: variant_super_pill variant_blue_pill

variant_super_pill:
	$(MAKE) variant_image HW_VARIANT=SUPER_PILL

variant_blue_pill:
	$(MAKE) variant_image HW_VARIANT=BLUE_PILL

gen_lcov_report: cov_report

mux_dbc:
	@ruby $(MUX_DBC_SCRIPT) Src ../xCOMMON_MODULES/Src --ids Src/cfg/MUX_ids.h --output $(MUX_DBC_OUTPUT)

rebuild: app_rebuild

# Build BM first to ensure it's available before APP links/combines
$(BM_IMAGE):
	@$(MAKE) -C ../BM bm_build

$(FBL_IMAGE):
	@$(MAKE) -C ../FBL fbl_build

# Tyre pressure table -> signed DATASET0 hex, downloadable on its own through FBL. Same CRC and
# signing steps as APP, over the dataset's own header/payload addresses. The generator already
# defines every byte of the region, so no separate padding step.
dataset:
	@mkdir -p $(BUILD_OUT_DIR)
	@echo "--> Generating DATASET0 (tyre pressures) from $(notdir $(DATASET0_CSV))..."
	@ruby $(DATASET0_GEN) $(DATASET0_CSV) --output=$(DATASET0_HEX_FILE) --header-addr=$(DATASET0_FLASH_START) --code-start=$(DATASET0_CODE_START) --code-end=$(DATASET0_FLASH_END)
	@$(BUILD_ENV_BIN)/app_crc_injector $(DATASET0_HEX_FILE) --output=$(DATASET0_HEX_WITH_CRC_FILE) --header-addr=$(DATASET0_FLASH_START) --code-start=$(DATASET0_CODE_START) --code-end=$(DATASET0_FLASH_END) --crc-type=$(APP_CRC_TYPE)
	@$(BUILD_ENV_BIN)/app_signer $(DATASET0_HEX_WITH_CRC_FILE) --output=$(DATASET0_HEX_SIGNED_FILE) --header-addr=$(DATASET0_FLASH_START) --code-start=$(DATASET0_CODE_START) --code-end=$(DATASET0_FLASH_END) --algorithm=$(SIGN_ALGORITHM) --signing-key=$(SIGNING_KEY)
	@echo "   $(notdir $(DATASET0_HEX_SIGNED_FILE))   (DATASET0 signed - download via FBL)"

# The full image of the one variant this make is for: BM, FBL, APP and the dataset, the combined image and the flash pack
variant_image: $(BM_IMAGE) $(FBL_IMAGE) app_variant dataset
	@$(SREC) $(BM_IMAGE) -Intel --multiple $(FBL_IMAGE) -Intel --multiple $(APP_HEX_SIGNED_FILE) -Intel --multiple $(DATASET0_HEX_SIGNED_FILE) -Intel --multiple -o $(COMBINED_IMAGE) -intel
	@echo "--> Combined image created: $(COMBINED_IMAGE)"
	@echo "    Contains: BM ($(BM_FLASH_START)-$(BM_FLASH_END)) + FBL ($(FBL_FLASH_START)-$(FBL_FLASH_END)) + APP ($(APP_FLASH_START)-$(APP_FLASH_END)) + DATASET0 ($(DATASET0_FLASH_START)-$(DATASET0_FLASH_END))"
	@echo "--> Generating flash pack..."
	@ruby -S $(FLASH_PACK_SCRIPT) $(FLASH_PACK_PROFILE) $(APP_HEX_SIGNED_FILE) \
		--output=$(FLASH_PACK_OUTPUT) \
		--security=$(FLASH_PACK_SEEDKEY) \
		--sw-version=$(VERSION) \
		|| echo "WARNING: create_flash_pack failed — pack not generated"
	@echo "   $(notdir $(FLASH_PACK_OUTPUT))  (flash pack)"

full_image_with_eol: variant_image
	@echo "--> Injecting EOL data from $(EOL_CONFIG)..."
	@ruby $(EOL_INJECTOR) inject $(COMBINED_IMAGE) $(EOL_CONFIG) --output=$(FINAL_IMAGE_WITH_EOL)
	@echo "--> Final production image: $(realpath $(FINAL_IMAGE_WITH_EOL))"

$(APP_OBJ_DIR)/%.o: %.c
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(APP_OPTIMISATION) $(INCLUDES) -c $< -o $@ 2>&1 | tee -a $(APP_COMPILER_OUTPUT) >&2

$(APP_OBJ_DIR)/%.o: %.S
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(APP_OPTIMISATION) $(INCLUDES) -c $< -o $@ 2>&1 | tee -a $(APP_COMPILER_OUTPUT) >&2

$(COMMON_OBJ_DIR)/%.o: %.c
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(COMMON_OPTIMISATION) $(INCLUDES) -c $< -o $@ 2>&1 | tee -a $(APP_COMPILER_OUTPUT) >&2

$(MCAL_OBJ_DIR)/%.o: %.c
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(MCAL_OPTIMISATION) $(INCLUDES) -c $< -o $@ 2>&1 | tee -a $(APP_COMPILER_OUTPUT) >&2

$(MCAL_OBJ_DIR)/%.o: %.S
	@echo "--> Compiling $(notdir $@)..."
	@$(CC) $(CFLAGS) $(MCAL_OPTIMISATION) $(INCLUDES) -c $< -o $@ 2>&1 | tee -a $(APP_COMPILER_OUTPUT) >&2

# Every object, as a target of its own: the parallel sub-make below is told this one name, not the ~430 paths, which
# alone run to over 12K characters and overflow the command line once the output directory is longer than Build/
.PHONY: all_objects
all_objects: $(APP_C_OBJS) $(APP_S_OBJS) $(COMMON_OBJS) $(MCAL_C_OBJS) $(MCAL_S_OBJS)

$(APP_OBJ_DIR):
	@echo "--> Creating build directories..."
	@mkdir -p $(APP_OBJ_DIR) $(COMMON_OBJ_DIR) $(MCAL_OBJ_DIR)

# Print build banner before any compilation starts
build_banner:
	@echo ""
	@echo "======================================================================"
	@echo "                    Building APP for $(HW_VARIANT), $(JOBS) cores "
	@echo "======================================================================"
	@echo ""
	
$(APP_ELF_FILE): build_banner autoversion $(APP_OBJ_DIR)
	@mkdir -p $(BUILD_OUT_DIR)
	@echo Compiler warnings generated by $(CC) for $(PROJECT_NAME) > $(APP_COMPILER_OUTPUT)
	@$(MAKE) -j$(JOBS) all_objects
	@printf '\n--> Linking Object Files...\n'
	@$(file >$(LINK_OBJECT_LIST),$(APP_C_OBJS) $(APP_S_OBJS) $(COMMON_OBJS) $(MCAL_C_OBJS) $(MCAL_S_OBJS))
	@$(LD) $(LDFLAGS) @$(LINK_OBJECT_LIST)
	@$(OBJCOPY) -O binary $(APP_ELF_FILE) $(APP_BIN_FILE)
	@$(OBJCOPY) -O ihex $(APP_ELF_FILE) $(APP_HEX_FILE)
	@$(SREC) $(APP_HEX_FILE) -Intel -o $(APP_SREC_FILE) -Motorola
	@printf '\n--> Padding HEX to the whole APP region (0xFF fill)...\n'
# BM's CRC and signature cover the whole region [APP_CODE_START, APP_CODE_END]. A hex that stops short
# leaves the previous build's bytes in the tail pages when the flasher rewrites only the pages a hex
# touches, so the CRC no longer matches and BM falls back to FBL. 0xFF is what both tools assume for
# the gap, so the CRC and signature values are unchanged.
	@$(SREC) $(APP_HEX_FILE) -Intel -fill 0xFF $(APP_FLASH_START) $(APP_FLASH_LIMIT) -o $(APP_HEX_PADDED_FILE) -Intel
	@printf '\n--> Injecting CRC32 into APP header (HEX file)...\n'
	@$(BUILD_ENV_BIN)/app_crc_injector $(APP_HEX_PADDED_FILE) --output=$(APP_HEX_WITH_CRC_FILE) --header-addr=$(APP_HEADER_ADDR) --code-start=$(APP_CODE_START) --code-end=$(APP_CODE_END) --crc-type=$(APP_CRC_TYPE)
	@printf '\n--> Signing firmware ($(SIGN_ALGORITHM))...\n'
	@$(BUILD_ENV_BIN)/app_signer $(APP_HEX_WITH_CRC_FILE) --output=$(APP_HEX_SIGNED_FILE) --header-addr=$(APP_HEADER_ADDR) --code-start=$(APP_CODE_START) --code-end=$(APP_CODE_END) --algorithm=$(SIGN_ALGORITHM) --signing-key=$(SIGNING_KEY)
	@echo "--> Converting signed HEX to S19 format..."
	@$(SREC) $(APP_HEX_SIGNED_FILE) -Intel -o $(APP_S19_SIGNED_FILE) -Motorola -address-length=4
	@build_timer $(BUILD_OUT_DIR)/build_time.csv $(START_TIME)
	@echo "--> Output files generated in: $(realpath $(BUILD_OUT_DIR))"
	@echo "   FLASH THIS: $(notdir $(APP_HEX_SIGNED_FILE)) (Intel HEX) or $(notdir $(APP_S19_SIGNED_FILE)) (Motorola S19), both signed"
	@echo ""
	
build_clean:
	@rm -fr Build $(AUTOVERSION_FILE)
	@echo "--> $(PROJECT_NAME) Build Cleaned..."

####################################################################################################
#                                  Ceedling test target & rules                                    #
####################################################################################################
# YML FILES for test configuration - run from test/
UNIT_TEST_YML 	 := unit_project.yml
SW_TEST_YML 	 := sw_test_project.yml
CEEDLING_COV_DIR := test/CodeCoverage

# Define unwanted file patterns for coverage exclusion (as separate excludes)
EXCLUDE_TEST     := --exclude '.*test_.*'
EXCLUDE_TMD      := --exclude '.*TMD_.*'
EXCLUDE_UNITY    := --exclude '.*unity.*'
EXCLUDE_TEST_DIR := --exclude '.*/test/.*'
GCOVR_EXCLUDES   := $(EXCLUDE_TEST) $(EXCLUDE_TMD) $(EXCLUDE_UNITY) $(EXCLUDE_TEST_DIR)

# Where gcovr looks for coverage data: Ceedling's own build, and the instrumented library's objects when there are any
GCOVR_SEARCH     := test/build

$(CEEDLING_COV_DIR):
	@mkdir -p $(CEEDLING_COV_DIR)

# Coverage report generation - works for both single tests and all tests
cov_report: $(CEEDLING_COV_DIR)
	@echo "--> Generating Code Coverage Report..."
	@$(GCOVR_EXE) $(GCOVR_EXCLUDES) --root .. --txt-metric branch --merge-mode-functions=merge-use-line-0 --html-details --gcov-ignore-errors=no_working_dir_found -o $(CEEDLING_COV_DIR)/coverage.html $(GCOVR_SEARCH)
	@$(GCOVR_EXE) $(GCOVR_EXCLUDES) --root .. --txt-metric branch --merge-mode-functions=merge-use-line-0 --cobertura-pretty --gcov-ignore-errors=no_working_dir_found --cobertura $(CEEDLING_COV_DIR)/example_cobertura.xml $(GCOVR_SEARCH)
	@echo "--> Launching HTML..."
	@-cd $(CEEDLING_COV_DIR) && $(OPEN_CMD) coverage.html 2>/dev/null || true

# Cleans up any generated files from tests
test_clean: 
	@echo "--> Cleaning test artifacts..."
	@-cd test && ceedling --project=$(UNIT_TEST_YML) clobber > /dev/null 2>&1
	@-cd test && ceedling --project=$(SW_TEST_YML) clobber > /dev/null 2>&1
	@$(MAKE) -C test/build_system clean
	@rm -fr $(CEEDLING_COV_DIR)
	@echo "--> Test Cleaned!"

# make test_X.test runs one test - the form the VS Code task uses. A unit test also gets its coverage report.
# A common-module unit test is run by xCOMMON_MODULES' own Makefile; an APP unit test (test/unit_tests, UNIT_TEST_YML,
# none yet), or else a software test (test/sw_tests, SW_TEST_YML), is run here. Ceedling reports an unknown name.
# A software test is an end to end test of the whole APP, not a measure of one module: no coverage is shown for it
%.test: $(CEEDLING_COV_DIR)
	@if [ -f ../xCOMMON_MODULES/test/unit_tests/$*.c ]; then \
	  $(MAKE) -C ../xCOMMON_MODULES $@; \
	elif [ -f test/unit_tests/$*.c ]; then \
	  ( cd test && ceedling --project=$(UNIT_TEST_YML) gcov:$*.c ) && $(MAKE) cov_report; \
	else \
	  ( cd test && ceedling --project=$(SW_TEST_YML) test:$*.c ); \
	fi

# All unit tests: the common modules' (xCOMMON_MODULES/test/unit_tests), run by their own Makefile, then APP's own
# (test/unit_tests). Fails if any does.
test_unit_all:
	@echo "Running all unit tests"
	@$(MAKE) -C ../xCOMMON_MODULES test_all
	@cd test && ceedling --project=$(UNIT_TEST_YML) gcov:all
	@$(MAKE) cov_report

# All software tests (no coverage: see %.test)
test_sw_all:
	@echo "Running all software tests"
	@cd test && ceedling --project=$(SW_TEST_YML) test:all

####################################################################################################
#                                  Doxygen targets & rules                                         #
####################################################################################################
DOXYGEN_CONFIG := ../Tool_cfg/Doxygen/Doxygen_config
DOXYGEN_EXE    := doxygen
DOXYGEN_OUTPUT := doc/doxygen

doxygen:
	@echo "--> Running Doxygen..."
	@mkdir -p $(DOXYGEN_OUTPUT)
	@$(DOXYGEN_EXE) -b $(DOXYGEN_CONFIG)
	@cd $(DOXYGEN_OUTPUT) && find -type f \( -name '*.md5' -o -name '*.map' -o -name '*globals*' -o -name '*dir_*' \) -print0 | xargs -0 rm -f
	@-$(OPEN_CMD) doc/doxygen/html/index.html 2>/dev/null || true

# Cleans up any generated files from doxygen
doxygen_clean:
	@echo "--> Cleaning doxygen..."
	@rm -rf $(DOXYGEN_OUTPUT)/*

####################################################################################################
#                                  Miscellaneous targets & rules                                   #
####################################################################################################
# Every target named in the makefiles, pattern rules like %.test included
list:
	@echo "--> Available Targets:"
	@grep -hoiE '^[a-z%][a-z0-9_.%-]*:' $(filter-out %.d,$(MAKEFILE_LIST)) | tr -d : | sort -u

analyse_app_mapfile: $(APP_MAP_FILE)
	@sed '/debug/d' $(APP_MAP_FILE) > $(APP_MAP_FILE)_stripped
	@amap $(APP_MAP_FILE)_stripped

# From the repo root (the parent of APP), where file_list.txt is written too
tree:
	@cd .. && cmd //c "tree /a" | tee file_list.txt

files:
	@cd .. && cmd //c "tree /a /f" | tee file_list.txt

autoversion:
	@sVersion -a Src --yml project.yml && echo "--> Generating autoversion header file..."

$(APP_MAP_FILE): $(APP_ELF_FILE)

total_clean: test_clean build_clean doxygen_clean
	@$(MAKE) -C ../BM bm_clean
	@$(MAKE) -C ../FBL fbl_clean
	@rm -fr Build ../BM/Build ../FBL/Build
	@echo "--> Total clean complete!"

warnings_parser: $(APP_ELF_FILE)
	@cc_warning_parser $(APP_COMPILER_OUTPUT)

# The README memory tables and the memory badges are written by CI, which commits them: a local build leaves them alone.
# UPDATE_README=y refreshes them locally.
README_ARGS := $(if $(or $(CI),$(UPDATE_README)),--readme ../README.md --tag APP)

memory_stats: $(APP_ELF_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(APP_MAP_FILE) $(APP_ELF_FILE) $(APP_LINKER_SCRIPT) --out $(APP_MEM_OUTPUT_FILE) --csv $(APP_MEM_CSV_FILE) $(README_ARGS)

# A table per object file listing every function and its size. Separate from memory_stats as it adds several seconds.
object_stats: $(APP_ELF_FILE)
	@$(BUILD_ENV_BIN)/obj_size_analyser $(APP_OBJ_DIR) --out $(OBJECT_SIZE_FILE) > /dev/null
	@echo "--> Per-function sizes written to $(OBJECT_SIZE_FILE)"

release_package:
	@echo "======================================================================"
	@echo "--> Creating release package $(VERSION)..."
	@echo "======================================================================"
	@-rm -fr $(RELEASE_PACKAGE_BASE_DIR)
	@mkdir -p $(RELEASE_PACKAGE_NAME) $(RELEASE_PACKAGE_NAME)/reports $(RELEASE_PACKAGE_NAME)/function_docs
	@echo "--> Step 1/8: Clean build environment..." 
	@$(MAKE) total_clean
	@echo ""
	@echo "--> Step 2/8: Building every variant (BM, FBL, APP, dataset, images)..."
	@$(MAKE) all
	@echo ""
	@echo "--> Step 3/8: Generating memory statistics..."
	@$(MAKE) memory_stats >/dev/null 2>&1
	@echo ""
	@echo "--> Step 4/8: Generating Doxygen documentation..."
	@-$(MAKE) doxygen || echo "Warning: doxygen step failed - skipping"
	@echo ""
	@echo "--> Step 5/8: Generating function documentation..."
	@-$(MAKE) doc_functions || echo "Warning: doc_functions step failed - skipping"
	@echo ""
	@echo "--> Step 6/8: Generating cyclomatic complexity analysis..."
	@ruby -S complexity_scanner . --threshold=0 --sort --html=$(RELEASE_PACKAGE_NAME)/reports/complexity_report.html --csv=$(RELEASE_PACKAGE_NAME)/reports/complexity_report.csv --exclude=test,FREERTOS,Build,build 2>&1 || echo "Warning: complexity_scanner not available"
	@echo ""
	@echo "--> Step 7/8: Copying build artifacts..."
	@cp -r $(BUILD_OUT_DIR)/. $(RELEASE_PACKAGE_NAME) 2>/dev/null || echo "Warning: build artifact copy failed (build may have been incomplete)"
	@-find $(RELEASE_PACKAGE_NAME)/object_files/ -type f -name '*.d' -delete -o -name '*.su' -delete 2>/dev/null || true
	@-cp -r $(DOXYGEN_OUTPUT) $(RELEASE_PACKAGE_NAME)
	@echo "--> Step 8/8: Generating comprehensive analysis reports..."
	@echo "   --> Source code statistics..."
	@ruby -S source_line_counter --dir Src ../Common_Src ../xCOMMON_MODULES/Src > $(RELEASE_PACKAGE_NAME)/reports/source_statistics.txt 2>&1 || echo "Warning: source_line_counter not available"
	@echo "   --> TODO/FIXME analysis..."
	@ruby -S code_marker_scanner --html=$(RELEASE_PACKAGE_NAME)/reports/code_markers.html --stats Src ../Common_Src ../xCOMMON_MODULES/Src > $(RELEASE_PACKAGE_NAME)/reports/code_markers_summary.txt 2>&1 || echo "Warning: code_marker_scanner not available"
	@echo "   --> Copying analysis reports..."
	@-cp $(APP_COMPILER_OUTPUT) $(RELEASE_PACKAGE_NAME)/reports/compiler_warnings.txt 2>/dev/null
	@-cp $(APP_MEM_OUTPUT_FILE) $(RELEASE_PACKAGE_NAME)/reports/memory_analysis_full.txt 2>/dev/null
	@-cp $(OBJECT_SIZE_FILE) $(RELEASE_PACKAGE_NAME)/reports/object_sizes.txt 2>/dev/null
	@-cp build_time.csv $(RELEASE_PACKAGE_NAME)/reports/build_time.csv 2>/dev/null
	@echo "   --> Copying function documentation..."
	@-find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.html' -exec cp {} $(RELEASE_PACKAGE_NAME)/function_docs/ \; 2>/dev/null
	@-find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.docx' -exec cp {} $(RELEASE_PACKAGE_NAME)/function_docs/ \; 2>/dev/null
	@-cp version.svg $(RELEASE_PACKAGE_NAME)/ 2>/dev/null
	@-cp ../.github/badges/version.svg $(RELEASE_PACKAGE_NAME)/ 2>/dev/null
	@-cp ../README.md $(RELEASE_PACKAGE_NAME)/
	@echo "   --> Generating checksums..."
	@-find $(RELEASE_PACKAGE_NAME) -type f -print0 | sort -z | xargs -0 sha1sum > $(RELEASE_PACKAGE_NAME)/chksum.txt 2>/dev/null || echo "Warning: checksum generation failed"
	@echo "   --> Creating ZIP archive..."
	@cd $(RELEASE_PACKAGE_BASE_DIR) && $(ZIP) a "$(notdir $(RELEASE_PACKAGE_NAME)).zip" ./* > /dev/null || echo "Warning: ZIP creation failed"
	@-rm -fr $(RELEASE_PACKAGE_NAME)
	@echo ""
	@echo "======================================================================"
	@echo "✅ Release package step completed (check warnings above for any skipped steps)"
	@echo "======================================================================"
	@echo "Location: $(RELEASE_PACKAGE_BASE_DIR)/$(notdir $(RELEASE_PACKAGE_NAME)).zip"
	@echo ""

####################################################################################################
# Documentation Generation
####################################################################################################
# Every C file of the project and the common modules. xargs batches the list (all at once is far over the
# Windows command-line limit); only the tool's failure lines are shown, it prints several lines per file
doc_functions:
	@echo "Generating function documentation for all C files..."
	@find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*.c' ! -path '*/test/*' ! -path '*/FREERTOS/*' \
	  ! -path '*/CRYPTO/BearSSL/*' ! -path '*/CRYPTO/micro-ecc/*' ! -path '*/CRYPTO/ED25519/*' \
	  ! -path '*/JSON/tinyjson.c' ! -path '*/stm32g4xx/*' \
	  | xargs -s 6000 c_function_documenter | grep -E 'Error|Failed' || true

doc_functions_clean:
	@echo "--> Cleaning function documentation files..."
	@find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.html' -delete 2>/dev/null
	@find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.docx' -delete 2>/dev/null
	@echo "--> Documentation files cleaned!"

-include $(TARGET_DEPS)