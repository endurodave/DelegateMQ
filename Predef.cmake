# Predef build options

if (USE_THREAD STREQUAL "THREAD_STDLIB")
	add_compile_definitions(THREAD_STDLIB)
	file(GLOB THREAD_SOURCES 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/os/stdlib/*.c*" 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/os/stdlib/*.h" 
	)
elseif (USE_THREAD STREQUAL "THREAD_FREERTOS")
	add_compile_definitions(THREAD_FREERTOS)
	file(GLOB THREAD_SOURCES 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/os/freertos/*.c*" 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/os/freertos/*.h" 
	)
elseif (USE_THREAD STREQUAL "THREAD_NONE")
	add_compile_definitions(THREAD_NONE)
else()
	message(FATAL_ERROR "Must set USE_THREAD option.")
endif()

if (USE_SERIALIZE STREQUAL "SERIALIZE_NONE")
	add_compile_definitions(SERIALIZE_NONE)
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_SERIALIZE")
	add_compile_definitions(SERIALIZE_SERIALIZE)
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_RAPIDJSON")
	add_compile_definitions(SERIALIZE_RAPIDJSON)
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_MSGPACK")
	add_compile_definitions(SERIALIZE_MSGPACK)
endif()

if (USE_TRANSPORT STREQUAL "TRANSPORT_NONE")
	add_compile_definitions(TRANSPORT_NONE)
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_ZEROMQ")
	add_compile_definitions(TRANSPORT_ZEROMQ)
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_WIN32_PIPE")
	add_compile_definitions(TRANSPORT_WIN32_PIPE)
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_WIN32_UDP")
	add_compile_definitions(TRANSPORT_WIN32_UDP)
endif()

if (USE_ALLOCATOR STREQUAL "ON")
	add_compile_definitions(USE_ALLOCATOR)
	file(GLOB ALLOCATOR_SOURCES 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/allocator/*.c*" 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/allocator/*.h" 
	)
endif()

if (USE_UTIL STREQUAL "ON")
	file(GLOB UTIL_SOURCES 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/util/*.c*" 
		"${DELEGATE_ROOT_DIR}/src/delegate-mq/predef/util/*.h" 
	)
endif()

if (USE_ASSERTS STREQUAL "ON")
	add_compile_definitions(USE_ASSERTS)
endif()

# Combine all predef sources into PREDEF_SOURCES
set(PREDEF_SOURCES "")
list(APPEND PREDEF_SOURCES ${THREAD_SOURCES})
list(APPEND PREDEF_SOURCES ${OS_SOURCES})
list(APPEND PREDEF_SOURCES ${ALLOCATOR_SOURCES})
list(APPEND PREDEF_SOURCES ${UTIL_SOURCES})
