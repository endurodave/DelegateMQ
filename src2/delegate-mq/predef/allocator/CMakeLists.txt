file(GLOB SUBDIR_SOURCES "*.h" "*.cpp")

# Create a library target
add_library(allocator_lib STATIC ${SUBDIR_SOURCES})

# Include directories for the library
target_include_directories(allocator_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")