cmake_minimum_required(VERSION 3.10)

project(xmodem_upload LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Source
add_executable(xmodem_upload xmodem_upload.cpp)

# Compiler warnings
target_compile_options(xmodem_upload PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Link pthreads on platforms that provide it
find_package(Threads QUIET)
if(Threads_FOUND)
  target_link_libraries(xmodem_upload PRIVATE Threads::Threads)
endif()

# Install target
install(TARGETS xmodem_upload RUNTIME DESTINATION bin)