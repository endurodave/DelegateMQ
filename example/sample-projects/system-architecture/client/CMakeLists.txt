# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_client_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)
add_compile_definitions(CLIENT_APP)

# Set build options
set (DMQ_EXTERNAL_LIB "ON")
set (DMQ_ALLOCATOR "OFF")
set (DMQ_UTIL "ON")
set (DMQ_THREAD "DMQ_THREAD_STDLIB")
set (DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
set (DMQ_TRANSPORT "DMQ_TRANSPORT_ZEROMQ")
include("${CMAKE_SOURCE_DIR}/../../../../src/delegate-mq/DelegateMQ.cmake")

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "*.cpp" "*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/../common
    ${DMQ_ROOT_DIR}
    ${VCPKG_ROOT_DIR}/include
    ${MSGPACK_INCLUDE_DIR}
)

add_subdirectory("../common" "${CMAKE_BINARY_DIR}/common")

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

target_link_libraries(${DELEGATE_APP} PRIVATE 
    common_lib
    ${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}
)

copy_vcpkg_dlls(${VCPKG_ROOT_DIR} ${DELEGATE_APP})



