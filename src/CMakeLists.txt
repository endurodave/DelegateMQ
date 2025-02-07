# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_app")
set (DELEGATE_LIB "delegate_mq")
project(Delegate VERSION 1.0 LANGUAGES CXX)

include("${CMAKE_SOURCE_DIR}/Config.cmake")

# Set library build options
set (USE_THREAD "THREAD_STDLIB")
set (USE_SERIALIZE "SERIALIZE_SERIALIZE")
set (USE_TRANSPORT "TRANSPORT_NONE")

# Select library build option
if (USE_THREAD STREQUAL "THREAD_STDLIB")
    add_compile_definitions(THREAD_STDLIB)
    file(GLOB SUBDIR_SOURCES "delegate-mq/predef/os/stdlib/*.cpp")
    set(SOURCES ${SOURCES} ${SUBDIR_SOURCES})
else()
    message(FATAL_ERROR "Must select a delegate thread option.")
endif()

# Select the serialization build option
if (USE_SERIALIZE STREQUAL "SERIALIZE_SERIALIZE")
    add_compile_definitions(SERIALIZE_SERIALIZE)
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_MSGPACK")
    add_compile_definitions(SERIALIZE_MSGPACK)
    if (IS_DIRECTORY ${VCPKG_ROOT_DIR})
        include_directories(${VCPKG_ROOT_DIR}/include)
    else()
        message(FATAL_ERROR "Cannot find VCPKG_ROOT_DIR directory.")
    endif()
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_NONE")
    # Do nothing
else()
    message(FATAL_ERROR "Must select a delegate serialize option.")
endif()

# Select the serialization build option
if (USE_TRANSPORT STREQUAL "TRANSPORT_MSGPACK")
    add_compile_definitions(TRANSPORT_MSGPACK)
    if (IS_DIRECTORY ${VCPKG_ROOT_DIR})
        include_directories(${VCPKG_ROOT_DIR}/include)
    else()
        message(FATAL_ERROR "Cannot find VCPKG_ROOT_DIR directory.")
    endif()
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_WIN32_PIPE")
    add_compile_definitions(TRANSPORT_WIN32_PIPE)
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_WIN32_UDP")
    add_compile_definitions(TRANSPORT_WIN32_UDP)
elseif (USE_TRANSPORT STREQUAL "TRANSPORT_NONE")
    # Do nothing
endif()

file(GLOB SUBDIR_SOURCES "delegate-mq/predef/allocator/*.cpp") 
set(SOURCES ${SOURCES} ${SUBDIR_SOURCES})

file(GLOB SUBDIR_SOURCES "delegate-mq/predef/util/*.cpp") 
set(SOURCES ${SOURCES} ${SUBDIR_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
)

# Create the library target (delegate_mq) as a static library
add_library(${DELEGATE_LIB} STATIC ${SOURCES})

# Post-build action to copy the built library to the ../lib directory
add_custom_command(TARGET ${DELEGATE_LIB} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:delegate_mq> ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    COMMENT "Copying delegate_mq to ../lib"
)

