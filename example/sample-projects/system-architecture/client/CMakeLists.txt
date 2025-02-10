# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_client_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)
add_compile_definitions(CLIENT_APP)

# Set build options
set (USE_EXTERNAL_LIB "ON")
set (USE_ALLOCATOR "OFF")
set (USE_UTIL "ON")
set (USE_THREAD "THREAD_STDLIB")
set (USE_SERIALIZE "SERIALIZE_MSGPACK")
set (USE_TRANSPORT "TRANSPORT_ZEROMQ")
include("${CMAKE_SOURCE_DIR}/../../../../Config.cmake")

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DELEGATEMQ_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/../common
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
    ${VCPKG_ROOT_DIR}/include
    ${MSGPACK_INCLUDE_DIR}
)

add_subdirectory("../common" "${CMAKE_BINARY_DIR}/common")

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DELEGATEMQ_SOURCES})

target_link_libraries(${DELEGATE_APP} PRIVATE 
    common_lib
    ${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}
)

copy_vcpkg_dlls(${VCPKG_ROOT_DIR} ${DELEGATE_APP})



