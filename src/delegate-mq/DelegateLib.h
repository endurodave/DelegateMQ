#ifndef _DELEGATE_LIB_H
#define _DELEGATE_LIB_H

// Delegate.h
// @see https://github.com/endurodave/cpp-async-delegate
// David Lafreniere, Aug 2020.

/// @file
/// @brief DelegateLib.h is a single include to obtain all delegate functionality. 
///
/// A C++ delegate library capable of invoking any callable function either synchronously   
/// or asynchronously on a user specified thread of control.
/// 
/// Asynchronous function calls support both non-blocking and blocking modes with a timeout. 
/// The library supports all types of target functions, including free functions, class 
/// member functions, static class functions, lambdas, and `std::function`. It is capable of 
/// handling any function signature, regardless of the number of arguments or return value. 
/// All argument types are supported, including by value, pointers, pointers to pointers, 
/// and references. The delegate library takes care of the intricate details of function 
/// invocation across thread boundaries. Thread-safe delegate containers stores delegate 
/// instances with a matching function signature.
/// 
///  A delegate instance can be:
///
/// * Copied freely.
/// * Compared to same type delegatesand `nullptr`.
/// * Reassigned.
/// * Called.
/// 
/// Typical use cases are:
///
/// * Asynchronous Method Invocation(AMI)
/// * Publish / Subscribe(Observer) Pattern
/// * Anonymous, Asynchronous Thread - Safe Callbacks
/// * Event - Driven Programming
/// * Thread - Safe Asynchronous API
/// * Design Patterns(Active Object)
///
/// The delegate library's asynchronous features differ from `std::async` in that the 
/// caller-specified thread of control is used to invoke the target function bound to 
/// the delegate, rather than a random thread from the thread pool. The asynchronous 
/// variants copy the argument data into the event queue, ensuring safe transport to the 
/// destination thread, regardless of the argument type. This approach provides 'fire and 
/// forget' functionality, allowing the caller to avoid waiting or worrying about 
/// out-of-scope stack variables being accessed by the target thread.
/// 
/// The `Async` and `AsyncWait` class variants may throw `std::bad_alloc` if heap allocation 
/// fails within `operator()(Args... args)`. Alternatively, define `USE_ASSERTS` to use `assert`
/// as opposed to exceptions. All other delegate class functions do not throw exceptions.
/// 
/// Github responsitory location:  
/// https://github.com/endurodave/cpp-async-delegate
///
/// See README.md, DETAILS.md, and source code Doxygen comments for more information.

#include "DelegateOpt.h"
#include "MulticastDelegateSafe.h"
#include "UnicastDelegate.h"
#include "DelegateAsync.h"
#include "DelegateAsyncWait.h"
#include "DelegateRemote.h"

#endif
