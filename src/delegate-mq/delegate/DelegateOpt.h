#ifndef _DELEGATE_OPT_H
#define _DELEGATE_OPT_H

/// @file
/// @brief Delegate library options header file.

#include <chrono>

namespace dmq
{
    using Duration = std::chrono::duration<uint32_t, std::milli>;
}

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

    // Use default std::allocator for dynamic storage allocation
    template <typename T, typename Alloc = std::allocator<T>>
    using xlist = std::list<T, Alloc>;

    typedef std::basic_ostringstream<char, std::char_traits<char>> xostringstream;
    typedef std::basic_stringstream<char, std::char_traits<char>> xstringstream;

    // Not using xallocator; define as nothing
    #define XALLOCATOR
#endif

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
