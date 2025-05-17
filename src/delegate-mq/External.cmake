# External library definitions to support remote delegates. Update the options 
# below based on the target build platform.

# ZeroMQ library package
# https://github.com/zeromq
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_ZEROMQ")
    # vcpkg package manager
    # https://github.com/microsoft/vcpkg
    set_and_check(VCPKG_ROOT_DIR "${DMQ_ROOT_DIR}/../../../vcpkg")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_and_check(ZeroMQ_DIR "${VCPKG_ROOT_DIR}/installed/x64-windows/share/zeromq")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_and_check(ZeroMQ_DIR "${VCPKG_ROOT_DIR}/installed/x64-linux-dynamic/share/zeromq")
    endif()
    find_package(ZeroMQ CONFIG REQUIRED)
    if (ZeroMQ_FOUND)
        message(STATUS "ZeroMQ found: ${ZeroMQ_VERSION}")
    else()
        message(FATAL_ERROR "ZeroMQ not found!")
    endif()
endif()

# NNG library
# https://github.com/nanomsg/nng
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_NNG")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_and_check(NNG_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../nng/include")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_and_check(NNG_INCLUDE_DIR "/usr/local/include/nng")
    endif()
endif()

# MQTT C library
# https://github.com/eclipse-paho/paho.mqtt.c
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_MQTT")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set_and_check(MQTT_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../mqtt/paho-c/include")
        set_and_check(MQTT_LIBRARY_DIR "${DMQ_ROOT_DIR}/../../../mqtt/paho-c/lib")
        set_and_check(MQTT_BINARY_DIR "${DMQ_ROOT_DIR}/../../../mqtt/paho-c/bin")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_and_check(MQTT_INCLUDE_DIR "/usr/local/include")
        set_and_check(MQTT_LIBRARY_DIR "/usr/local/lib")
        set_and_check(MQTT_BINARY_DIR "/usr/local/bin/MQTTVersion")
    endif()
endif()
    
# MessagePack C++ library (msgpack.hpp)
# https://github.com/msgpack/msgpack-c/tree/cpp_master
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_MSGPACK")
    set_and_check(MSGPACK_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../msgpack-c/include")
endif()

# Cereal C++ library
# https://github.com/USCiLab/cereal
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_CEREAL")
    set_and_check(CEREAL_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../cereal/include")
endif()

# Bitsery C++ library
# https://github.com/fraillt/bitsery
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_BITSERY")
    set_and_check(BITSERY_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../bitsery/include")
endif()

# RapidJSON C++ library
# https://github.com/Tencent/rapidjson
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_RAPIDJSON")
    set_and_check(RAPIDJSON_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../rapidjson/include")
endif()

# FreeRTOS library
# https://github.com/FreeRTOS/FreeRTOS
if(DMQ_THREAD STREQUAL "DMQ_THREAD_FREERTOS")
    set_and_check(FREERTOS_ROOT_DIR "${DMQ_ROOT_DIR}/../../../FreeRTOSv202212.00")
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



