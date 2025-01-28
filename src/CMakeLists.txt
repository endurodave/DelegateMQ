# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set default if USE_THREAD option if not defined
if (NOT DEFINED USE_THREAD)
    set(USE_THREAD "THREAD_STDLIB" CACHE STRING "Select the thread for the delegate")
endif()

# Select library build options
if (USE_THREAD STREQUAL "THREAD_STDLIB")
    add_compile_definitions(THREAD_STDLIB)
    file(GLOB SUBDIR_SOURCES "delegate-mq/predef/os/stdlib/*.cpp") 
    set(SOURCES ${SOURCES} ${SUBDIR_SOURCES})
else()
    message(FATAL_ERROR "Must select a delegate thread option.")
endif()

# Set default if USE_SERIALIZE option if not defined
if (NOT DEFINED USE_SERIALIZE)
    set(USE_SERIALIZE "SERIALIZE_SERIALIZE" CACHE STRING "Select the serialize for the delegate")
endif()

if (USE_SERIALIZE STREQUAL "SERIALIZE_SERIALIZE")
    add_compile_definitions(SERIALIZE_SERIALIZE)
elseif (USE_SERIALIZE STREQUAL "SERIALIZE_MSGPACK")
    add_compile_definitions(SERIALIZE_MSGPACK)
    set(MSGPACK_INCLUDE "../../msgpack-c/include")
    include_directories(${MSGPACK_INCLUDE})
else()
    message(FATAL_ERROR "Must select a delegate serialize option.")
endif()

file(GLOB SUBDIR_SOURCES "delegate-mq/predef/util/*.cpp") 
set(SOURCES ${SOURCES} ${SUBDIR_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_CURRENT_SOURCE_DIR}/delegate-mq
)

# Create the library target (delegate_mq) as a static library
add_library(delegate_mq STATIC ${SOURCES})

# Optionally handle allocator
if (ENABLE_ALLOCATOR)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/delegate-mq/predef/allocator)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/delegate-mq/predef/allocator ${CMAKE_BINARY_DIR}/allocator)
    target_link_libraries(delegate_mq PRIVATE allocator_lib)
    add_compile_definitions(USE_ALLOCATOR)
endif()

# Post-build action to copy the built library to the ../lib directory
add_custom_command(TARGET delegate_mq POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:delegate_mq> ${CMAKE_CURRENT_SOURCE_DIR}/../lib
    COMMENT "Copying delegate_mq to ../lib"
)

