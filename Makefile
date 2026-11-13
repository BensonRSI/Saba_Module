
# Parameters
#CPM65_PATH = ../cpm65
#EXTRA_CMAKE_ARGS += -DCMAKE_VERBOSE_MAKEFILE=ON

$(info This Makefile is not required and for convenience only)

ifeq ($(PICO_SDK_PATH),)
  ifneq ($(wildcard $(CURDIR)/build/_deps/pico_sdk-src/README.md),)
    PICO_SDK_PATH := $(CURDIR)/build/_deps/pico_sdk-src
  else ifneq ($(wildcard $(CURDIR)/build/pico-sdk/README.md),)
    PICO_SDK_PATH := $(CURDIR)/build/pico-sdk
  else ifneq ($(wildcard $(CURDIR)/submodules/pico-sdk/README.md),)
    PICO_SDK_PATH := $(CURDIR)/submodules/pico-sdk
  else ifneq ($(wildcard $(CURDIR)/../submodules/pico-sdk/README.md),)
    PICO_SDK_PATH := $(CURDIR)/../submodules/pico-sdk
  else
    PICO_SDK_PATH := $(shell readlink -f ../submodules/pico-sdk 2>/dev/null || true)
  endif
endif

ifneq ($(PICO_SDK_PATH),)
$(info Using local pico sdk at: $(PICO_SDK_PATH))
else
$(info PICO_SDK_PATH is not set and no local Pico SDK was found. Set PICO_SDK_PATH explicitly.)
endif

EXTRA_CMAKE_ARGS += -DPICO_SDK_PATH="$(PICO_SDK_PATH)"

ifeq ($(PICO_FLASHTOOL_PATH),)
  ifneq ($(wildcard $(CURDIR)/build/_deps/picotool-src/CMakeLists.txt),)
    PICO_FLASHTOOL_PATH := $(CURDIR)/build/_deps/picotool-src
  else ifneq ($(wildcard $(CURDIR)/build/picotool/CMakeLists.txt),)
    PICO_FLASHTOOL_PATH := $(CURDIR)/build/picotool
  else ifneq ($(wildcard $(CURDIR)/tools/picotool/CMakeLists.txt),)
    PICO_FLASHTOOL_PATH := $(CURDIR)/tools/picotool
  else
    PICO_FLASHTOOL_PATH := $(shell readlink -f $(CURDIR)/../picotool 2>/dev/null || true)
  endif
endif

ifneq ($(PICO_FLASHTOOL_PATH),)
$(info Using local picotool source at: $(PICO_FLASHTOOL_PATH))
else
$(info PICO_FLASHTOOL_PATH is not set and no local picotool source was found.)
endif

BUILD_DIR ?= $(CURDIR)/build
BUILD_TOOL_DIR ?= $(CURDIR)/build/tools
SRC_DIR := $(CURDIR)/src
JOBS ?= 4
DEFAULT_TARGET ?= $(BUILD_DIR)/eprom_emulator.uf2

.PHONY: all clean distclean release picotool xmodem_upload /usr/local/bin/picotool
 
all: $(PICO_SDK_PATH)/README.md xmodem_upload
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(EXTRA_CMAKE_ARGS)
	make -C $(BUILD_DIR) -j$(JOBS) && echo "\nbuild was successful\n"


xmodem_upload: tools/xmodem_upload/xmodem_upload.cpp
	cmake -S tools/xmodem_upload -B $(BUILD_TOOL_DIR)/xmodem_upload_build
	make -C $(BUILD_TOOL_DIR)/xmodem_upload_build -j$(JOBS)
	
go:	/usr/local/bin/picotool all	
	picotool load -f $(DEFAULT_TARGET)
	picotool reboot -f

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
	cmake -S $(PICO_FLASHTOOL_PATH) -B $(BUILD_TOOL_DIR) -DPICO_SDK_PATH="$(PICO_SDK_PATH)" -DPICOTOOL_NO_LIBUSB=0
	make -C $(BUILD_TOOL_DIR) -j$(JOBS) && echo "\nbuild of picotool was successful\n"
	sudo make -C $(BUILD_TOOL_DIR) install

distclean:
	rm -rf $(BUILD_DIR)
