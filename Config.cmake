set (DELEGATE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(EXISTS "${DELEGATE_ROOT_DIR}/Common.cmake")
    include ("${DELEGATE_ROOT_DIR}/Common.cmake")
else()
    message(FATAL_ERROR "Common.cmake not found.")
endif()

if(EXISTS "${DELEGATE_ROOT_DIR}/Predef.cmake")
    include ("${DELEGATE_ROOT_DIR}/Predef.cmake")
else()
    message(FATAL_ERROR "Predef.cmake not found.")
endif()

if (USE_EXTERNAL_LIB)
    if(EXISTS "${DELEGATE_ROOT_DIR}/External.cmake")
        include ("${DELEGATE_ROOT_DIR}/External.cmake")
    else()
        message(FATAL_ERROR "External.cmake not found.")
    endif()
endif()

if(EXISTS "${DELEGATE_ROOT_DIR}/Macros.cmake")
    include ("${DELEGATE_ROOT_DIR}/Macros.cmake")
else()
    message(FATAL_ERROR "Macros.cmake not found.")
endif()

