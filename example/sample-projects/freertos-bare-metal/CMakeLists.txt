# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_app")
project(Delegate VERSION 1.0 LANGUAGES CXX C)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_GENERATOR_PLATFORM Win32)
endif()

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
source_group("Delegate Files" FILES ${DELEGATEMQ_SOURCES})
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

