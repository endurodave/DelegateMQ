# CMake command must use 32-bit architecture on Windows FreeRTOS simulation build.
# cmake -A Win32 -B build .

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

if (CMAKE_HOST_SYSTEM_NAME MATCHES "Windows") 
    if (${CMAKE_GENERATOR_PLATFORM} MATCHES "Win32")
        message(STATUS "32-bit architecture (Win32) set.")
    else()
        message(FATAL_ERROR "64-bit architecture not supported on FreeRTOS build.\nUse `-A Win32` cmake option.")
    endif()
else()
    message(WARNING "Unknown architecture. Ensure 32-bit for FreeRTOS build.")
endif()

set (DELEGATE_APP "delegate_app")
project(Delegate VERSION 1.0 LANGUAGES CXX C)

# Set build options
set (USE_EXTERNAL_LIB "ON")
set (USE_ALLOCATOR "OFF")
set (USE_UTIL "ON")
set (USE_THREAD "THREAD_FREERTOS")
set (USE_SERIALIZE "SERIALIZE_NONE")
set (USE_TRANSPORT "TRANSPORT_NONE")
include("${CMAKE_SOURCE_DIR}/../../../Config.cmake")

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.c*" "${CMAKE_SOURCE_DIR}/*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${PREDEF_SOURCES})

# Collect FreeRTOS source files
list(APPEND SOURCES ${FREERTOS_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DELEGATEMQ_SOURCES} ${PREDEF_SOURCES})
source_group("FreeRTOS Files" FILES ${FREERTOS_SOURCES})

# Add subdirectories to include path
include_directories(
    .
    ./Trace_Recorder_Configuration
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
    ${FREERTOS_ROOT_DIR}/FreeRTOS/Source/include
    ${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MSVC-MingW
    ${FREERTOS_ROOT_DIR}/FreeRTOS/Common/include
    ${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/Include
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DELEGATEMQ_SOURCES})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

