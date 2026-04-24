#ifndef _XSTRING_H
#define _XSTRING_H

#include "stl_allocator.h"
#include <string>

namespace dmq {
    typedef std::basic_string<char, std::char_traits<char>, stl_allocator<char> > xstring;
}
// xwstring is unused by DelegateMQ. Uncomment if wide string support is needed by the application.
// typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, stl_allocator<wchar_t> > xwstring;

#endif 

