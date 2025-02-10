# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_app")
set (DELEGATE_LIB "delegate_mq")
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set build options
set (USE_EXTERNAL_LIB "ON")
set (USE_ALLOCATOR "OFF")
set (USE_UTIL "ON")
set (USE_THREAD "THREAD_STDLIB")
set (USE_SERIALIZE "SERIALIZE_SERIALIZE")
set (USE_TRANSPORT "TRANSPORT_NONE")
include("${CMAKE_SOURCE_DIR}/Config.cmake")

# Collect all .cpp and *.h source files 
file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DELEGATEMQ_SOURCES})

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
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
    ${DELEGATE_ROOT_DIR}/test/unit-tests
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DELEGATEMQ_SOURCES})

target_link_libraries(${DELEGATE_APP} PRIVATE 
    unit_tests_lib
    sample_code_lib
)

# Add subdirectories for unit tests and sample code
add_subdirectory(unit-tests)
add_subdirectory(${CMAKE_SOURCE_DIR}/example/sample-code ${CMAKE_BINARY_DIR}/sample-code)

