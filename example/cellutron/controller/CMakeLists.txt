cmake_minimum_required(VERSION 3.16)
project(CellutronController VERSION 1.0 LANGUAGES CXX C)

# If not building from the top-level Cellutron project, define variables
if (NOT CELLUTRON_COMMON_DIR)
    set(CELLUTRON_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
endif()

# Set FreeRTOS thread port for this processor
set(DMQ_THREAD "DMQ_THREAD_FREERTOS")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_SERIALIZE")
set(DMQ_DATABUS "ON")

# Include DelegateMQ setup
include("${CMAKE_CURRENT_SOURCE_DIR}/../../../src/delegate-mq/DelegateMQ.cmake")
get_filename_component(DMQ_SRC_DIR "${DMQ_INCLUDE_DIR}" DIRECTORY)

set(TOOLS_BRIDGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/bridge")
set(TOOLS_SRC_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/src")

# Glob common sources but exclude SQLite
file(GLOB_RECURSE ALL_COMMON_SOURCES "${CELLUTRON_COMMON_DIR}/*.h" "${CELLUTRON_COMMON_DIR}/*.cpp" "${CELLUTRON_COMMON_DIR}/*.c")
set(COMMON_SOURCES ${ALL_COMMON_SOURCES})

# Ensure Network.cpp is included
list(APPEND COMMON_SOURCES "${CELLUTRON_COMMON_DIR}/util/Network.cpp")
list(APPEND COMMON_SOURCES "${CELLUTRON_COMMON_DIR}/util/RemoteConfig.cpp")
list(APPEND COMMON_SOURCES "${TOOLS_BRIDGE_DIR}/SpyBridge.cpp")
list(APPEND COMMON_SOURCES "${TOOLS_BRIDGE_DIR}/NodeBridge.cpp")
list(REMOVE_DUPLICATES COMMON_SOURCES)

foreach(FILE_PATH ${COMMON_SOURCES})
    if(FILE_PATH MATCHES "sqlite")
        list(REMOVE_ITEM COMMON_SOURCES ${FILE_PATH})
    endif()
endforeach()

# Controller application sources
file(GLOB SM_APP_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/process/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/actuators/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/*.cpp"
)

add_executable(cellutron_controller 
    main.cpp 
    system/System.cpp
    process/Process.cpp
    process/CellProcess.cpp
    process/PumpProcess.cpp
    actuators/Centrifuge.cpp
    ${COMMON_SOURCES}

    ${SM_APP_SOURCES}
    ${DMQ_LIB_SOURCES} 
    ${DMQ_PORT_SOURCES}
    ${FREERTOS_SOURCES}
)

target_compile_definitions(cellutron_controller PRIVATE ${DMQ_COMPILE_DEFINITIONS})

# Organize delegate source files within IDE (Visual Studio)
source_group("Delegate Files" FILES ${DMQ_LIB_SOURCES})
source_group("Port Files" FILES ${DMQ_PORT_ONLY_SOURCES})
source_group("Extras Files" FILES ${DMQ_EXTRAS_SOURCES})
source_group("FreeRTOS Files" FILES ${FREERTOS_SOURCES})
source_group("Process Files" FILES ${SM_APP_SOURCES})
source_group("Common Files" FILES ${COMMON_SOURCES})

target_include_directories(cellutron_controller PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CELLUTRON_COMMON_DIR}
    ${CELLUTRON_COMMON_DIR}/util
    ${CELLUTRON_COMMON_DIR}/messages
    ${DMQ_INCLUDE_DIR}
    ${DMQ_SRC_DIR}
    ${SPDLOG_INCLUDE_DIR}
    ${FREERTOS_ROOT_DIR}/include
    ${FREERTOS_ROOT_DIR}/portable/MSVC-MingW
    ${TOOLS_BRIDGE_DIR}
    ${TOOLS_SRC_DIR}
)
