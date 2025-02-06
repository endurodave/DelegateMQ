# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_client_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)
add_compile_definitions(CLIENT_APP)

set (USE_EXTERNAL_LIB "ON")
include("${CMAKE_SOURCE_DIR}/../../../../Config.cmake")

# Set delegate library build options
add_compile_definitions(THREAD_STDLIB)
add_compile_definitions(SERIALIZE_MSGPACK)
add_compile_definitions(TRANSPORT_MSGPACK)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect all header files in the delegate directory
file(GLOB Delegate_HEADERS 
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/*.h"
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/delegate/*.h"
)

# Organize delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/../common
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
    ${VCPKG_ROOT_DIR}/include
    ${MSGPACK_INCLUDE_DIR}
)

add_subdirectory("../common" "${CMAKE_BINARY_DIR}/common")

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${Delegate_HEADERS})

# Link the library directory
link_directories(${DELEGATE_LIB_DIR})

# Link against the pre-built delegate_mq.lib
target_link_libraries(${DELEGATE_APP} PRIVATE 
    common_lib
    ${DELEGATE_LIB_DIR}/${DELEGATE_LIB_NAME}
    ${ZMQ_LIB_DIR}/${ZMQ_LIB_NAME}
)

copy_vcpkg_dlls(${VCPKG_ROOT_DIR} ${DELEGATE_APP})



