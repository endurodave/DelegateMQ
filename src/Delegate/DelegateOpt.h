#ifndef _DELEGATE_OPT_H
#define _DELEGATE_OPT_H

/// @file
/// @brief Delegate library options header file. 

// Define this macro to switch between assert or exception handling
//#define USE_ASSERTS  // Comment this out to use asserts

#ifdef USE_ASSERTS
    #include <cassert>
    // Use assert error handling. Change assert to a different error 
    // handler as required by the target application.
    #define BAD_ALLOC() assert(false && "Memory allocation failed!")
#else
    #include <new>
    // Use exception error handling
    #define BAD_ALLOC() throw std::bad_alloc()
#endif

// If USE_ASSERTS defined above, consider defining USE_ALLOCATOR to prevent 
// std::list usage within delegate library from throwing a std::bad_alloc 
// exception. The std_allocator calls assert if out of memory. 
// See master CMakeLists.txt for info on enabling the fixed-block allocator.
#ifdef USE_ALLOCATOR
    // Use stl_allocator fixed-block allocator for dynamic storage allocation
    #include "xlist.h"
    #include "stl_allocator.h"
#else
    #include <list>

    // Use default std::allocator for dynamic storage allocation
    template <typename T, typename Alloc = std::allocator<T>>
    using xlist = std::list<T, Alloc>;

    #define XALLOCATOR
#endif

#endif
