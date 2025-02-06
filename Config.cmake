set (DELEGATE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(EXISTS "${DELEGATE_ROOT_DIR}/Common.cmake")
    include ("${DELEGATE_ROOT_DIR}/Common.cmake")
else()
    message(FATAL_ERROR "Common.cmake not found.")
endif()

if(EXISTS "${DELEGATE_ROOT_DIR}/External.cmake")
    include ("${DELEGATE_ROOT_DIR}/External.cmake")
else()
    message(FATAL_ERROR "External.cmake not found.")
endif()

if(EXISTS "${DELEGATE_ROOT_DIR}/Macros.cmake")
    include ("${DELEGATE_ROOT_DIR}/Macros.cmake")
else()
    message(FATAL_ERROR "Macros.cmake not found.")
endif()

# Specify the directory containing the pre-built delegate_mq.lib
set(DELEGATE_LIB_DIR "${DELEGATE_ROOT_DIR}/lib")

# Check if the delegate_mq.lib exists in the specified directory
set(DELEGATE_LIB_NAME "delegate_mq.lib")
if (NOT EXISTS "${DELEGATE_LIB_DIR}/${DELEGATE_LIB_NAME}")
    #message(FATAL_ERROR "Error: ${DELEGATE_LIB_NAME} not found in ${DELEGATE_LIB_DIR}. Please ensure the library is pre-built and available.")
else()
    message(STATUS "Found ${DELEGATE_LIB_NAME} in ${DELEGATE_LIB_DIR}")
endif()