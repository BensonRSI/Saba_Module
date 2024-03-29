cmake_minimum_required(VERSION 3.13)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
   message(FATAL_ERROR "In-source build detected!")
endif()

# initialize the SDK based on PICO_SDK_PATH and BOARD config
# note: this must happen before project()
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/rp2040")
set(PICO_BOARD "pico")
include(pico_sdk_import.cmake)
project(saba_module)
# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

function(setup_target targetname)
   pico_enable_stdio_usb(${targetname} 1)
   pico_enable_stdio_uart(${targetname} 0)
   pico_add_extra_outputs(${targetname})
endfunction()

# self-hosted tools to e.g. convert binary to text data
#add_subdirectory(tools)

add_library(mcurses
   mcurses/src/mcurses.c
   mcurses/src/hexedit.c
   )
target_include_directories(mcurses
   PUBLIC mcurses/src
   )

# eprom_emulator: simulate an Eprom, to load games from flash
add_executable(eprom_emulator
   main.c
   core0.c
   core1.c
   getaline.c
   )
   
target_link_libraries(eprom_emulator
   mcurses
   pico_stdlib
   pico_multicore
   )
setup_target(eprom_emulator)
