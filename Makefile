
# Parameters
#CPM65_PATH = ../cpm65
#EXTRA_CMAKE_ARGS += -DCMAKE_VERBOSE_MAKEFILE=ON

$(info This Makefile is not required and for convenience only)

ifeq ($(PICO_SDK_PATH),)
PICO_SDK_PATH=$(shell readlink -f ../submodules/pico-sdk)
$(info Using local pico sdk at: $(PICO_SDK_PATH))
else
$(info Using global pico sdk at: $(PICO_SDK_PATH))
endif
EXTRA_CMAKE_ARGS += -DPICO_SDK_PATH="$(PICO_SDK_PATH)"

BUILD_DIR ?= $(CURDIR)/build
BUILD_TOOL_DIR ?= $(CURDIR)/build/picotool
SRC_DIR := $(CURDIR)/src
JOBS ?= 4
DEFAULT_TARGET ?= $(BUILD_DIR)/eprom_emulator.uf2

.PHONY: all clean distclean release picotool

all: $(PICO_SDK_PATH)/README.md 
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(EXTRA_CMAKE_ARGS)
	make -C $(BUILD_DIR) -j$(JOBS) && echo "\nbuild was successful\n"

go:	/usr/local/bin/picotool all	
	picotool load $(DEFAULT_TARGET)
	picotool reboot

boot: /usr/local/bin/picotool 
	picotool reboot -f

log: $(PICO_SDK_PATH)/README.md
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) -DCMAKE_VERBOSE_MAKEFILE=ON $(EXTRA_CMAKE_ARGS) 2>&1 | tee cmake.log
	make VERBOSE=1 -C $(BUILD_DIR) -j1 2>&1 | tee make.log

clean:
	make -C $(BUILD_DIR) clean	
	rm -rf $(RELEASE_ARCHIVE) cmake.log make.log $(BUILD_DIR)/CMakeFiles
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(EXTRA_CMAKE_ARGS) 

/usr/local/bin/picotool: 
	cmake -S $(PICO_FLASHTOOL_PATH) -B $(BUILD_TOOL_DIR)
	make -C $(BUILD_TOOL_DIR) -j$(JOBS) && echo "\nbuild of picotool was successful\n"
	sudo make -C $(BUILD_TOOL_DIR) install

distclean:
	rm -rf $(BUILD_DIR)
