set (DMQ_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(EXISTS "${DMQ_ROOT_DIR}/Common.cmake")
    include ("${DMQ_ROOT_DIR}/Common.cmake")
else()
    message(FATAL_ERROR "Common.cmake not found.")
endif()

if(EXISTS "${DMQ_ROOT_DIR}/Predef.cmake")
    include ("${DMQ_ROOT_DIR}/Predef.cmake")
else()
    message(FATAL_ERROR "Predef.cmake not found.")
endif()

if (DMQ_EXTERNAL_LIB)
    if(EXISTS "${DMQ_ROOT_DIR}/External.cmake")
        include ("${DMQ_ROOT_DIR}/External.cmake")
    else()
        message(FATAL_ERROR "External.cmake not found.")
    endif()
endif()

if(EXISTS "${DMQ_ROOT_DIR}/Macros.cmake")
    include ("${DMQ_ROOT_DIR}/Macros.cmake")
else()
    message(FATAL_ERROR "Macros.cmake not found.")
endif()

