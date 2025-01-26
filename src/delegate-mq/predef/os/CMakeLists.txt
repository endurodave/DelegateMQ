
file(GLOB SUBDIR_SOURCES "*.h" "stdlib/*.h" "stdlib/*.cpp")

# Create a library target
add_library(os_lib STATIC ${SUBDIR_SOURCES})

# Include directories for the library
target_include_directories(os_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

