# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DENABLE_ALLOCATOR=ON

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Root directory for delegate library source code
set(DELEGATE_ROOT_DIR "${CMAKE_SOURCE_DIR}/../../../../src")
if(NOT EXISTS "${DELEGATE_ROOT_DIR}")
    message(FATAL_ERROR "${DELEGATE_ROOT_DIR} Directory does not exist. Update DELEGATE_ROOT_DIR to the correct directory.")
endif()

# Include directory for MessagePack C++ library 
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../../../../../msgpack-c/include")
if(NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR to the correct msgpack-c/include directory.")
endif()

# Path to the vcpkg directory for support libraries (e.g. ZeroMQ)
set(VCPKG_ROOT_DIR "../../../../../vcpkg/installed/x64-windows")
if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR to the correct directory.")
endif()

# Set C++ standard 
# C++17 minimum required. C++20 tested and works too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect all header files in the delegate directory
file(GLOB Delegate_HEADERS 
    "${DELEGATE_ROOT_DIR}/delegate-mq/*.h"
    "${DELEGATE_ROOT_DIR}/delegate-mq/delegate/*.h"
)

# Organize Delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

# Add subdirectories to include path
include_directories( 
    ../common
    ${DELEGATE_ROOT_DIR}/delegate-mq
    ${DELEGATE_ROOT_DIR}/delegate-mq/delegate
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/os
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/serialize
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/util
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/dispatcher
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/transport/msgpack
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/os/stdlib
    ${DELEGATE_ROOT_DIR}/delegate-mq/predef/serialize/msgpack
    ${VCPKG_ROOT_DIR}/include
    ${MSGPACK_INCLUDE_DIR}
)

# Add an executable target
add_executable(delegate_server_app ${SOURCES} ${Delegate_HEADERS})

add_compile_definitions(SERVER_APP)

# Add subdirectories to build
add_subdirectory(../common ${CMAKE_BINARY_DIR}/common)
add_subdirectory(${DELEGATE_ROOT_DIR}/delegate-mq/predef/os ${CMAKE_BINARY_DIR}/predef/os)
add_subdirectory(${DELEGATE_ROOT_DIR}/delegate-mq/predef/util ${CMAKE_BINARY_DIR}/predef/util)

target_link_libraries(delegate_server_app PRIVATE 
    ../${VCPKG_ROOT_DIR}/lib/libzmq-mt-4_3_5
    os_lib
    util_lib
    common_lib
)

if (ENABLE_ALLOCATOR)
    include_directories(${DELEGATE_ROOT_DIR}/delegate-mq/predef/allocator)
    add_subdirectory(${DELEGATE_ROOT_DIR}//delegate-mq/predef/allocator ${CMAKE_BINARY_DIR}/allocator)
    target_link_libraries(delegate_server_app PRIVATE allocator_lib)
    add_compile_definitions(USE_ALLOCATOR)
endif()

# Get .dll files in the vcpkg bin directory
file(GLOB ZMQ_BIN_FILES "${VCPKG_ROOT_DIR}/bin/libzmq*.dll" "${VCPKG_ROOT_DIR}/bin/libzmq*.pdb")

# Copy each DLL file to the build output directory
foreach(DLL_FILE ${ZMQ_BIN_FILES})
    add_custom_command(TARGET delegate_server_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLL_FILE}  # Copy the DLL file
        "${CMAKE_BINARY_DIR}/Debug"  # Destination directory
        COMMENT "Copying ${DLL_FILE} to build output"
    )
endforeach()

