![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml)
[![Codecov](https://codecov.io/gh/endurodave/cpp-async-delegate/branch/master/graph/badge.svg)](https://app.codecov.io/gh/endurodave/cpp-async-delegate)

# Delegates in C++

The DelegateMQ C++ library can invoke any callable function synchronously, asynchronously, or on a remote endpoint. The library unifies function invocations to simplify multi-threaded and multi-processor development. Well-defined abstract interfaces and numerous concrete examples offer easy porting to any platform. It is a header-only template library that is thread-safe, unit-tested, and easy to use.

```cpp
// Create an async delegate targeting lambda on thread1
auto lambda = [](int i) { std::cout << i; };
auto lambdaDelegate = MakeDelegate(std::function(lambda), thread1);

// Create an async delegate targeting Class::Func() on thread2
Class myClass;
auto memberDelegate = MakeDelegate(&myClass, &Class::Func, thread2);

// Create a thread-safe delegate container
MulticastDelegateSafe<void(int)> delegates;

// Insert delegates into the container 
delegates += lambdaDelegate;
delegates += memberDelegate;

// Invoke all callable targets asynchronously 
delegates(123);
```

# Overview

In C++, a delegate function object encapsulates a callable entity, such as a function, method, or lambda, so it can be invoked later. A delegate is a type-safe wrapper around a callable function that allows it to be passed around, stored, or invoked at a later time, typically within different contexts or on different threads. Delegates are particularly useful for event-driven programming, callbacks, asynchronous APIs, or when you need to pass functions as arguments.

Synchronous and asynchronous delegates are available. Asynchronous variants handle both non-blocking and blocking modes with a timeout. The library supports all types of target functions, including free functions, class member functions, static class functions, lambdas, and `std::function`. It is capable of handling any function signature, regardless of the number of arguments or return value. All argument types are supported, including by value, pointers, pointers to pointers, and references. The delegate library takes care of the intricate details of function invocation across thread boundaries. Concrete porting examples below with easy porting to others.

* **Operating System:** Windows, Linux, [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS)

Remote delegates enable function invocation across processes or processors by using customizable serialization and transport mechanisms. All argument data is marshaled to support remote callable endpoints with any function signature. Minimal effort is required to extend support to any new library. Concrete examples are provided using support libraries below. 

* **Serialization:** [MessagePack](https://msgpack.org/index.html), [RapidJSON](https://github.com/Tencent/rapidjson), [MessageSerialize](https://github.com/endurodave/MessageSerialize)
* **Transport:** [ZeroMQ](https://zeromq.org/), UDP, data pipe, memory buffer
 
It is always safe to call the delegate. In its null state, a call will not perform any action and will return a default-constructed return value. A delegate behaves like a normal pointer type: it can be copied, compared for equality, called, and compared to `nullptr`. Const correctness is maintained; stored const objects can only be called by const member functions.

 A delegate instance can be:

 * Copied freely.
 * Compared to same type delegates and `nullptr`.
 * Reassigned.
 * Called.
 
Typical use cases are:

* Synchronous and Asynchronous Callbacks
* Event-Driven Programming
* Inter-Process and Inter-Processor Communication 
* Inter-Thread Publish/Subscribe (Observer) Pattern
* Thread-Safe Asynchronous API
* Asynchronous Method Invocation (AMI) 
* Design Patterns (Active Object)
* `std::async` Thread Targeting

The DelegateMQ library asynchronous features differ from `std::async` in that the caller-specified thread of control is used to invoke the target function bound to the delegate, rather than a random thread from the thread pool. The asynchronous variants copy the argument data into an event queue, ensuring safe transport to the destination thread, regardless of the argument type. This approach provides 'fire and forget' functionality, allowing the caller to avoid waiting or worrying about out-of-scope stack variables being accessed by the target thread.

DelegateMQ uses an external thread, transport, and serializer, all of which are based on simple, well-defined interfaces.

<img src="docs/LayerDiagram.jpg" alt="DelegateMQ Layer Diagram" style="width:60%;"><br>
*DelegateMQ Layer Diagram*


Originally published on CodeProject at: <a href="https://www.codeproject.com/Articles/5277036/Asynchronous-Multicast-Delegates-in-Modern-Cpluspl">Asynchronous Multicast Delegates in Modern C++</a>

# Documentation

 See [Design Details](docs/DETAILS.md) for implementation design documentation and more examples.

 See [Doxygen Documentation](https://endurodave.github.io/cpp-async-delegate/html/index.html) for source code documentation. 

 See [Unit Test Code Coverage](https://app.codecov.io/gh/endurodave/cpp-async-delegate) test results.

# Project Build

<a href="https://www.cmake.org">CMake</a> is used to create the build files. Visual Studio, GCC, and Clang toolchains are supported. Easy porting to other targets. Execute CMake console commands in the project root directory. 

`cmake -B build -S .`

See [Example Projects](docs/DETAILS.md#example-projects) to build other project examples.

# Features

DelegateMQ features at a glance. 

| Feature | DelegateMQ |
| --- | --- |
| Purpose | DelegateMQ is a library used for invoking any callable synchronously, asynchronously, or remotely |
| Usages | Callbacks (synchronous and asynchronous), asynchronous API's, communication and data distribution, and more |
| Library | Allows customizing data sharing between threads, processes, or processors |
| Complexity | Lightweight and extensible through external library interfaces and full source code |
| Threading | No internal threads. External configurable thread interface portable to any OS (`IThread`). |
| Serialization | External configurable serialization data formats, such as MessagePack, RapidJSON, or custom encoding (`ISerializer`) |
| Transport | External configurable transport, such as ZeroMQ, TCP, UDP, serial, data pipe or any custom transport (`ITransport`)  |
| Message Buffering | Provided by a communication library (e.g. ZeroMQ) or custom solution within transport |
| Timeouts and Retries | Provided by a communication library (e.g. ZeroMQ REQ-REP (Request-Reply)), TCP/IP stack, or custom solution |
| Dynamic Memory | Heap or DelegateMQ fixed-block allocator |
| Error Handling | Configurable for return error code, assert or exception |
| Embedded Friendly | Yes. Any OS such as Windows, Linux and FreeRTOS. An OS is not required (i.e. "superloop"). |
| Operation System | Any. Custom `IThread` implementation may be required. |
| Language | C++17 or higher |

# Related Repositories

## Alternative Implementations

Alternative asynchronous implementations similar in concept to DelegateMQ, simpler, and less features.

* <a href="https://github.com/endurodave/AsyncCallback">Asynchronous Callbacks in C++</a> - A C++ asynchronous callback framework.
* <a href="https://github.com/endurodave/C_AsyncCallback">Asynchronous Callbacks in C</a> - A C language asynchronous callback framework.

## Other Projects Using DelegateMQ

Repositories utilizing the DelegateMQ library.

* <a href="https://github.com/endurodave/AsyncStateMachine">Asynchronous State Machine Design in C++</a> - an asynchronous C++ state machine implemented using an asynchronous delegate library.
* <a href="https://github.com/endurodave/IntegrationTestFramework">Integration Test Framework using Google Test and Delegates</a> - a multi-threaded C++ software integration test framework using Google Test and Delegate libraries.
* <a href="https://github.com/endurodave/Async-SQLite">Asynchronous SQLite API using C++ Delegates</a> - an asynchronous SQLite wrapper implemented using an asynchronous delegate library.







