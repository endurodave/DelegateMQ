# External library directories. Update directory based on the target platform.

# Path to the vcpkg directory for support libraries (e.g. ZeroMQ)
set(VCPKG_ROOT_DIR "${DELEGATE_ROOT_DIR}/../vcpkg/installed/x64-windows")
if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR to the correct directory.")
endif()

# ZeroMQ library file and location
set(ZMQ_LIB_NAME "libzmq-mt-4_3_5.lib")
set(ZMQ_LIB_DIR "${VCPKG_ROOT_DIR}/lib")
if (NOT EXISTS "${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}")
    message(FATAL_ERROR "Error: ${ZMQ_LIB_NAME} not found in ${ZMQ_LIB_DIR}. Please ensure the library is available.")
else()
    message(STATUS "Found ${ZMQ_LIB_NAME} in ${ZMQ_LIB_DIR}")
endif()

# Include directory for MessagePack C++ library 
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${DELEGATE_ROOT_DIR}/../msgpack-c/include")
if(NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR to the correct msgpack-c/include directory.")
endif()







