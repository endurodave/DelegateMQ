cmake_minimum_required(VERSION 3.15)
project(ExampleBuild)

# Enable testing
enable_testing()

# Get the current source and binary directories
set(TOP_LEVEL_DIR ${CMAKE_SOURCE_DIR})

# Add subprojects with absolute paths
add_subdirectory(${TOP_LEVEL_DIR}/sample-projects/bare-metal ${CMAKE_BINARY_DIR}/bare-metal)
add_subdirectory(${TOP_LEVEL_DIR}/sample-projects/system-architecture/client ${CMAKE_BINARY_DIR}/system-architecture/client)
add_subdirectory(${TOP_LEVEL_DIR}/sample-projects/system-architecture/server ${CMAKE_BINARY_DIR}/system-architecture/server)
add_subdirectory(${TOP_LEVEL_DIR}/sample-projects/zeromq-msgpack-cpp ${CMAKE_BINARY_DIR}/zeromq-msgpack-cpp)
add_subdirectory(${TOP_LEVEL_DIR}/sample-projects/zeromq-serializer ${CMAKE_BINARY_DIR}/zeromq-serializer)
