# --- Demo build: LoRa RX only (no SIM7000E modem) ---
# Usage:
#   cd gateway-lora-receiver/SWSD003/lr11xx/apps/p2p_receiver/demo
#   make            # build
#   make flash      # build + flash via ST-LINK

######################################
# target
######################################
TOP_DIR = ../../../..

APP = p2p_receiver_demo

PROJECTS_COMMON_MAKEFILE = $(TOP_DIR)/lr11xx/common/apps_common.mk

# LR1121 shield has NO TCXO — must override SDK default (LR1110MB1DIS has TCXO).
# Without this, calibrate(0x3F) hangs BUSY indefinitely.
# See: reports/lr1121-calibration-hang.md in the main repo.
RADIO_SHIELD = LR1121MB1DIS

######################################
# building variables
######################################
DEBUG ?= 1
OPT ?= -O0

#######################################
# paths
#######################################
BUILD_DIR = ./build

######################################
# source — NO sim7000e files
######################################
C_SOURCES = \
../main_demo.c \
../payload_parser.c

C_DEFS =

# Add parent dir so main_p2p_receiver.h and payload_parser.h are found
C_INCLUDES += -I..

#######################################
# include
#######################################
include $(PROJECTS_COMMON_MAKEFILE)

#######################################
# Flash helper (ST-LINK SWD)
#######################################
STM32_PROG ?= "C:/ST/STM32CubeIDE_1.18.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.100.202412061334/tools/bin/STM32_Programmer_CLI.exe"

.PHONY: all target flash

all: target

target: $(BUILD_DIR)/$(APP).elf $(BUILD_DIR)/$(APP).bin

.DEFAULT_GOAL:= target

## For the main application
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c $(MAKEFILE_LIST) | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s $(MAKEFILE_LIST) | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(APP).elf: $(OBJECTS) Makefile | $(BUILD_DIR)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

flash: target
	$(STM32_PROG) --connect port=SWD --download $(BUILD_DIR)/$(APP).bin 0x08000000 --start

print-%  : ; @echo $* = $($*)

#######################################
# clean up
#######################################
clean:
	rm -fR $(BUILD_DIR)

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
