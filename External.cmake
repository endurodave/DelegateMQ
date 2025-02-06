# External library definitions to support remote delegates. Update the options 
# below based on the target build platform.

# ***** BEGIN TARGET BUILD OPTIONS *****
# Modify the options below for your target platform external libraries.

# Set path to the vcpkg directory for support libraries (zmq.h)
set(VCPKG_ROOT_DIR "${DELEGATE_ROOT_DIR}/../vcpkg/installed/x64-windows")

# Set ZeroMQ library file name and directory
# https://github.com/zeromq
set(ZMQ_LIB_NAME "libzmq-mt-4_3_5.lib")     # Update to installed library version
set(ZMQ_LIB_DIR "${VCPKG_ROOT_DIR}/lib")

# Set path to the MessagePack C++ library (msgpack.hpp)
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${DELEGATE_ROOT_DIR}/../msgpack-c/include")

# ***** END TARGET BUILD OPTIONS *****

if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    #message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR to the correct directory.")
endif()

if (NOT EXISTS "${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}")
    #message(FATAL_ERROR "Error: ${ZMQ_LIB_NAME} not found in ${ZMQ_LIB_DIR}. Please ensure the library is available.")
else()
    message(STATUS "Found ${ZMQ_LIB_NAME} in ${ZMQ_LIB_DIR}")
endif()

if(NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    #message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR to the correct directory.")
endif()







