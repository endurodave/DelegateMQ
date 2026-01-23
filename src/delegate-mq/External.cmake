include(CMakeFindDependencyMacro)

# Previously, this checked if "${_var}" (the variable name) existed as a file.
# Now it correctly checks "${_file}" (the actual path).
macro(set_and_check _var _file)
    set(${_var} "${_file}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist!")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# ZeroMQ library package
# ---------------------------------------------------------------------------
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_ZEROMQ")
    set(ZMQ_ROOT "${DMQ_ROOT_DIR}/../../../zeromq/install")
    list(APPEND CMAKE_PREFIX_PATH "${ZMQ_ROOT}")

    find_package(ZeroMQ CONFIG REQUIRED)

    if (ZeroMQ_FOUND)
        message(STATUS "ZeroMQ found: ${ZeroMQ_VERSION}")
        if(TARGET libzmq-static AND NOT TARGET libzmq)
            add_library(libzmq ALIAS libzmq-static)
        endif()
        add_compile_definitions(ZMQ_STATIC)
    else()
        message(FATAL_ERROR "ZeroMQ not found in ${ZMQ_ROOT}")
    endif()
endif()

# ---------------------------------------------------------------------------
# NNG library
# ---------------------------------------------------------------------------
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_NNG")
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # Fixed: Point to the 'install' folder where headers actually live after build
        set_and_check(NNG_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../nng/install/include")
        set_and_check(NNG_LIBRARY_DIR "${DMQ_ROOT_DIR}/../../../nng/install/lib")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_and_check(NNG_INCLUDE_DIR "/usr/local/include/nng")
        set_and_check(NNG_LIBRARY_DIR "/usr/local/include/nng")
    endif()
endif()

# ---------------------------------------------------------------------------
# MQTT C library
# ---------------------------------------------------------------------------
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_MQTT")
    # Define root relative to DelegateMQ source
    set(MQTT_ROOT "${DMQ_ROOT_DIR}/../../../mqtt")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # 1. Headers
        # Note: If 'install' step wasn't run, headers might be in "${MQTT_ROOT}/src"
        set_and_check(MQTT_INCLUDE_DIR "${MQTT_ROOT}/install/include")
        
        # 2. Library (Lib) Directory (for linking)
        # Point to build/src where the .lib files are generated
        set(MQTT_LIBRARY_DIR "${MQTT_ROOT}/build/src")
        
        # 3. Binary (DLL) Directory (for runtime copy)
        # Point to build/src where the .dll files are generated
        set(MQTT_BINARY_DIR "${MQTT_ROOT}/build/src")

        # Fallback for manual definition if needed
        set(MQTT_LIBRARY 
            "${MQTT_LIBRARY_DIR}/paho-mqtt3c.lib"
            "ws2_32" "Rpcrt4" "Crypt32"
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_and_check(MQTT_INCLUDE_DIR "/usr/local/include")
        set_and_check(MQTT_LIBRARY "/usr/local/lib/libpaho-mqtt3c.so")
        set_and_check(MQTT_BINARY_DIR "/usr/local/bin")
    endif()
endif()

# ---------------------------------------------------------------------------
# libserialport library
# ---------------------------------------------------------------------------
if(DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_SERIAL_PORT")
    # Define the root of libserialport relative to the DelegateMQ source directory
    # Structure: Workspace/DelegateMQ/src/delegate-mq/External.cmake -> Workspace/libserialport
    set(SERIALPORT_ROOT "${DMQ_ROOT_DIR}/../../../libserialport")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # 1. Header: Found in the root of the source folder (libserialport.h)
        set_and_check(SERIALPORT_INCLUDE_DIR "${SERIALPORT_ROOT}") 
        
        # 2. Library: Found in the Visual Studio build output folder
        # Note: Change 'Debug' to 'Release' if building in Release mode
        set_and_check(SERIALPORT_LIBRARY_DIR "${SERIALPORT_ROOT}/x64/Debug")

    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # 1. Header: Found in the root of the source folder
        set_and_check(SERIALPORT_INCLUDE_DIR "${SERIALPORT_ROOT}") 
        
        # 2. Library: Found in the local build directory
        # Standard Autotools builds (./configure && make) place .so files in the hidden .libs/ directory
        if(EXISTS "${SERIALPORT_ROOT}/.libs")
            set_and_check(SERIALPORT_LIBRARY_DIR "${SERIALPORT_ROOT}/.libs")
        else()
            # Fallback: Check root if built differently
            set_and_check(SERIALPORT_LIBRARY_DIR "${SERIALPORT_ROOT}")
        endif()
    endif()
endif()
    
# ---------------------------------------------------------------------------
# MessagePack
# ---------------------------------------------------------------------------
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_MSGPACK")
    add_compile_definitions(MSGPACK_NO_BOOST)
    set_and_check(MSGPACK_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../msgpack-c/include")
endif()

# ---------------------------------------------------------------------------
# Cereal
# ---------------------------------------------------------------------------
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_CEREAL")
    set_and_check(CEREAL_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../cereal/include")
endif()

# ---------------------------------------------------------------------------
# Bitsery
# ---------------------------------------------------------------------------
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_BITSERY")
    set_and_check(BITSERY_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../bitsery/include")
endif()

# ---------------------------------------------------------------------------
# RapidJSON
# ---------------------------------------------------------------------------
if(DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_RAPIDJSON")
    set_and_check(RAPIDJSON_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../rapidjson/include")

    # Globally silence C++17 std::iterator warnings on RapidJSON library
    if(MSVC)
        add_compile_definitions(_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
    endif()
endif()

# ---------------------------------------------------------------------------
# FreeRTOS
# ---------------------------------------------------------------------------
if(DMQ_THREAD STREQUAL "DMQ_THREAD_FREERTOS")
    set_and_check(FREERTOS_ROOT_DIR "${DMQ_ROOT_DIR}/../../../FreeRTOSv202212.00")
    
    file(GLOB FREERTOS_SOURCES 
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/*.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/include/*.h"
    )
    list(APPEND FREERTOS_SOURCES 
        "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcKernelPort.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-Trace/trcSnapshotRecorder.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MSVC-MingW/port.c"
        "${FREERTOS_ROOT_DIR}/FreeRTOS/Source/portable/MemMang/heap_5.c"
    )
endif()

# ---------------------------------------------------------------------------
# spdlog
# ---------------------------------------------------------------------------
if (DMQ_LOG STREQUAL "ON")
    set_and_check(SPDLOG_INCLUDE_DIR "${DMQ_ROOT_DIR}/../../../spdlog/include")
endif()