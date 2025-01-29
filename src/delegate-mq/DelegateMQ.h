#ifndef _DELEGATE_MQ_H
#define _DELEGATE_MQ_H

// Delegate.h
// @see https://github.com/endurodave/cpp-async-delegate
// David Lafreniere, 2025.

/// @file
/// @brief DelegateMQ.h is a single include to obtain all delegate functionality. 
///
/// A C++ delegate library capable of invoking any callable function either synchronously   
/// or asynchronously on a user specified thread of control. It is also capable of calling
/// a function remotely over any transport protocol.
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
/// Github repository location:  
/// https://github.com/endurodave/cpp-async-delegate
///
/// See README.md, DETAILS.md, EXAMPLES.md, and source code Doxygen comments for more information.

#include "delegate/DelegateOpt.h"
#include "delegate/MulticastDelegateSafe.h"
#include "delegate/UnicastDelegate.h"
#include "delegate/DelegateAsync.h"
#include "delegate/DelegateAsyncWait.h"
#include "delegate/DelegateRemote.h"

#if defined(THREAD_STDLIB)
    #include "predef/os/stdlib/Thread.h"
    #include "predef/os/stdlib/ThreadMsg.h"
#elif defined(THREAD_NONE)
    // Create a custom application-specific thread
#else
    #error "Thread implemention not found."
#endif

#if defined(SERIALIZE_MSGPACK)
    #include "predef/serialize/msgpack/Serializer.h"
#elif defined(SERIALIZE_SERIALIZE)
    #include "predef/serialize/serialize/Serializer.h"
#elif defined(SERIALIZE_NONE)
    // Create a custom application-sepcific serializer
#else
    #error "Serialize implementation not found."
#endif

#if defined(TRANSPORT_MSGPACK)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/msgpack/Transport.h"
#elif defined(TRANSPORT_NONE)
    // Create a custom application-specific transport
#else
    #error "Transport implementation not found."
#endif

#include "predef/util/Fault.h"
#include "predef/util/Timer.h"
#include "predef/util/AsyncInvoke.h"

#endif
