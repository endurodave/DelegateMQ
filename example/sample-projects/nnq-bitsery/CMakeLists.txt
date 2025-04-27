# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set(DELEGATE_APP "delegate_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)

if (MSVC)
    add_compile_options("/Zc:__cplusplus")
endif()

# Set build options
set(DMQ_ALLOCATOR "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_BITSERY")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_NNG")
include("${CMAKE_SOURCE_DIR}/../../../src/delegate-mq/DelegateMQ.cmake")

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "*.cpp" "*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files" FILES ${DMQ_PREDEF_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${DMQ_INCLUDE_DIR}
    ${NNG_INCLUDE_DIR}
    ${BITSERY_INCLUDE_DIR}
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

find_library(NNG_LIB nng HINTS ${NNG_LIBRARY_DIR})

if (NOT NNG_LIB)
    message(FATAL_ERROR "Could not find nng.lib in ${NNG_LIBRARY_DIR}")
endif()

add_definitions(-DNNG_STATIC_LIB) 

target_link_libraries(${DELEGATE_APP} PRIVATE 
    ${NNG_LIB}
)

# Link ws2_32 on Windows only
if (WIN32)
    target_link_libraries(${DELEGATE_APP} PRIVATE ws2_32)
endif()


