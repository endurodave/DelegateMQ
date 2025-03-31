#ifndef _ASYNC_INVOKE_H
#define _ASYNC_INVOKE_H

class Thread;

/// @file
/// @brief Helper functions to simplify invoking a free or member function
/// on the user-specified thread of control using a single line of code.
/// Particually useful for creating asynhronous APIs (see AsyncAPI.cpp). 
/// 
/// size_t send_data(const std::string& data) {
///    return AsyncInvoke(send_data_private, comm_thread, WAIT_INFINITE, data);
/// }

/// Helper function to simplify asynchronous invoke of a free function. If this
/// function is called by `thread` the function is invoked synchronously. Otherwise
/// an asynchronous delegate is created to invoke the funciton on `thread` and 
/// block waiting for the target function call to complete before returning.
/// @param[in] func - the function to invoke
/// @param[in] thread - the thread to invoke func
/// @param[in] timeout - the time to wait for invoke to complete
/// @param[in] args - the function argument(s) passed to func
template <class Func, class... Args>
auto AsyncInvoke(Func func, Thread& thread, const dmq::Duration& timeout, Args&&... args) {
    // Deduce return type of func
    using RetType = decltype(func(std::forward<Args>(args)...));

    // Is the calling function executing on the requested thread?
    if (thread.GetThreadId() != Thread::GetCurrentThreadId()) {
        // Create a delegate that points to func to invoke on thread
        auto delegate = dmq::MakeDelegate(func, thread, timeout);

        // Invoke the delegate target function asynchronously and wait for function call to complete
        auto retVal = delegate.AsyncInvoke(std::forward<Args>(args)...);

        // Target function has a return value?
        if constexpr (std::is_void<RetType>::value == false) {
            // Did async function call succeed?
            if (retVal.has_value()) {
                // Return the return value to caller
                return std::any_cast<RetType>(retVal.value());
            }
            else {
                return RetType();
            }
        }
    }
    else {
        // Invoke target function synchronously
        return std::invoke(func, std::forward<Args>(args)...);
    }
}

/// Helper function to simplify asynchronous invoke of a member function. If this
/// function is called by `thread` the function is invoked synchronously. Otherwise
/// an asynchronous delegate is created to invoke the funciton on `thread` and 
/// block waiting for the target function call to complete before returning.
/// @param[in] tclass - the class instance
/// @param[in] func - the function to invoke
/// @param[in] thread - the thread to invoke func
/// @param[in] timeout - the time to wait for invoke to complete
/// @param[in] args - the function argument(s) passed to func
template <class TClass, class Func, class... Args>
auto AsyncInvoke(TClass tclass, Func func, Thread& thread, const dmq::Duration& timeout, Args&&... args) {
    // Deduce return type of func
    using RetType = decltype((tclass->*func)(std::forward<Args>(args)...));

    // Is the calling function executing on the requested thread?
    if (thread.GetThreadId() != Thread::GetCurrentThreadId()) {
        // Create a delegate that points to func to invoke on thread
        auto delegate = dmq::MakeDelegate(tclass, func, thread, timeout);

        // Invoke the delegate target function asynchronously and wait for function call to complete
        auto retVal = delegate.AsyncInvoke(std::forward<Args>(args)...);

        // Target function has a return value?
        if constexpr (std::is_void<RetType>::value == false) {
            // Did async function call succeed?
            if (retVal.has_value()) {
                // Return the return value to caller
                return std::any_cast<RetType>(retVal.value());
            }
            else {
                return RetType();
            }
        }
    }
    else {
        // Invoke target function synchronously
        return std::invoke(func, tclass, std::forward<Args>(args)...);
    }

    return RetType();
}

#endif