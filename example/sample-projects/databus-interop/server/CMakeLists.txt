# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.16)

set(DELEGATE_APP "delegate_databus_interop_server")
project(DataBusInteropServer VERSION 1.0 LANGUAGES CXX)

# Disable Boost dependency for msgpack-c
add_compile_definitions(MSGPACK_NO_BOOST)

# Set build options
set(DMQ_ALLOCATOR "OFF")
set(DMQ_UTIL      "ON")
set(DMQ_THREAD    "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
set(DMQ_DATABUS   "ON")

if (WIN32)
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")
elseif (UNIX AND NOT APPLE)
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")
else()
    message(FATAL_ERROR "Unsupported platform for DMQ_TRANSPORT")
endif()

include("${CMAKE_SOURCE_DIR}/../../../../src/delegate-mq/DelegateMQ.cmake")

if (MSVC)
    add_compile_options("/utf-8")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

# Collect source files
file(GLOB SOURCES "*.cpp" "*.h")
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize in IDE
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files"   FILES ${DMQ_PREDEF_SOURCES})

# Include paths
include_directories(
    ${DMQ_INCLUDE_DIR}
    ${MSGPACK_INCLUDE_DIR}
)

# Executable
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

# Precompiled header
target_precompile_headers(${DELEGATE_APP} PRIVATE
    "${DMQ_INCLUDE_DIR}/DelegateMQ.h"
)

# Link libraries
find_package(Threads REQUIRED)
if (WIN32)
    target_link_libraries(${DELEGATE_APP} PRIVATE ws2_32 Threads::Threads)
else()
    target_link_libraries(${DELEGATE_APP} PRIVATE Threads::Threads)
endif()
