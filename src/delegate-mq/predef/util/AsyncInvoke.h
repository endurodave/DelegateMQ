#ifndef ASYNC_INVOKE_H
#define ASYNC_INVOKE_H

#include "DelegateMQ.h"
#include <functional>
#include <type_traits>
#include <utility>
#include <any>

/// @file
/// @brief Helper functions to simplify invoking a free or member function
/// on the user-specified thread of control using a single line of code.

/// Helper function to simplify asynchronous invoke of a free function/lambda.
/// @param[in] func - the function/lambda to invoke
/// @param[in] thread - the thread to invoke func on
/// @param[in] timeout - the time to wait for invoke to complete
/// @param[in] args - the function argument(s) passed to func
template <class Func, class... Args>
auto AsyncInvoke(Func func, Thread& thread, const dmq::Duration& timeout, Args&&... args)
{
    // Deduce return type
    using RetType = decltype(func(std::forward<Args>(args)...));

    // Is the calling function executing on the requested thread?
    if (thread.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Explicitly convert lambda 'func' to std::function.
        // MakeDelegate cannot deduce std::function from a raw lambda.
        // We use the deduced argument types (Args...) to match the delegate signature.
        std::function<RetType(Args...)> typedFunc = func;

        // Create a delegate that points to func to invoke on thread
        auto delegate = dmq::MakeDelegate(typedFunc, thread, timeout);

        // Invoke the delegate target function asynchronously and wait for completion
        auto retVal = delegate.AsyncInvoke(std::forward<Args>(args)...);

        // Target function has a return value?
        if constexpr (!std::is_void_v<RetType>)
        {
            if (retVal.has_value()) {
                return std::any_cast<RetType>(retVal.value());
            }
            else {
                return RetType{};
            }
        }
    }
    else
    {
        // Invoke target function synchronously
        return func(std::forward<Args>(args)...);
    }
}

/// Helper function to simplify asynchronous invoke of a member function.
/// @param[in] tclass - the class instance (pointer)
/// @param[in] func - the member function pointer
/// @param[in] thread - the thread to invoke func on
/// @param[in] timeout - the time to wait for invoke to complete
/// @param[in] args - the function argument(s) passed to func
template <class TClass, class Func, class... Args>
auto AsyncInvoke(TClass* tclass, Func func, Thread& thread, const dmq::Duration& timeout, Args&&... args)
{
    // Deduce return type using std::invoke (robust for member pointers)
    using RetType = decltype(std::invoke(func, tclass, std::forward<Args>(args)...));

    if (thread.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Create delegate. MakeDelegate handles member pointers correctly.
        auto delegate = dmq::MakeDelegate(tclass, func, thread, timeout);

        auto retVal = delegate.AsyncInvoke(std::forward<Args>(args)...);

        if constexpr (!std::is_void_v<RetType>)
        {
            if (retVal.has_value()) {
                return std::any_cast<RetType>(retVal.value());
            }
            else {
                return RetType{};
            }
        }
    }
    else
    {
        return std::invoke(func, tclass, std::forward<Args>(args)...);
    }
}

#endif