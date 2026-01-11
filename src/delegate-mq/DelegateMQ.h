#ifndef _DELEGATE_MQ_H
#define _DELEGATE_MQ_H

// Delegate.h
// @see https://github.com/endurodave/DelegateMQ
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
/// fails within `operator()(Args... args)`. Alternatively, define `DMQ_ASSERTS` to use `assert`
/// as opposed to exceptions. All other delegate class functions do not throw exceptions.
/// 
/// Github repository location:  
/// https://github.com/endurodave/DelegateMQ
///
/// See README.md, DETAILS.md, EXAMPLES.md, and source code Doxygen comments for more information.

// -----------------------------------------------------------------------------
// 1. Core Non-Thread-Safe Delegates
// (Always available: Bare Metal, FreeRTOS, Windows, Linux)
// -----------------------------------------------------------------------------
#include "delegate/Delegate.h"
#include "delegate/DelegateRemote.h"
#include "delegate/MulticastDelegate.h"
#include "delegate/UnicastDelegate.h"
#include "delegate/Signal.h"

// -----------------------------------------------------------------------------
// 2. Thread-Safe Wrappers (Mutex Only)
// -----------------------------------------------------------------------------
// - FreeRTOS: Uses FreeRTOSRecursiveMutex
// - Bare Metal: Uses NullMutex
// - StdLib: Uses std::recursive_mutex
#if defined(DMQ_THREAD_STDLIB) || defined(DMQ_THREAD_FREERTOS) || defined(DMQ_THREAD_NONE)
    #include "delegate/MulticastDelegateSafe.h"
    #include "delegate/UnicastDelegateSafe.h"
    #include "delegate/SignalSafe.h"
#endif

// -----------------------------------------------------------------------------
// 3. Asynchronous "Fire and Forget" Delegates
// -----------------------------------------------------------------------------
// - FreeRTOS: OK (Requires you to implement IThread wrapper for Queues)
// - Bare Metal: OK (Requires you to implement IThread wrapper for Event Loop)
// - StdLib: OK
#if defined(DMQ_THREAD_STDLIB) || defined(DMQ_THREAD_FREERTOS) || defined(DMQ_THREAD_NONE)
    #include "delegate/DelegateAsync.h"
#endif

// -----------------------------------------------------------------------------
// 4. Asynchronous "Blocking" Delegates (Wait for Result)
// -----------------------------------------------------------------------------
// - FreeRTOS: NO (Requires Semaphore/Condition Variable)
// - Bare Metal: NO (Cannot block/sleep)
// - StdLib: OK
#if defined(DMQ_THREAD_STDLIB)
    #include "delegate/DelegateAsyncWait.h"
#endif

#if defined(DMQ_THREAD_STDLIB)
    #include "predef/os/stdlib/Thread.h"
    #include "predef/os/stdlib/ThreadMsg.h"
#elif defined(DMQ_THREAD_FREERTOS)
    #include "predef/os/freertos/Thread.h"
    #include "predef/os/freertos/ThreadMsg.h"
#elif defined(DMQ_THREAD_NONE)
    // Bare metal: User must implement their own polling/interrupt logic
#else
    #error "Thread implementation not found."
#endif

#if defined(DMQ_SERIALIZE_MSGPACK)
    #include "predef/serialize/msgpack/Serializer.h"
#elif defined(DMQ_SERIALIZE_CEREAL)
    #include "predef/serialize/cereal/Serializer.h"
#elif defined(DMQ_SERIALIZE_BITSERY)
    #include "predef/serialize/bitsery/Serializer.h"
#elif defined(DMQ_SERIALIZE_RAPIDJSON)
    #include "predef/serialize/rapidjson/Serializer.h"
#elif defined(DMQ_SERIALIZE_SERIALIZE)
    #include "predef/serialize/serialize/Serializer.h"
#elif defined(DMQ_SERIALIZE_NONE)
    // Create a custom application-specific serializer
#else
    #error "Serialize implementation not found."
#endif

#if defined(DMQ_TRANSPORT_ZEROMQ)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/zeromq/ZeroMqTransport.h"
#elif defined(DMQ_TRANSPORT_NNG)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/nng/NngTransport.h"
#elif defined(DMQ_TRANSPORT_WIN32_PIPE)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/win32-pipe/Win32PipeTransport.h"
#elif defined(DMQ_TRANSPORT_WIN32_UDP)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/win32-udp/Win32UdpTransport.h"
#elif defined(DMQ_TRANSPORT_LINUX_UDP)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/linux-udp/LinuxUdpTransport.h"
#elif defined(DMQ_TRANSPORT_MQTT)
    #include "predef/dispatcher/Dispatcher.h"
    #include "predef/transport/mqtt/MqttTransport.h"
#elif defined(DMQ_TRANSPORT_NONE)
    // Create a custom application-specific transport
#else
    #error "Transport implementation not found."
#endif

#if defined(DMQ_TRANSPORT_ZEROMQ) || defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP)
    #include "predef/util/NetworkEngine.h"
#endif

#include "predef/util/Fault.h"

// Only include Timer and AsyncInvoke if threads exist
#if !defined(DMQ_THREAD_NONE)
    #include "predef/util/Timer.h"
    #include "predef/util/AsyncInvoke.h"
    #include "predef/util/TransportMonitor.h"
#endif

#endif
