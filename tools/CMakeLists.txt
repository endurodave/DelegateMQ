cmake_minimum_required(VERSION 3.15)
project(DelegateMQ-Tools VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths relative to this tools/ directory
set(DELEGATE_MQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/delegate-mq")
set(WORKSPACE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(FTXUI_DIR   "${WORKSPACE_DIR}/ftxui")
set(BITSERY_DIR "${WORKSPACE_DIR}/bitsery")
set(SPDLOG_DIR  "${WORKSPACE_DIR}/spdlog")

# --- DelegateMQ configuration for tools ---
set(DMQ_THREAD    "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_BITSERY")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_NONE")
set(DMQ_UTIL      "ON")
set(DMQ_DATABUS   "ON")
set(DMQ_ASSERTS   "ON")

include("${DELEGATE_MQ_DIR}/DelegateMQ.cmake")

# --- FTXUI ---
if(EXISTS "${FTXUI_DIR}/CMakeLists.txt")
    add_subdirectory("${FTXUI_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/ftxui" EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "FTXUI not found at ${FTXUI_DIR}. Run 01_fetch_repos.py first.")
endif()

# --- spdlog ---
if(EXISTS "${SPDLOG_DIR}/CMakeLists.txt")
    add_subdirectory("${SPDLOG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/spdlog" EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "spdlog not found at ${SPDLOG_DIR}. Run 01_fetch_repos.py first.")
endif()

if(MSVC)
    add_compile_options("/utf-8")
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
endif()

set(COMMON_INCLUDES
    ${DMQ_INCLUDE_DIR}
    "${BITSERY_DIR}/include"
    "${SPDLOG_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# --- dmq-spy ---
add_executable(dmq-spy
    spy/main.cpp
    ${DMQ_LIB_SOURCES}
    ${DMQ_PREDEF_SOURCES}
)
target_include_directories(dmq-spy PRIVATE ${COMMON_INCLUDES})
target_link_libraries(dmq-spy PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    spdlog::spdlog
)

# --- dmq-monitor ---
add_executable(dmq-monitor
    monitor/monitor_main.cpp
    ${DMQ_LIB_SOURCES}
    ${DMQ_PREDEF_SOURCES}
)
target_include_directories(dmq-monitor PRIVATE
    ${COMMON_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
)
target_link_libraries(dmq-monitor PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# --- dmq-target (test source app) ---
add_executable(dmq-target
    target/TargetApp.cpp
    bridge/SpyBridge.cpp
    bridge/NodeBridge.cpp
    ${DMQ_LIB_SOURCES}
    ${DMQ_PREDEF_SOURCES}
)
target_include_directories(dmq-target PRIVATE
    ${COMMON_INCLUDES}
    "${CMAKE_CURRENT_SOURCE_DIR}/bridge"
)
target_link_libraries(dmq-target PRIVATE spdlog::spdlog)

# --- Platform-specific ---
if(WIN32)
    target_compile_definitions(dmq-spy     PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(dmq-spy          PRIVATE ws2_32)

    target_compile_definitions(dmq-monitor PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(dmq-monitor      PRIVATE ws2_32)

    target_compile_definitions(dmq-target  PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(dmq-target       PRIVATE ws2_32)
endif()
