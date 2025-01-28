# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set C++ standard 
# C++17 minimum required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(../sample_config.cmake)

# Set delegate library build options
add_compile_definitions(THREAD_STDLIB)
add_compile_definitions(SERIALIZE_NONE)
add_compile_definitions(TRANSPORT_NONE)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

set(DELEGATE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Collect all header files in the delegate directory
file(GLOB Delegate_HEADERS 
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/*.h"
    "${DELEGATE_ROOT_DIR}/src/delegate-mq/delegate/*.h"
)

# Organize delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

# Add subdirectories to include path
include_directories( 
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
)

# Add an executable target
add_executable(delegate_app ${SOURCES} ${Delegate_HEADERS})

# Link the library directory
link_directories(${LIBRARY_DIR})

# Link against the pre-built delegate_mq.lib
target_link_libraries(delegate_app PRIVATE 
    ${LIBRARY_DIR}/delegate_mq.lib
)

