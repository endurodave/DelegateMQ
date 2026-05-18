#ifndef _COROUTINE_H
#define _COROUTINE_H

#if defined(_MSVC_LANG) && _MSVC_LANG >= 202002L || __cplusplus >= 202002L

namespace Example
{
    /// Execute the C++20 coroutine with delegates example
    void CoroutineExample();
}

#endif  // C++20

#endif  // _COROUTINE_H
