# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

set(DELEGATE_APP "delegate_pub_app")
project(Delegate VERSION 1.0 LANGUAGES CXX)

# Set build options
set(DMQ_ALLOCATOR "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_RAPIDJSON")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_MQTT")
include("${CMAKE_SOURCE_DIR}/../../../../src/delegate-mq/DelegateMQ.cmake")

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
    ${MQTT_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES})

# Find the libraries in the specified directory
find_library(MQTT_LIB_A paho-mqtt3a HINTS ${MQTT_LIBRARY_DIR})
find_library(MQTT_LIB_C paho-mqtt3c HINTS ${MQTT_LIBRARY_DIR})

# Check if libraries were found
if(NOT MQTT_LIB_A OR NOT MQTT_LIB_C)
    message(FATAL_ERROR "Could not find MQTT libraries in ${MQTT_LIBRARY_DIR}")
endif()

# Link the libraries
target_link_libraries(${DELEGATE_APP} PRIVATE 
    ${MQTT_LIB_A}
    ${MQTT_LIB_C}
)




