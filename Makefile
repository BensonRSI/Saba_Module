
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
SRC_DIR := $(CURDIR)/src
JOBS ?= 4

.PHONY: all clean distclean release setup-apt

all: $(PICO_SDK_PATH)/README.md
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) $(EXTRA_CMAKE_ARGS)
	make -C $(BUILD_DIR) -j$(JOBS) && echo "\nbuild was successful\n"

log: $(PICO_SDK_PATH)/README.md
	cmake -S $(SRC_DIR) -B $(BUILD_DIR) -DCMAKE_VERBOSE_MAKEFILE=ON $(EXTRA_CMAKE_ARGS) 2>&1 | tee cmake.log
	make -C $(BUILD_DIR) -j$(JOBS) 2>&1 | tee make.log

clean:
	make -C $(BUILD_DIR) clean
	rm -f $(RELEASE_ARCHIVE) cmake.log make.log

distclean:
	rm -rf $(RELEASE_ARCHIVE) $(BUILD_DIR)
