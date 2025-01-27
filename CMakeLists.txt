# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A Win32 -B build -S .
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DENABLE_ALLOCATOR=ON
#
# *** Linux ***
# cmake -G "Unix Makefiles" -B build -S .
# cmake -G "Unix Makefiles" -B build -S . -DENABLE_ALLOCATOR=ON
# cmake -G "Unix Makefiles" -B build -S . -DTEST_COVERAGE=ON

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Use built-in serialize class
add_compile_definitions(MSGSERIALIZE_SERIALIZE)
add_compile_definitions(STDTHREAD_THREAD)

# Set C++ standard 
# C++17 minimum required. C++20 tested and works too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h")

# Collect all header files in the src/Delegate directory
file(GLOB Delegate_HEADERS 
    "${CMAKE_SOURCE_DIR}/src/delegate-mq/*.h"
    "${CMAKE_SOURCE_DIR}/src/delegate-mq/delegate/*.h"
)

# Organize Delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

# Check if the TEST_COVERAGE argument is passed
option(TEST_COVERAGE "Enable code coverage" OFF)

if(TEST_COVERAGE)
  message(STATUS "Code coverage is enabled")

  # Add coverage flags for GCC/Clang compilers
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
endif()

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/src/delegate-mq
    ${CMAKE_SOURCE_DIR}/src/delegate-mq/delegate
    ${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/os
    ${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/serialize
    ${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/util
    ${CMAKE_SOURCE_DIR}/test/unit-tests
)

include_directories(${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/os/stdlib)
include_directories(${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/serialize/serialize)

# Add an executable target
add_executable(delegate_app ${SOURCES} ${Delegate_HEADERS})

# Add subdirectories to build
add_subdirectory(src/delegate-mq/predef/os)
add_subdirectory(src/delegate-mq/predef/util)
add_subdirectory(tests/unit-tests)
add_subdirectory(examples/sample-code)

target_link_libraries(delegate_app PRIVATE 
    os_lib
    util_lib
    unit_tests_lib
    sample_code_lib
)

if (ENABLE_ALLOCATOR)
    include_directories(${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/allocator)
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/delegate-mq/predef/allocator ${CMAKE_BINARY_DIR}/allocator)
    target_link_libraries(delegate_app PRIVATE allocator_lib)
    add_compile_definitions(USE_ALLOCATOR)
endif()

