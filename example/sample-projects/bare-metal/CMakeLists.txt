# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set (DELEGATE_APP "delegate_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set build options
set (USE_EXTERNAL_LIB "OFF")
set (USE_ALLOCATOR "OFF")
set (USE_UTIL "ON")
set (USE_THREAD "THREAD_STDLIB")
set (USE_SERIALIZE "SERIALIZE_NONE")
set (USE_TRANSPORT "TRANSPORT_NONE")
include("${CMAKE_SOURCE_DIR}/../../../Config.cmake")

# Collect all .cpp and *.h source files in the src directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${PREDEF_SOURCES})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DELEGATEMQ_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${DELEGATE_ROOT_DIR}/src/delegate-mq
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DELEGATEMQ_SOURCES})



