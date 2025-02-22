#ifndef _DELEGATE_OPT_H
#define _DELEGATE_OPT_H

/// @file
/// @brief Delegate library options header file.

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

#endif
