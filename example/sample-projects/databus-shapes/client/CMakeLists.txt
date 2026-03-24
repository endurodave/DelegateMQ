# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.16)

set(DELEGATE_APP "delegate_databus_shapes_client")
project(DelegateDataBusShapesClient VERSION 1.0 LANGUAGES CXX)

# Set build options
set(DMQ_ASSERTS "ON")
set(DMQ_ALLOCATOR "OFF")
set(DMQ_LOG "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_SERIALIZE")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")
set(DMQ_DATABUS "ON")
option(DMQ_DATABUS_SPY "Enable DataBus Spy support" OFF)

include("${CMAKE_SOURCE_DIR}/../../../../src/delegate-mq/DelegateMQ.cmake")

# FTXUI & spdlog integration (from peer workspace folders)
set(WORKSPACE_DIR "${CMAKE_SOURCE_DIR}/../../../../..")
set(FTXUI_DIR "${WORKSPACE_DIR}/ftxui")
set(SPDLOG_DIR "${WORKSPACE_DIR}/spdlog")

if(EXISTS "${FTXUI_DIR}/CMakeLists.txt")
    add_subdirectory("${FTXUI_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ftxui" EXCLUDE_FROM_ALL)
endif()
if(EXISTS "${SPDLOG_DIR}/CMakeLists.txt")
    add_subdirectory("${SPDLOG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spdlog" EXCLUDE_FROM_ALL)
endif()

if (MSVC)
    add_compile_options("/utf-8")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

# Collect all .cpp and *.h source files in this directory
file(GLOB SOURCES "*.cpp" "*.h")

# Collect Common files to show them in VS Solution Explorer
file(GLOB COMMON_SOURCES "${CMAKE_SOURCE_DIR}/../common/*.h" "${CMAKE_SOURCE_DIR}/../common/*.cpp")
set_source_files_properties(${COMMON_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)

# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})

# Organize source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Predef Files" FILES ${DMQ_PREDEF_SOURCES})
source_group("Common Files" FILES ${COMMON_SOURCES})

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/../common
    ${DMQ_INCLUDE_DIR}
    "${SPDLOG_DIR}/include"
)

# Add an executable target
add_executable(${DELEGATE_APP} ${SOURCES} ${DMQ_LIB_SOURCES} ${COMMON_SOURCES})

target_link_libraries(${DELEGATE_APP} PRIVATE 
    ftxui::screen 
    ftxui::dom 
    ftxui::component
    spdlog::spdlog
)

# Link libraries based on platform
find_package(Threads REQUIRED)
if(WIN32)
    target_link_libraries(${DELEGATE_APP} PRIVATE ws2_32 Threads::Threads)
else()
    target_link_libraries(${DELEGATE_APP} PRIVATE Threads::Threads)
endif()
