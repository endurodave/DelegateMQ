#ifndef _XLIST_H
#define _XLIST_H

#include "stl_allocator.h"
#include <list>

// xlist uses a fix-block memory allocator
template <typename T, typename Alloc = stl_allocator<T>>
using xlist = std::list<T, Alloc>;

#if 0  // Deprecated
template<class _Ty,
	class _Ax = stl_allocator<_Ty> >
	class xlist
		: public std::list<_Ty, _Ax>
	{
	};
#endif

#endif
	
