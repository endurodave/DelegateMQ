# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

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
file(GLOB FREERTOS_SOURCES "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/*.c")
list(APPEND SOURCES ${FREERTOS_SOURCES})
list(APPEND SOURCES 
    "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcKernelPort.c"
    "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcSnapshotRecorder.c"
    "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MSVC-MingW/port.c"
    "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MemMang/heap_5.c"
)

# Collect all header files in the delegate directory
file(GLOB Delegate_HEADERS 
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/*.h"
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/delegate/*.h"
)

# Organize delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

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
add_executable(${DELEGATE_APP} ${SOURCES} ${Delegate_HEADERS})

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

