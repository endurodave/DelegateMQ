# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set(DELEGATE_APP "delegate_server_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)
add_compile_definitions(SERVER_APP)

# Set build options
set(DMQ_ASSERTS "OFF")
set(DMQ_ALLOCATOR "OFF")
set(DMQ_LOG "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_SERIALIZE")
if (WIN32)
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")
elseif (UNIX AND NOT APPLE)
    set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")
else()
    message(FATAL_ERROR "Unsupported platform for DMQ_TRANSPORT")
endif()
include("${CMAKE_SOURCE_DIR}/../../../../src/delegate-mq/DelegateMQ.cmake")

if (MSVC)
    add_compile_options("/utf-8")   # Required by spdlog
endif()

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "*.cpp" "*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files" FILES ${DMQ_PREDEF_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/../common
    ${DMQ_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

add_subdirectory("../common" "${CMAKE_BINARY_DIR}/common")

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

target_link_libraries(${DELEGATE_APP} PRIVATE 
    common_lib
)


