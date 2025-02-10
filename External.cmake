# External library definitions to support remote delegates. Update the options 
# below based on the target build platform.

# Modify the options below for your target platform external libraries.

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building on Windows")
    
    # TODO: Update to installed library version
    set(ZMQ_LIB_NAME "libzmq-mt-4_3_5.lib")     
    
    # Set path to the vcpkg directory for support libraries (zmq.h)
    set(VCPKG_ROOT_DIR "${DELEGATE_ROOT_DIR}/../vcpkg/installed/x64-windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on Linux")
    set(ZMQ_LIB_NAME "libzmq.a") 
    set(VCPKG_ROOT_DIR "${DELEGATE_ROOT_DIR}/../vcpkg/installed/x64-linux")

else()
    message(FATAL_ERROR "Select directories based on build platform.")
endif()

if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR to the correct directory.")
endif()

# Set ZeroMQ library file name and directory
# https://github.com/zeromq
set(ZMQ_LIB_DIR "${VCPKG_ROOT_DIR}/lib")
if (NOT EXISTS "${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}")
    message(FATAL_ERROR "Error: ${ZMQ_LIB_NAME} not found in ${ZMQ_LIB_DIR}. Please ensure the library is available.")
else()
    message(STATUS "Found ${ZMQ_LIB_NAME} in ${ZMQ_LIB_DIR}")
endif()

# Set path to the MessagePack C++ library (msgpack.hpp)
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${DELEGATE_ROOT_DIR}/../msgpack-c/include")
if(NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR to the correct directory.")
endif()

# Set path to the RapidJSON C++ library
# https://github.com/Tencent/rapidjson
set(RAPIDJSON_INCLUDE_DIR "${DELEGATE_ROOT_DIR}/../rapidjson/include")
if(NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}")
    message(FATAL_ERROR "${RAPIDJSON_INCLUDE_DIR} Directory does not exist. Update RAPIDJSON_INCLUDE_DIR to the correct directory.")
endif()

# Set path to the FreeRTOS library
# https://github.com/FreeRTOS/FreeRTOS
set(FREERTOS_ROOT_DIR "${DELEGATE_ROOT_DIR}/../FreeRTOSv202212.00")
if(NOT EXISTS "${FREERTOS_ROOT_DIR}")
    message(FATAL_ERROR "${FREERTOS_ROOT_DIR} Directory does not exist. Update FREERTOS_ROOT_DIR to the correct directory.")
endif()



