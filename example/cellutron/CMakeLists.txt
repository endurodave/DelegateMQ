cmake_minimum_required(VERSION 3.16)

# Force Win32 platform for FreeRTOS Simulator compatibility on Windows
if (WIN32 AND NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM Win32 CACHE STRING "Force Win32 for FreeRTOS" FORCE)
endif()

project(Cellutron VERSION 1.0 LANGUAGES CXX C)

set(CELLUTRON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# Add processor subdirectories
add_subdirectory(gui)
add_subdirectory(controller)
add_subdirectory(safety)
