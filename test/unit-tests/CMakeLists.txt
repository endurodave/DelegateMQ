# Collect all .cpp files in this subdirectory
file(GLOB SUBDIR_SOURCES "*.cpp")

# Collect all .h files in this subdirectory
file(GLOB SUBDIR_HEADERS "*.h")

# Create a library target 
add_library(unit_tests_lib STATIC ${SUBDIR_SOURCES} ${SUBDIR_HEADERS})

# Include directories for the library
target_include_directories(unit_tests_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Add /bigobj flag for MSVC because unit tests are large
if (MSVC)
    target_compile_options(unit_tests_lib PRIVATE /bigobj)
endif()

target_precompile_headers(unit_tests_lib PRIVATE 
    "${CMAKE_SOURCE_DIR}/src/delegate-mq/DelegateMQ.h"
)