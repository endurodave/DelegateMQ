# Predef build options

if (DMQ_THREAD STREQUAL "DMQ_THREAD_STDLIB")
    add_compile_definitions(DMQ_THREAD_STDLIB)
    file(GLOB THREAD_SOURCES 
        "${DMQ_ROOT_DIR}/predef/os/stdlib/*.c*" 
        "${DMQ_ROOT_DIR}/predef/os/stdlib/*.h" 
    )
elseif (DMQ_THREAD STREQUAL "DMQ_THREAD_FREERTOS")
    add_compile_definitions(DMQ_THREAD_FREERTOS)
    file(GLOB THREAD_SOURCES 
        "${DMQ_ROOT_DIR}/predef/os/freertos/*.c*" 
        "${DMQ_ROOT_DIR}/predef/os/freertos/*.h" 
    )
elseif (DMQ_THREAD STREQUAL "DMQ_THREAD_NONE")
    add_compile_definitions(DMQ_THREAD_NONE)
else()
    message(FATAL_ERROR "Must set DMQ_THREAD option.")
endif()

if (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_NONE")
    add_compile_definitions(DMQ_SERIALIZE_NONE)
elseif (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_SERIALIZE")
    add_compile_definitions(DMQ_SERIALIZE_SERIALIZE)
    file(GLOB SERIALIZE_SOURCES "${DMQ_ROOT_DIR}/predef/serialize/serialize/*.h")
elseif (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_RAPIDJSON")
    add_compile_definitions(DMQ_SERIALIZE_RAPIDJSON)
    file(GLOB SERIALIZE_SOURCES "${DMQ_ROOT_DIR}/predef/serialize/rapidjson/*.h")
elseif (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_MSGPACK")
    add_compile_definitions(DMQ_SERIALIZE_MSGPACK)
    file(GLOB SERIALIZE_SOURCES "${DMQ_ROOT_DIR}/predef/serialize/msgpack/*.h")
elseif (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_CEREAL")
    add_compile_definitions(DMQ_SERIALIZE_CEREAL)
    file(GLOB SERIALIZE_SOURCES "${DMQ_ROOT_DIR}/predef/serialize/cereal/*.h")
elseif (DMQ_SERIALIZE STREQUAL "DMQ_SERIALIZE_BITSERY")
    add_compile_definitions(DMQ_SERIALIZE_BITSERY)
    file(GLOB SERIALIZE_SOURCES "${DMQ_ROOT_DIR}/predef/serialize/bitsery/*.h")
else()
    message(FATAL_ERROR "Must set DMQ_SERIALIZE option.")
endif()

if (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_NONE")
    add_compile_definitions(DMQ_TRANSPORT_NONE)
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_ZEROMQ")
    add_compile_definitions(DMQ_TRANSPORT_ZEROMQ)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/zeromq/*.h")
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_NNG")
    add_compile_definitions(DMQ_TRANSPORT_NNG)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/nng/*.h")
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_MQTT")
    add_compile_definitions(DMQ_TRANSPORT_MQTT)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/mqtt/*.h")
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_WIN32_PIPE")
    add_compile_definitions(DMQ_TRANSPORT_WIN32_PIPE)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/win32-pipe/*.h")
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_WIN32_UDP")
    add_compile_definitions(DMQ_TRANSPORT_WIN32_UDP)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/win32-udp/*.h")
elseif (DMQ_TRANSPORT STREQUAL "DMQ_TRANSPORT_LINUX_UDP")
    add_compile_definitions(DMQ_TRANSPORT_LINUX_UDP)
    file(GLOB TRANSPORT_SOURCES "${DMQ_ROOT_DIR}/predef/transport/linux-udp/*.h")
else()
    message(FATAL_ERROR "Must set DMQ_TRANSPORT option.")
endif()

file(GLOB DISPATCHER_SOURCES 
    "${DMQ_ROOT_DIR}/predef/dispatcher/*.h" 
)

if (DMQ_ALLOCATOR STREQUAL "ON")
    add_compile_definitions(DMQ_ALLOCATOR)
    file(GLOB ALLOCATOR_SOURCES 
        "${DMQ_ROOT_DIR}/predef/allocator/*.c*" 
        "${DMQ_ROOT_DIR}/predef/allocator/*.h" 
    )
endif()

if (DMQ_UTIL STREQUAL "ON")
    file(GLOB UTIL_SOURCES 
        "${DMQ_ROOT_DIR}/predef/util/*.c*" 
        "${DMQ_ROOT_DIR}/predef/util/*.h" 
    )
endif()

if (DMQ_ASSERTS STREQUAL "ON")
    add_compile_definitions(DMQ_ASSERTS)
endif()

if (DMQ_LOG STREQUAL "ON")
    add_compile_definitions(DMQ_LOG)
endif()

# Combine all predef sources into DMQ_PREDEF_SOURCES
set(DMQ_PREDEF_SOURCES "")
list(APPEND DMQ_PREDEF_SOURCES ${TRANSPORT_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${SERIALIZE_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${DISPATCHER_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${THREAD_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${OS_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${ALLOCATOR_SOURCES})
list(APPEND DMQ_PREDEF_SOURCES ${UTIL_SOURCES})

# Collect all DelegateMQ files
file(GLOB DMQ_LIB_SOURCES
    "${DMQ_ROOT_DIR}/*.h"
    "${DMQ_ROOT_DIR}/delegate/*.h"
)
