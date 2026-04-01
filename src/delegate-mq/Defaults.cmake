# DelegateMQ Defaults
# 
# This module auto-selects default DelegateMQ build options based on the target platform.
# Options can be overridden by setting the variables before including DelegateMQ.cmake.

# --- Threading Defaults ---
if(NOT DEFINED DMQ_THREAD)
    if(WIN32 OR UNIX)
        set(DMQ_THREAD "DMQ_THREAD_STDLIB")
    elseif(FREERTOS OR FREE_RTOS)
        set(DMQ_THREAD "DMQ_THREAD_FREERTOS")
    elseif(THREADX)
        set(DMQ_THREAD "DMQ_THREAD_THREADX")
    elseif(ZEPHYR)
        set(DMQ_THREAD "DMQ_THREAD_ZEPHYR")
    elseif(CMSIS_RTOS2)
        set(DMQ_THREAD "DMQ_THREAD_CMSIS_RTOS2")
    else()
        set(DMQ_THREAD "DMQ_THREAD_NONE")
    endif()
endif()

# --- Transport Defaults ---
if(NOT DEFINED DMQ_TRANSPORT)
    if(WIN32)
        set(DMQ_TRANSPORT "DMQ_TRANSPORT_WIN32_UDP")
    elseif(UNIX AND NOT APPLE)
        set(DMQ_TRANSPORT "DMQ_TRANSPORT_LINUX_UDP")
    else()
        set(DMQ_TRANSPORT "DMQ_TRANSPORT_NONE")
    endif()
endif()

# --- Serialization Defaults ---
if(NOT DEFINED DMQ_SERIALIZE)
    set(DMQ_SERIALIZE "DMQ_SERIALIZE_SERIALIZE")
endif()

# --- Utility Class Defaults ---
if(NOT DEFINED DMQ_UTIL)
    set(DMQ_UTIL "ON")
endif()

# --- DataBus Defaults ---
if(NOT DEFINED DMQ_DATABUS)
    set(DMQ_DATABUS "ON")
endif()

# --- Allocator Defaults ---
if(NOT DEFINED DMQ_ALLOCATOR)
    set(DMQ_ALLOCATOR "OFF")
endif()

# --- Assert Defaults ---
if(NOT DEFINED DMQ_ASSERTS)
    set(DMQ_ASSERTS "OFF")
endif()

# --- Logging Defaults ---
if(NOT DEFINED DMQ_LOG)
    set(DMQ_LOG "OFF")
endif()
