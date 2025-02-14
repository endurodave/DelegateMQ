# External library definitions to support remote delegates. Update the options 
# below based on the target build platform.

# vcpkg package manager
# https://github.com/microsoft/vcpkg
set(VCPKG_ROOT_DIR "${DMQ_ROOT_DIR}/../../../vcpkg")
if(NOT EXISTS "${VCPKG_ROOT_DIR}")
    message(FATAL_ERROR "${VCPKG_ROOT_DIR} Directory does not exist. Update VCPKG_ROOT_DIR.")
endif()

# ZeroMQ library package
# https://github.com/zeromq
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ZeroMQ_DIR "${VCPKG_ROOT_DIR}/installed/x64-windows/share/zeromq")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ZeroMQ_DIR "${VCPKG_ROOT_DIR}/installed/x64-linux/share/zeromq")
endif()
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_ZEROMQ")
    find_package(ZeroMQ CONFIG REQUIRED)
    if (ZeroMQ_FOUND)
        message(STATUS "ZeroMQ found: ${ZeroMQ_VERSION}")
    else()
        message(FATAL_ERROR "ZeroMQ not found!")
    endif()
endif()
    
# MessagePack C++ library (msgpack.hpp)
# https://github.com/msgpack/msgpack-c/tree/cpp_master
set(MSGPACK_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../msgpack-c/include")
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_MSGPACK" AND NOT EXISTS "${MSGPACK_INCLUDE_DIR}")
    message(FATAL_ERROR "${MSGPACK_INCLUDE_DIR} Directory does not exist. Update MSGPACK_INCLUDE_DIR.")
endif()

# RapidJSON C++ library
# https://github.com/Tencent/rapidjson
set(RAPIDJSON_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../rapidjson/include")
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_RAPIDJSON" AND NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}")
    message(FATAL_ERROR "${RAPIDJSON_INCLUDE_DIR} Directory does not exist. Update RAPIDJSON_INCLUDE_DIR.")
endif()

# FreeRTOS library
# https://github.com/FreeRTOS/FreeRTOS
set(FREERTOS_ROOT_DIR "${DMQ_ROOT_DIR}/../../../FreeRTOSv202212.00")
if(DMQ_THREAD STREQUAL "DMQ_THREAD_FREERTOS" AND NOT EXISTS "${FREERTOS_ROOT_DIR}")
    message(FATAL_ERROR "${FREERTOS_ROOT_DIR} Directory does not exist. Update FREERTOS_ROOT_DIR.")
else()
    # Collect FreeRTOS source files
    file(GLOB FREERTOS_SOURCES 
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/*.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/Include/*.h"
    )
    list(APPEND FREERTOS_SOURCES 
        "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcKernelPort.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcSnapshotRecorder.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MSVC-MingW/port.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MemMang/heap_5.c"
    )
endif()



