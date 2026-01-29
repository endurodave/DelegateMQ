#ifndef _DELEGATE_OPT_H
#define _DELEGATE_OPT_H

/// @file
/// @brief Delegate library options header file.

#include <chrono>
#include <mutex>

#if defined(DMQ_THREAD_FREERTOS)
    #include "predef/util/FreeRTOSClock.h"
    #include "predef/util/FreeRTOSMutex.h"
#elif defined(DMQ_THREAD_THREADX)
    #include "predef/util/ThreadXClock.h"
    #include "predef/util/ThreadXMutex.h"
#elif defined(DMQ_THREAD_CMSIS_RTOS2)
    #include "predef/util/CmsisRtos2Clock.h"
    #include "predef/util/CmsisRtos2Mutex.h"
#elif defined(DMQ_THREAD_NONE)
    #include "predef/util/BareMetalClock.h"
#endif

namespace dmq
{
    // @TODO: Change aliases to switch clock type globally if necessary

    // --- CLOCK SELECTION ---
#if defined(DMQ_THREAD_FREERTOS)
    // Use the custom FreeRTOS wrapper
    using Clock = dmq::FreeRTOSClock;

#elif defined(DMQ_THREAD_THREADX)
    // Use the custom ThreadX wrapper
    using Clock = dmq::ThreadXClock;

#elif defined(DMQ_THREAD_ZEPHYR)
    // Use the custom Zephyr wrapper
    using Clock = dmq::ZephyrClock;

#elif defined(DMQ_THREAD_NONE)
    // Assuming implemented the 'g_ticks' variable
    using Clock = dmq::BareMetalClock;

 #elif defined(DMQ_THREAD_CMSIS_RTOS2)
    using Clock = dmq::CmsisRtos2Clock;

#else
    // Windows / Linux / macOS / Qt
    using Clock = std::chrono::steady_clock;
#endif

    // --- GENERIC TYPES ---
    // Automatically adapt to the underlying Clock's traits
    using Duration = typename Clock::duration;
    using TimePoint = typename Clock::time_point;

    // --- MUTEX / LOCK SELECTION ---
#if defined(DMQ_THREAD_FREERTOS)
    // Use the custom FreeRTOS wrapper
    using Mutex = dmq::FreeRTOSMutex;
    using RecursiveMutex = dmq::FreeRTOSRecursiveMutex;

#elif defined(DMQ_THREAD_THREADX)
    // Use the custom ThreadX wrapper
    using Mutex = dmq::ThreadXMutex;
    using RecursiveMutex = dmq::ThreadXRecursiveMutex;

#elif defined(DMQ_THREAD_ZEPHYR)
    // Use the custom Zephyr wrapper
    using Mutex = dmq::ZephyrMutex;
    using RecursiveMutex = dmq::ZephyrRecursiveMutex;

#elif defined(DMQ_THREAD_CMSIS_RTOS2)
    using Mutex = dmq::CmsisRtos2Mutex;
    using RecursiveMutex = dmq::CmsisRtos2RecursiveMutex;

#elif defined(DMQ_THREAD_NONE)
    // Bare metal has no threads, so no locking is required.
    // We define a dummy "No-Op" mutex.
    struct NullMutex {
        void lock() {}
        void unlock() {}
    };
    using Mutex = NullMutex;
    using RecursiveMutex = NullMutex;

#else
    // Windows / Linux / macOS / Qt
    using Mutex = std::mutex;
    using RecursiveMutex = std::recursive_mutex;
#endif
}

// @TODO: Select the desired software fault handling (see Predef.cmake).
#ifdef DMQ_ASSERTS
    #include <cassert>
    // Use assert error handling. Change assert to a different error 
    // handler as required by the target application.
    #define BAD_ALLOC() assert(false && "Memory allocation failed!")
#else
    #include <new>
    // Use exception error handling
    #define BAD_ALLOC() throw std::bad_alloc()
#endif

// @TODO: Select the desired heap allocation (see Predef.cmake).
// If DMQ_ASSERTS defined above, consider defining DMQ_ALLOCATOR to prevent 
// std::list usage within delegate library from throwing a std::bad_alloc 
// exception. The std_allocator calls assert if out of memory. 
// See master CMakeLists.txt for info on enabling the fixed-block allocator.
#ifdef DMQ_ALLOCATOR
    // Use stl_allocator fixed-block allocator for dynamic storage allocation
    #include "predef/allocator/xlist.h"
    #include "predef/allocator/xsstream.h"
    #include "predef/allocator/stl_allocator.h"
#else
    #include <list>
    #include <sstream>

    // Not using xallocator; define as nothing
    #undef XALLOCATOR
    #define XALLOCATOR

    // Use default std::allocator for dynamic storage allocation
    template <typename T, typename Alloc = std::allocator<T>>
    class xlist : public std::list<T, Alloc> {
    public:
        using std::list<T, Alloc>::list; // Inherit constructors
        using std::list<T, Alloc>::operator=;
    };

    typedef std::basic_ostringstream<char, std::char_traits<char>> xostringstream;
    typedef std::basic_stringstream<char, std::char_traits<char>> xstringstream;
#endif

// @TODO: Select the desired logging (see Predef.cmake).
#ifdef DMQ_LOG
    #include <spdlog/spdlog.h>
    #define LOG_INFO(...)    spdlog::info(__VA_ARGS__)
    #define LOG_DEBUG(...)   spdlog::debug(__VA_ARGS__)
    #define LOG_ERROR(...)   spdlog::error(__VA_ARGS__)
#else
    // No-op macros when logging disabled
    #define LOG_INFO(...)    do {} while(0)
    #define LOG_DEBUG(...)   do {} while(0)
    #define LOG_ERROR(...)   do {} while(0)
#endif

#endif
