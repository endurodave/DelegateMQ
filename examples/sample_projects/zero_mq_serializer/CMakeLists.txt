# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
# cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DENABLE_ALLOCATOR=ON

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(Delegate VERSION 1.0 LANGUAGES CXX)

set(DELEGATE_ROOT_DIR "../../../src")

# Path to the vcpkg directory (for ZeroMQ library)
set(VCPKG_ROOT_DIR "../../../../vcpkg/installed/x64-windows")
set(VCPKG_BIN_DIR "${VCPKG_ROOT_DIR}/bin")
set(VCPKG_LIB_DIR "${VCPKG_ROOT_DIR}/lib")
set(VCPKG_INCLUDE_DIR "${VCPKG_ROOT_DIR}/include")

# Check if the file exists
if(EXISTS "${VCPKG_LIB_DIR}/czmq.lib")
    message(STATUS "File czmq.lib exists!")
else()
    message(FATAL_ERROR "File czmq.lib does NOT exist! Check VCPKG_ROOT_DIR path.")
endif()

# Set C++ standard 
# C++17 minimum required. C++20 tested and works too.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect all header files in the src/Delegate directory
file(GLOB Delegate_HEADERS "${CMAKE_SOURCE_DIR}/${DELEGATE_ROOT_DIR}/delegate/*.h")

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
    ${VCPKG_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/${DELEGATE_ROOT_DIR}/delegate
    ${CMAKE_SOURCE_DIR}/${DELEGATE_ROOT_DIR}/port
)

if (ENABLE_ALLOCATOR)
include_directories( 
    ${CMAKE_SOURCE_DIR}/${DELEGATE_ROOT_DIR}/Allocator
)
endif()

# Add an executable target
add_executable(delegate_app ${SOURCES} ${Delegate_HEADERS})

if (ENABLE_ALLOCATOR)
    add_compile_definitions(USE_ALLOCATOR)
endif()

# Add subdirectories to build
add_subdirectory(${DELEGATE_ROOT_DIR}/port ${CMAKE_BINARY_DIR}/port)

if (ENABLE_ALLOCATOR)
    add_subdirectory(${DELEGATE_ROOT_DIR}/allocator ${CMAKE_BINARY_DIR}/allocator)
endif()

target_link_libraries(delegate_app PRIVATE 
    ../${VCPKG_LIB_DIR}/czmq
    ../${VCPKG_LIB_DIR}/libzmq-mt-4_3_5
    port_lib
)

if (ENABLE_ALLOCATOR)
    target_link_libraries(delegate_app PRIVATE 
        allocator_lib
)
endif()

# Get all .dll files in the vcpkg bin directory
file(GLOB ZMQ_BIN_FILES "${VCPKG_BIN_DIR}/*.dll" "${VCPKG_BIN_DIR}/*.pdb")

# Copy each DLL file to the build output directory
foreach(DLL_FILE ${ZMQ_BIN_FILES})
    add_custom_command(TARGET delegate_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLL_FILE}  # Copy the DLL file
        "${CMAKE_BINARY_DIR}/Debug"  # Destination directory
        COMMENT "Copying ${DLL_FILE} to build output"
    )
endforeach()

