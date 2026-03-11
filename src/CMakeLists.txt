cmake_minimum_required(VERSION 3.13)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
   message(FATAL_ERROR "In-source build detected!")
endif()

# initialize the SDK based on PICO_SDK_PATH and BOARD config
# note: this must happen before project()

set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/rp2040")
set(PICO_BOARD "pico")
set(PICO_COPY_TO_RAM 1)
add_compile_definitions(SKpico PICO_NO_FPGA_CHECK=1)
add_compile_definitions(SKpico PICO_BARE_METAL=1)
add_compile_definitions(SKpico PICO_CXX_ENABLE_EXCEPTIONS=0)
add_compile_definitions(SKpico PICO_STDIO_UART=0)
add_compile_definitions(SKpico PICO_COPY_TO_RAM=1)

include(pico_sdk_import.cmake)
project(saba_module C CXX ASM)
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

set_property(SOURCE modul/romc.S APPEND PROPERTY LANGUAGE ASM )

#add_compile_definitions(DEBUG_STATES)
#add_compile_definitions(STORE_BUS_DATA)  # Debug-feature, read from bus, instead of writing
#add_compile_definitions(DEBUG_STATES_IO_ADDR)  # Store IO-Adress accessed in IO-bank for debug at 0x200
add_compile_definitions(RECEIVE_FULL_FILE)  # Transfer full file via xmodem, not just 256 bytes


# eprom_emulator: simulate an Eprom, to load games from flash
add_executable(eprom_emulator
   main.c
   core0.c
   core1.c
   flash_store.c
   getaline.c
   modul/romc.S
   xmodem/xmodem.c
   )

target_include_directories(eprom_emulator
    PUBLIC $(CMAKE_CURRENT_SOURCE_DIR)/xmodem
   )

target_link_libraries(eprom_emulator
   mcurses
   pico_stdlib
   pico_multicore
   )
setup_target(eprom_emulator)
