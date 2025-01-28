# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Root directory for delegate library source code
set(DELEGATE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")
if(NOT EXISTS "${DELEGATE_ROOT_DIR}")
    message(FATAL_ERROR "${DELEGATE_ROOT_DIR} Directory does not exist. Update DELEGATE_ROOT_DIR to the correct directory.")
endif()

# Include directory for MessagePack C++ library 
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../msgpack-c/include")
if(NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR to the correct msgpack-c/include directory.")
endif()

# Path to the vcpkg directory for support libraries (e.g. ZeroMQ)
set(VCPKG_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../vcpkg/installed/x64-windows")
if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR to the correct directory.")
endif()

# Specify the directory containing the pre-built delegate_mq.lib
set(LIBRARY_DIR ${DELEGATE_ROOT_DIR}/lib)

# Check if the delegate_mq.lib exists in the specified directory
if (NOT EXISTS "${LIBRARY_DIR}/delegate_mq.lib")
    message(FATAL_ERROR "Error: delegate_mq.lib not found in ${LIBRARY_DIR}. Please ensure the library is pre-built and available.")
else()
    message(STATUS "Found delegate_mq.lib in ${LIBRARY_DIR}")
endif()
