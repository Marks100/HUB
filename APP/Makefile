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
APP_HEX_WITH_CRC_FILE        := $(OUTPUT_NAME)_with_CRC.hex
APP_HEX_SIGNED_FILE          := $(OUTPUT_NAME)_signed.hex
APP_S19_SIGNED_FILE          := $(OUTPUT_NAME)_signed.s19
OBJECT_SIZE_FILE             := $(BUILD_OUT_DIR)/object_sizes.txt
APP_COMPILER_OUTPUT          := $(BUILD_OUT_DIR)/compiler_warnings.txt
APP_MEM_OUTPUT_FILE          := $(BUILD_OUT_DIR)/memory_analysis.txt
APP_MEM_CSV_FILE             := $(BUILD_OUT_DIR)/memory_analysis.csv
AUTOVERSION_FILE             := Src/autoversion.h
RELEASE_PACKAGE_NAME         := $(RELEASE_PACKAGE_BASE_DIR)/$(PROJECT_TYPE)_$(VERSION)

# Flash memory partition layout — must match the linker scripts in BM/, FBL/, APP/
# STM32F103C8T6: 64KB total flash (0x10000) starting at 0x08000000, 1KB pages
# BM=4KB  FBL=16KB  APP=44KB — adjust sizes and update linker scripts accordingly
BM_FLASH_START               := 0x08000000
BM_FLASH_SIZE                := 0x1000
BM_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) - 1 )))

FBL_FLASH_START               := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) )))
FBL_FLASH_SIZE                := 0x4000
FBL_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) - 1 )))

APP_FLASH_START               := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) )))
APP_FLASH_SIZE                := 0xB000
APP_FLASH_END                 := $(shell printf '0x%08X' $$(( $(BM_FLASH_START) + $(BM_FLASH_SIZE) + $(FBL_FLASH_SIZE) + $(APP_FLASH_SIZE) - 1 )))

# TODO: Set APP_HEADER_ADDR and APP_CODE_START to match your linker script APP section layout
APP_HEADER_ADDR              := $(APP_FLASH_START)
APP_CODE_START               := $(shell printf '0x%08X' $$(( $(APP_FLASH_START) + 0x100 )))
APP_CODE_END                 := $(APP_FLASH_END)

BM_IMAGE                     := ../BM/$(BUILD_OUT_DIR)/BM.hex
FBL_IMAGE                    := ../FBL/$(BUILD_OUT_DIR)/FBL.hex
COMBINED_IMAGE               := $(BUILD_OUT_DIR)/BM_FBL_APP_combined.hex

# EOL (End-of-Line) data injection
EOL_CONFIG                   := ../Tool_cfg/EOL/eol_config.yml
EOL_INJECTOR                 := ../Tool_cfg/EOL/inject_eol_data

# Signing keys
SIGNING_KEY                  := ../Tool_cfg/SigningKeys/signing_key.pem
FINAL_IMAGE_WITH_EOL         := $(BUILD_OUT_DIR)/BM_FBL_APP_with_EOL.hex

# Flash pack
FLASH_PACK_SCRIPT            := ../scripts/create_flash_pack
FLASH_PACK_PROFILE           := ../Tool_cfg/CANFLASH/erx2_slp3.json
FLASH_PACK_SEEDKEY           := ../Tool_cfg/CANFLASH/seedkey.dll
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
COMMON_SRCS                  := $(shell find $(COMMON_SRC_DIRS) -type f -name '*.c' ! -path '*/test/*' ! -path '*/MCAL/*' ! -path '*/CRYPTO/*' ! -path '*/xCOMMON_MODULES/cfg/*' ! -path '*/JSON/*')

# Include paths - add "-I" prefix to all discovered directories
# Exclude all MCAL subdirs from the broad search; MCAL headers come exclusively from MCAL_INCLUDES
INCLUDES                     := $(addprefix -I ,$(shell find $(APP_SRC_DIRS) $(COMMON_SRC_DIRS) -type d ! -path '*/MCAL/*')) \
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
.PHONY: all rebuild build_clean total_clean app_build app_rebuild test_all gen_lcov_report
.PHONY: $(BM_IMAGE) $(FBL_IMAGE) full_image full_image_with_eol
.PHONY: test_clean test_unit_all test_int_all insert_test_headers
.PHONY: doxygen doxygen_clean
.PHONY: doc_functions doc_functions_clean
.PHONY: no_targets__ list analyse_app_mapfile print_file_list
.PHONY: warnings_parser basic_memory_stats full_memory_stats memory_badges test_badges
.PHONY: release_package force_ceedling_relink mux_dbc

####################################################################################################
#                      All (Default) Target and other Build Targets                                #
####################################################################################################
MUX_DBC_SCRIPT  := ../scripts/generate_mux_dbc.rb
MUX_DBC_OUTPUT  := ../Tool_cfg/DBC/mux_debug.dbc

app_build: $(APP_ELF_FILE) basic_memory_stats warnings_parser
app_rebuild: build_clean app_build

gen_lcov_report: cov_report

mux_dbc:
	@ruby $(MUX_DBC_SCRIPT) Src ../xCOMMON_MODULES/Src --ids Src/cfg/MUX_ids.h --output $(MUX_DBC_OUTPUT)

rebuild: build_clean
	$(MAKE) app_build

# Build BM first to ensure it's available before APP links/combines  
$(BM_IMAGE):
#@$(MAKE) -j $(MAKE_FLAGS) -C ../BM bm_build

# Build FBL to ensure it's available before APP links/combines
$(FBL_IMAGE):
#@$(MAKE) -j $(MAKE_FLAGS) -C ../FBL fbl_build

all: $(BM_IMAGE) $(FBL_IMAGE) app_build
	@$(SREC) $(BM_IMAGE) -Intel --multiple $(FBL_IMAGE) -Intel --multiple $(APP_HEX_WITH_CRC_FILE) -Intel --multiple -o $(COMBINED_IMAGE) -intel
	@echo "--> Combined image created: $(COMBINED_IMAGE)"
	@echo "    Contains: BM ($(BM_FLASH_START)-$(BM_FLASH_END)) + FBL ($(FBL_FLASH_START)-$(FBL_FLASH_END)) + APP ($(APP_FLASH_START)-$(APP_FLASH_END))"
	@echo "--> Generating flash pack..."
	@ruby -S $(FLASH_PACK_SCRIPT) $(FLASH_PACK_PROFILE) $(APP_HEX_SIGNED_FILE) \
		--output=$(FLASH_PACK_OUTPUT) \
		--security=$(FLASH_PACK_SEEDKEY) \
		--sw-version=$(VERSION) \
		|| echo "WARNING: create_flash_pack failed — pack not generated"
	@echo "   $(notdir $(FLASH_PACK_OUTPUT))  (flash pack)"
	@$(MAKE) $(MAKE_FLAGS) memory_badges

full_image_with_eol: full_image
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

$(APP_OBJ_DIR):
	@echo "--> Creating build directories..."
	@mkdir -p $(APP_OBJ_DIR) $(COMMON_OBJ_DIR) $(MCAL_OBJ_DIR)

# Print build banner before any compilation starts
build_banner:
	@echo ""
	@echo "======================================================================"
	@echo "                    Building APP (Application) $(JOBS) cores "
	@echo "======================================================================"
	@echo ""
	
$(APP_ELF_FILE): build_banner autoversion $(APP_OBJ_DIR) #$(BearSSL_LIB)
	@mkdir -p $(BUILD_OUT_DIR)
	@echo Compiler warnings generated by $(CC) for $(PROJECT_NAME) > $(APP_COMPILER_OUTPUT)
	@$(MAKE) -j$(JOBS) $(MAKE_FLAGS) $(APP_C_OBJS) $(APP_S_OBJS) $(COMMON_OBJS) $(MCAL_C_OBJS) $(MCAL_S_OBJS)
	@echo ""
	@echo "--> Linking Object Files..."
	@$(LD) $(LDFLAGS) $(APP_C_OBJS) $(APP_S_OBJS) $(COMMON_OBJS) $(MCAL_C_OBJS) $(MCAL_S_OBJS) $(BearSSL_LIB)
	@$(OBJCOPY) -O binary $(APP_ELF_FILE) $(BUILD_OUT_DIR)/$(PROJECT_NAME).bin
	@$(OBJCOPY) -O ihex $(APP_ELF_FILE) $(APP_HEX_FILE)
	@echo "--> Filling gaps in HEX file with 0xFF (from code end to EOL section start)..."
	@$(SREC) $(APP_HEX_FILE) -Intel --fill 0xFF $(APP_CODE_START) $(APP_CODE_END) -Output $(APP_HEX_FILE) -Intel -address-length=4
	@$(SREC) $(APP_HEX_FILE) -Intel -o $(BUILD_OUT_DIR)/$(PROJECT_NAME).srec -Motorola
	@echo ""
	@echo "--> Injecting CRC32 into APP header (HEX file)..."
	@$(BUILD_ENV_BIN)/app_crc_injector $(APP_HEX_FILE) --output=$(APP_HEX_WITH_CRC_FILE) --header-addr=$(APP_HEADER_ADDR) --code-start=$(APP_CODE_START) --code-end=$(APP_CODE_END)
	@echo ""
	@echo "--> Signing firmware with ECDSA..."
	@$(BUILD_ENV_BIN)/app_signer $(APP_HEX_WITH_CRC_FILE) $(APP_HEX_SIGNED_FILE)
	@echo "--> Converting signed HEX to S19 format..."
	@$(SREC) $(APP_HEX_SIGNED_FILE) -Intel -o $(APP_S19_SIGNED_FILE) -Motorola -address-length=4
	@build_timer $(BUILD_OUT_DIR)/build_time.csv $(START_TIME)
	@echo "--> Output files generated in: $(realpath $(BUILD_OUT_DIR))"
	@echo "   $(notdir $(APP_ELF_FILE))   (executable)"
	@echo "   $(notdir $(APP_HEX_FILE))   (Intel HEX - raw)"
	@echo "   $(notdir $(APP_HEX_WITH_CRC_FILE))   (Intel HEX with CRC)"
	@echo "   $(notdir $(APP_HEX_SIGNED_FILE))   (Intel HEX signed - FLASH THIS!)"
	@echo "   $(notdir $(APP_S19_SIGNED_FILE))   (Motorola S19 signed - FLASH THIS!)"
	@echo "   $(notdir $(APP_BIN_FILE))   (binary)"
	@echo "   $(notdir $(APP_SREC_FILE))  (S-record)"
	@echo "   $(notdir $(APP_MAP_FILE))   (linker map)"
	@echo ""
	
build_clean:
	@rm -fr $(BUILD_OUT_DIR) $(AUTOVERSION_FILE)
	@echo "--> $(PROJECT_NAME) Build Cleaned..."

####################################################################################################
#                                  Ceedling test target & rules                                    #
####################################################################################################
# Library timestamp tracking - forces Ceedling to relink test executables when libraries change
LIBRARY_TIMESTAMP_FILE := test/support/library_timestamp.h

# Update timestamp to force test relink
force_ceedling_relink:
	@mkdir -p $(dir $(LIBRARY_TIMESTAMP_FILE))
	@touch $(LIBRARY_TIMESTAMP_FILE)

# YML FILES for test configuration
UNIT_TEST_YML 	:= unit_project.yml
INT_TEST_YML 	:= int_project.yml

# Various test reports
CEEDLING_COV_DIR					  := test/CodeCoverage
CEEDLING_COV_XML_TEST_REPORT_DEST     := test/$(CEEDLING_COV_DIR)

# Define unwanted file patterns for coverage exclusion (as separate excludes)
EXCLUDE_TEST    := --exclude '.*test_.*'
EXCLUDE_TMD     := --exclude '.*TMD_.*'
EXCLUDE_UNITY   := --exclude '.*unity.*'
GCOVR_EXCLUDES  := $(EXCLUDE_TEST) $(EXCLUDE_TMD) $(EXCLUDE_UNITY)

# Function to detect test type based on file location
define detect_test_type
$(if $(wildcard test/unit_tests/$(1).c ../*/test/unit_tests/$(1).c),unit,integration)
endef

# Function to get the correct YML file
define get_test_yml
$(if $(filter unit,$(call detect_test_type,$(1))),$(UNIT_TEST_YML),$(INT_TEST_YML))
endef

define set_test_vars
  	$(eval TEST_FILE := $(1))
  	$(eval TEST_TYPE := $(call detect_test_type,$(basename $(1))))
  	$(eval TEST_YML := $(call get_test_yml,$(basename $(1))))
  	$(info --> Detected $(TEST_TYPE) test with coverage: $(TEST_FILE))
	$(info --> Using config: $(TEST_YML))
endef

$(CEEDLING_COV_XML_TEST_REPORT_DEST):
	@mkdir -p $(CEEDLING_COV_XML_TEST_REPORT_DEST)

$(CEEDLING_COV_DIR):
	@mkdir -p $(CEEDLING_COV_DIR)

# BearSSL MinGW library for test environment
$(BearSSL_MINGW_LIB):
	@echo "Building BearSSL MinGW library for tests..."
	@$(MAKE) $(MAKE_FLAGS) -C ../BearSSL -f Makefile.mingw.mk all
	@$(MAKE) $(MAKE_FLAGS) force_ceedling_relink

# BearSSL ARM library for embedded target
$(BearSSL_LIB):
	@echo "Building BearSSL ARM library for embedded target..."
	@$(MAKE) $(MAKE_FLAGS) -C ../BearSSL -f Makefile.arm.mk all

# mbedTLS MinGW library for test environment
$(MBEDTLS_MINGW_LIB):
	@echo "Building mbedTLS MinGW library for tests..."
	@$(MAKE) $(MAKE_FLAGS) -C ../mbedtls-official-3.6.4 -f Makefile.mingw.mk all
	@$(MAKE) $(MAKE_FLAGS) force_ceedling_relink

TEST_REPORT_HTML      := test/test_report.html
BADGE_OUTPUT_DIR      := ../.github/badges

# Coverage report generation - works for both single tests and all tests
cov_report: $(CEEDLING_COV_DIR)
	@echo "--> Generating Code Coverage Report..."
	@$(GCOVR_EXE) $(GCOVR_EXCLUDES) --root .. --txt-metric branch --merge-mode-functions=merge-use-line-0 --html-details --gcov-ignore-errors=no_working_dir_found -o $(CEEDLING_COV_DIR)/coverage.html test/build
	@$(GCOVR_EXE) --root .. --txt-metric branch --merge-mode-functions=merge-use-line-0 --cobertura-pretty --gcov-ignore-errors=no_working_dir_found --cobertura $(CEEDLING_COV_DIR)/example_cobertura.xml test/build
	@-$(MAKE) test_badges
	@echo "--> Launching HTML..."
	@-cd $(CEEDLING_COV_DIR) && $(OPEN_CMD) coverage.html 2>/dev/null || true

# Parse test_report.html and generate test count badges
.PHONY: test_badges
test_badges:
	@ruby ../scripts/generate_test_badge.rb $(TEST_REPORT_HTML) $(BADGE_OUTPUT_DIR) || echo "--> test_badges failed"

# Cleans up any generated files from tests
test_clean: 
	@echo "--> Cleaning test artifacts..."
	@-cd test && ceedling --project=$(UNIT_TEST_YML) clobber > /dev/null 2>&1
	@-cd test && ceedling --project=$(INT_TEST_YML) clobber > /dev/null 2>&1
	@rm -fr $(CEEDLING_COV_XML_TEST_REPORT_DEST) $(CEEDLING_COV_DIR)
	@echo "--> Test Cleaned!"

# Inserts/updates auto-managed headers in all int test files from the template
.PHONY: insert_test_headers
insert_test_headers:
	@echo "--> Inserting test headers from template..."
	@ruby -S test_header_inserter --template test/int_tests/header_template.txt --test-dir test/int_tests

# Universal test runner - automatically detects unit vs integration
test_%:
	@$(call set_test_vars,test_$*.c)
	@if [ "$(TEST_TYPE)" = "integration" ]; then $(MAKE) insert_test_headers; $(MAKE) autoversion; $(MAKE) $(BearSSL_MINGW_LIB); $(MAKE) $(MBEDTLS_MINGW_LIB); fi
	@cd test && ceedling --project=$(TEST_YML) test:$(TEST_FILE)

# Universal test with coverage - automatically detects unit vs integration
test_%_cov: $(CEEDLING_COV_DIR)
	@$(call set_test_vars,test_$(patsubst %_cov,%,$*).c)
	@if [ "$(TEST_TYPE)" = "integration" ]; then $(MAKE) insert_test_headers; $(MAKE) autoversion; $(MAKE) $(BearSSL_MINGW_LIB); $(MAKE) $(MBEDTLS_MINGW_LIB); fi
	@cd test && ceedling --project=$(TEST_YML) gcov:$(TEST_FILE)
	@$(MAKE) cov_report

# All unit tests
test_unit_all: test_clean $(CEEDLING_COV_DIR)
	@echo "Running all unit tests"
	@-cd test && ceedling --project=$(UNIT_TEST_YML) gcov:all
	@$(MAKE) cov_report

# All integration tests
test_int_all: test_clean insert_test_headers autoversion $(BearSSL_MINGW_LIB) $(MBEDTLS_MINGW_LIB)
	@echo "Running all integration tests"
	@cd test && ceedling --project=$(INT_TEST_YML) gcov:all
	@$(MAKE) cov_report

# user fn check overall test result for Pass \ Fail.  Exits with error code on failure
define check_test_result
    @grep -q "Overall Test Result: PASS" $(CEEDLING_COV_XML_TEST_REPORT_DEST)SoftwareCeedlingTestReport.html \
    && echo "--> Overall Test Result: PASS" || { echo "--> Overall Test Result: FAIL"; exit 1; }
endef

####################################################################################################
#                                  Doxygen targets & rules                                         #
####################################################################################################
DOXYGEN_CONFIG := ../Tool_Cfg/Doxygen/Doxygen_config
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
# Print all targets if no target has been supplied
no_targets__:
list:
	@echo "--> Available Targets:"
	@sh -c "$(MAKE) $(MAKE_FLAGS) -p no_targets__ | awk -F':' '/^[a-zA-Z0-9][^\$$#\/\\t=]*:([^=]|$$)/ {split(\$$1,A,/ /);for(i in A)print A[i]}' | grep -v '__\$$' | sort"

analyse_app_mapfile: $(APP_MAP_FILE)
	@sed '/debug/d' $(APP_MAP_FILE) > $(APP_MAP_FILE)_stripped
	@amap $(APP_MAP_FILE)_stripped

print_file_list:
	@cmd //c "tree /a $(if $(FILE),/f,)" | tee file_list.txt

autoversion:
	@sVersion -a Src --yml project.yml && echo "--> Generating autoversion header file..."

$(APP_MAP_FILE): $(APP_ELF_FILE)

total_clean: test_clean build_clean doxygen_clean
	@$(MAKE) $(MAKE_FLAGS) -C ../BM build_clean
	@$(MAKE) $(MAKE_FLAGS) -C ../FBL clean
	@echo "--> Total clean complete!"

warnings_parser: $(APP_ELF_FILE)
	@cc_warning_parser $(APP_COMPILER_OUTPUT)

basic_memory_stats: $(APP_ELF_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(APP_MAP_FILE) $(APP_ELF_FILE) $(APP_LINKER_SCRIPT) --out $(APP_MEM_OUTPUT_FILE) --csv $(APP_MEM_CSV_FILE)

full_memory_stats: $(APP_MAP_FILE)
	@$(BUILD_ENV_BIN)/memory_analyser_stm32 $(APP_MAP_FILE) $(APP_ELF_FILE) $(APP_LINKER_SCRIPT) --out $(APP_MEM_OUTPUT_FILE) --csv $(APP_MEM_CSV_FILE)

memory_badges:
	@memory_analyser_s32k --badges --root .. --output ../.github/badges --badge-creator "ruby -S badge_creator"
		
release_package:
	@echo "======================================================================"
	@echo "--> Creating release package $(SW_VER)..."
	@echo "======================================================================"
	@-rm -fr $(RELEASE_PACKAGE_BASE_DIR)
	@mkdir -p $(RELEASE_PACKAGE_NAME) $(RELEASE_PACKAGE_NAME)/reports $(RELEASE_PACKAGE_NAME)/function_docs
	@echo "--> Step 1/8: Clean build environment..." 
	@$(MAKE) $(MAKE_FLAGS) total_clean
	@echo ""
	@echo "--> Step 2/8: Building full firmware image..."
	@$(MAKE) $(MAKE_FLAGS) full_image
	@echo ""
	@echo "--> Step 3/8: Generating FULL memory statistics (with object file data)..."
	@$(MAKE) $(MAKE_FLAGS) full_memory_stats >/dev/null 2>&1
	@echo ""
	@echo "--> Step 4/8: Generating test coverage reports..."
	#@$(MAKE) $(MAKE_FLAGS) gen_overall_cov_report
	@echo ""
	@echo "--> Step 5/9: Generating Doxygen documentation..."
	@-$(MAKE) $(MAKE_FLAGS) doxygen || echo "Warning: doxygen step failed - skipping"
	@echo ""
	@echo "--> Step 6/9: Generating function documentation..."
	@-$(MAKE) $(MAKE_FLAGS) doc_functions || echo "Warning: doc_functions step failed - skipping"
	@echo ""
	@echo "--> Step 7/9: Generating cyclomatic complexity analysis..."
	@ruby -S complexity_scanner . --threshold=0 --sort --html=$(RELEASE_PACKAGE_NAME)/reports/complexity_report.html --csv=$(RELEASE_PACKAGE_NAME)/reports/complexity_report.csv --exclude=test,FREERTOS,Build,build 2>&1 || echo "Warning: complexity_scanner not available"
	@echo ""
	@echo "--> Step 8/9: Copying build artifacts..."
	@cp -r $(BUILD_OUT_DIR)/. $(RELEASE_PACKAGE_NAME) 2>/dev/null || echo "Warning: build artifact copy failed (build may have been incomplete)"
	@-find $(RELEASE_PACKAGE_NAME)/object_files/ -type f -name '*.d' -delete -o -name '*.su' -delete 2>/dev/null || true
	@-cp -r $(DOXYGEN_OUTPUT) $(RELEASE_PACKAGE_NAME)
	@echo "--> Step 9/9: Generating comprehensive analysis reports..."
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
.PHONY: doc_functions doc_functions_clean

# Find all C source files in multiple source directories (excluding test and FREERTOS)
DOCUMENTABLE_C_FILES := $(shell find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*.c' ! -path '*/test/*' ! -path '*/FREERTOS/*' 2>/dev/null)

doc_functions:
	@echo "======================================================================"
	@echo "Generating function documentation for all C files..."
	@echo "======================================================================"
	@c_function_documenter $(DOCUMENTABLE_C_FILES)
	@echo ""
	@echo "======================================================================"
	@echo "Documentation generation complete!"
	@echo "======================================================================"

doc_functions_clean:
	@echo "--> Cleaning function documentation files..."
	@find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.html' -delete 2>/dev/null
	@find Src ../Common_Src ../xCOMMON_MODULES/Src -name '*_Documentation.docx' -delete 2>/dev/null
	@echo "--> Documentation files cleaned!"

-include $(TARGET_DEPS) $(CEEDLING_LIB_DEPS)