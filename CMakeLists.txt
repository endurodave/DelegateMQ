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

# Set C++ standard 
# C++17 minimum required. C++20 tested and works too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h")

# Collect all header files in the src/Delegate directory
file(GLOB Delegate_HEADERS "${CMAKE_SOURCE_DIR}/src/Delegate/*.h")

# Organize Delegate headers into a "Delegate Files" folder in Visual Studio
source_group("Delegate Files" FILES ${Delegate_HEADERS})

# Check if the TEST_COVERAGE argument is passed
option(TEST_COVERAGE "Enable code coverage" OFF)

if(TEST_COVERAGE)
  message(STATUS "Code coverage is enabled")

  # Add coverage flags for GCC/Clang compilers
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/src/Delegate
    ${CMAKE_SOURCE_DIR}/src/Examples
    ${CMAKE_SOURCE_DIR}/src/Port
    ${CMAKE_SOURCE_DIR}/tests/UnitTests
)

if (ENABLE_ALLOCATOR)
include_directories( 
    ${CMAKE_SOURCE_DIR}/src/Allocator
)
endif()

# Add an executable target
add_executable(DelegateApp ${SOURCES} ${Delegate_HEADERS})

if (ENABLE_ALLOCATOR)
    add_compile_definitions(USE_ALLOCATOR)
endif()

# Add subdirectories to build
add_subdirectory(src/Examples)
add_subdirectory(src/Port)
add_subdirectory(tests/UnitTests)

if (ENABLE_ALLOCATOR)
    add_subdirectory(src/Allocator)
endif()

target_link_libraries(DelegateApp PRIVATE 
    ExamplesLib
    PortLib
    UnitTestsLib
)

if (ENABLE_ALLOCATOR)
    target_link_libraries(DelegateApp PRIVATE 
        AllocatorLib
)
endif()

