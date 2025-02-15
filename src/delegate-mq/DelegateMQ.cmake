# DelegateMQ cmake module 
#
# This module sets the following variables in your project:
#
#   DMQ_INCLUDE_DIR - the directory containing DelegateMQ headers.
#   DMQ_LIB_SOURCES - the core DelegateMQ delegate library files.
#   DMQ_PREDEF_SOURCES - the predefined supporting source code files 
#   based on the DMQ build options.
#
# Set DMQ build options:
#
#   # Set DMQ build options. Update as necessary.
#   set(DMQ_ALLOCATOR "OFF")
#   set(DMQ_UTIL "ON")
#   set(DMQ_THREAD "DMQ_THREAD_STDLIB")
#   set(DMQ_SERIALIZE "DMQ_SERIALIZE_NONE")
#   set(DMQ_TRANSPORT "DMQ_TRANSPORT_NONE")
#   include("${CMAKE_SOURCE_DIR}/../../../src/delegate-mq/DelegateMQ.cmake")
#
# Use variables to build:
#
#   # Collect DelegateMQ predef source files
#   list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})
#
#   # Add include directory
#   include_directories(${DMQ_INCLUDE_DIR})

macro(check _file)
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist!")
    endif()
endmacro()

macro(set_and_check _var _file)
    set(${_var} "${_file}")
    check("${_file}")
endmacro()

set_and_check(DMQ_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set_and_check(DMQ_INCLUDE_DIR "${DMQ_ROOT_DIR}")

check("${DMQ_ROOT_DIR}/Macros.cmake")
include ("${DMQ_ROOT_DIR}/Macros.cmake")

check("${DMQ_ROOT_DIR}/Common.cmake")
include ("${DMQ_ROOT_DIR}/Common.cmake")

check("${DMQ_ROOT_DIR}/Predef.cmake")
include ("${DMQ_ROOT_DIR}/Predef.cmake")

check("${DMQ_ROOT_DIR}/External.cmake")
include ("${DMQ_ROOT_DIR}/External.cmake")

