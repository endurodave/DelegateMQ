# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set C++ standard 
# C++17 minimum required. C++20 tested and works too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files 
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect all delegate header files 
file(GLOB Delegate_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/delegate/*.h"
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/delegate
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/predef/os
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/predef/serialize
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/predef/util
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/unit-tests
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/predef/os/stdlib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq/predef/serialize/serialize)

# Add an executable target
add_executable(delegate_app ${SOURCES} ${Delegate_HEADERS})

# Specify the directory containing the pre-built delegate_mq.lib
set(LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

# Check if the delegate_mq.lib exists in the specified directory
if (NOT EXISTS "${LIBRARY_DIR}/delegate_mq.lib")
    message(FATAL_ERROR "Error: delegate_mq.lib not found in ${LIBRARY_DIR}. Please ensure the library is pre-built and available.")
else()
    message(STATUS "Found delegate_mq.lib in ${LIBRARY_DIR}")
endif()

# Link the library directory
link_directories(${LIBRARY_DIR})

# Link against the pre-built delegate_mq.lib
target_link_libraries(delegate_app PRIVATE 
    ${LIBRARY_DIR}/delegate_mq.lib
    unit_tests_lib
    sample_code_lib
)

# Add subdirectories for unit tests and sample code
add_subdirectory(unit-tests)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../example/sample-code ${CMAKE_BINARY_DIR}/../example/sample-code)

