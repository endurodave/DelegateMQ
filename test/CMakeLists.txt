# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.16)

set(DELEGATE_APP "delegate_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)

# @TODO: Set build options (see Predef.cmake). 
# Rerun cmake configuration and clean/rebuild if you change these settings
set(DMQ_ASSERTS "OFF")                        # ON for assert faults
set(DMQ_ALLOCATOR "OFF")                      # ON for fixed-block allocator
set(DMQ_LOG "OFF")                            # ON for spglog output
set(DMQ_UTIL "ON")                            # ON for delegate utility classes
set(DMQ_THREAD "DMQ_THREAD_STDLIB")           # Set thread support library or none
set(DMQ_SERIALIZE "DMQ_SERIALIZE_SERIALIZE")  # Set serialization support library or none
set(DMQ_TRANSPORT "DMQ_TRANSPORT_NONE")       # Set transport support library or none

# Include master delegate cmake build options
include("${CMAKE_SOURCE_DIR}/src/delegate-mq/DelegateMQ.cmake")

if (MSVC)
    add_compile_options("/utf-8")   # Required by spdlog
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

# Collect all .cpp and *.h source files 
file(GLOB SOURCES "*.cpp" "*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files" FILES ${DMQ_PREDEF_SOURCES})

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
    ${DMQ_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
    test/unit-tests
)

# Add subdirectories for unit tests and sample code
# NOTE: Added before target creation so libraries are defined for linking
add_subdirectory(unit-tests)
add_subdirectory(${CMAKE_SOURCE_DIR}/example/sample-code ${CMAKE_BINARY_DIR}/sample-code)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

# -----------------------------------------------------------------------------
# PRECOMPILED HEADERS (PCH)
# This single command works for both MSVC (Windows) and GCC/Clang (Linux).
# It compiles DelegateMQ.h once and auto-includes it into every source file
# in the target, significantly speeding up compilation.
# -----------------------------------------------------------------------------
target_precompile_headers(${DELEGATE_APP} PRIVATE 
    "${CMAKE_SOURCE_DIR}/src/delegate-mq/DelegateMQ.h"
)

target_link_libraries(${DELEGATE_APP} PRIVATE 
    unit_tests_lib
    sample_code_lib
)