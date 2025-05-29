![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_ubuntu.yml)
[![conan Clang](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/DelegateMQ/actions/workflows/cmake_windows.yml)
[![Codecov](https://codecov.io/gh/endurodave/DelegateMQ/branch/master/graph/badge.svg)](https://app.codecov.io/gh/endurodave/DelegateMQ)

# Delegates in C++

The DelegateMQ C++ library enables function invocations on any callable, either synchronously, asynchronously, or on a remote endpoint.

# Table of Contents

- [Delegates in C++](#delegates-in-c)
- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Build](#build)
  - [Example Projects](#example-projects)
  - [Examples Setup](#examples-setup)
  - [Build Integration](#build-integration)
- [Quick Start](#quick-start)
  - [Basic Examples](#basic-examples)
  - [Publish/Subscribe Example](#publishsubscribe-example)
    - [Publisher](#publisher)
    - [Subscriber](#subscriber)
  - [Asynchronous API Example](#asynchronous-api-example)
- [Background](#background)
- [Usage](#usage)
  - [Delegates](#delegates)
  - [Delegate Containers](#delegate-containers)
  - [Synchronous Delegates](#synchronous-delegates)
  - [Asynchronous Delegates](#asynchronous-delegates)
    - [Non-Blocking](#non-blocking)
    - [Blocking](#blocking)
    - [Message Priority](#message-priority)
  - [Remote Delegates](#remote-delegates)
  - [Error Handling](#error-handling)
  - [Usage Summary](#usage-summary)
- [Design Details](#design-details)
  - [Library Dependencies](#library-dependencies)
  - [Fixed-Block Memory Allocator](#fixed-block-memory-allocator)
  - [Function Argument Copy](#function-argument-copy)
  - [Caution Using `std::bind`](#caution-using-stdbind)
  - [Caution Using Raw Object Pointers](#caution-using-raw-object-pointers)
  - [Alternatives Considered](#alternatives-considered)
    - [`std::function`](#stdfunction)
    - [`std::async` and `std::future`](#stdasync-and-stdfuture)
    - [`DelegateMQ` vs. `std::async` Feature Comparisons](#delegatemq-vs-stdasync-feature-comparisons)
- [Library](#library)
  - [Heap Template Parameter Pack](#heap-template-parameter-pack)
    - [Argument Heap Copy](#argument-heap-copy)
    - [Bypassing Argument Heap Copy](#bypassing-argument-heap-copy)
    - [Array Argument Heap Copy](#array-argument-heap-copy)
- [Interfaces](#interfaces)
  - [`IThread`](#ithread)
    - [Send `DelegateMsg`](#send-delegatemsg)
    - [Receive `DelegateMsg`](#receive-delegatemsg)
  - [`ISerializer`](#iserializer)
  - [`IDispatcher`](#idispatcher)
- [Examples](#examples)
  - [Callback Example](#callback-example)
  - [Register Callback Example](#register-callback-example)
  - [Asynchronous API Examples](#asynchronous-api-examples)
    - [No Locks](#no-locks)
    - [Reinvoke](#reinvoke)
    - [Blocking Reinvoke](#blocking-reinvoke)
    - [Blocking Remote Send](#blocking-remote-send)
  - [Timer Example](#timer-example)
  - [`std::async` Thread Targeting Example](#stdasync-thread-targeting-example)
  - [Remote Delegate Example](#remote-delegate-example)
  - [More Examples](#more-examples)
  - [Sample Projects](#sample-projects)
    - [External Dependencies](#external-dependencies)
    - [system-architecture](#system-architecture)
    - [bare-metal](#bare-metal)
    - [freertos-bare-metal](#freertos-bare-metal)
    - [mqtt-rapidjson](#mqtt-rapidjson)
    - [nng-bitsery](#nng-bitsery)
    - [win32-pipe-serializer](#win32-pipe-serializer)
    - [win32-upd-serializer](#win32-upd-serializer)
    - [zeromq-cereal](#zeromq-cereal)
    - [zeromq-bitsery](#zeromq-bitsery)
    - [zeromq-serializer](#zeromq-serializer)
    - [zeromq-msgpack-cpp](#zeromq-msgpack-cpp)
    - [zeromq-rapidjson](#zeromq-rapidjson)
- [Tests](#tests)
  - [Unit Tests](#unit-tests)
  - [Valgrind Memory Tests](#valgrind-memory-tests)
    - [Heap Memory Test Results](#heap-memory-test-results)
    - [Fixed-Block Memory Allocator Test Results](#fixed-block-memory-allocator-test-results)
- [Library Comparison](#library-comparison)
- [References](#references)

# Introduction

DelegateMQ is a C++ header-only library for invoking any callable (e.g., function, method, lambda):

- Synchronously
- Asynchronously
- Remotely across processes or processors

It unifies function calls across threads or systems via a simple delegate interface. DelegateMQ is thread-safe, unit-tested, and easy to port to any platform.

1. **Header-Only Library** – implemented using a small number of header files
2. **Any Compiler** – standard C++17 code for any compiler
3. **Any Function** – invoke any callable function: member, static, free, or lambda
4. **Any Argument Type** – supports any argument type: value, reference, pointer, pointer to pointer
5. **Multiple Arguments** – supports N number of function arguments for the bound function
6. **Synchronous Invocation** – call the bound function synchronously
7. **Asynchronous Invocation** – call the bound function asynchronously on a client specified thread
8. **Remote Invocation** - call a function located on a remote endpoint.
9. **Blocking Asynchronous Invocation** - invoke asynchronously using blocking or non-blocking delegates
10. **Smart Pointer Support** - bind an instance function using a raw object pointer or `std::shared_ptr`
11. **Lambda Support** - bind and invoke lambda functions asynchronously using delegates
12. **Automatic Heap Handling** – automatically copy argument data to the heap for safe transport through a message queue
13. **Memory Management** - support for global heap or fixed-block memory allocator
14. **Any OS** – easy porting to any OS. C++11 `std::thread` port included
15. **32/64-bit** - Support for 32 and 64-bit projects
16. **Dynamic Storage Allocation** - optional fixed-block memory allocator
17. **CMake Build** - CMake supports most toolchains including Windows and Linux
18. **Unit Tests** - extensive unit testing of the delegate library included
19. **No External Libraries** – delegate does not rely upon external libraries
20. **Ease of Use** – function signature template arguments (e.g., `DelegateFree<void(TestStruct*)>`)

# Build

To build and run DelegateMQ, follow these simple steps. The library uses <a href="https://www.cmake.org">CMake</a> to generate build files and supports Visual Studio, GCC, and Clang toolchains.

1. Clone the repository.
2. From the repository root, run the following CMake command:   
   `cmake -B build .`
3. Build and run the project within the `build` directory. 

## Example Projects

Remote delegate example projects are located within the `example/sample-projects` directory and explained in [Sample Projects](#sample-projects). 

The [system-architecture](#system-architecture) project located within the `example\sample-projects\system-architecture-no-deps` directory is a client-server application with no external library dependencies. The app runs on Windows and Linux, showcasing various delegate-based design techniques, including remote communication, asynchronous callbacks/APIs, and more.

## Examples Setup

Some remote delegate example projects have external library dependencies. Follow the setup instructions below to build and run any sample project. Not all external libraries are required, as it depended on the example executed.

1. Install [vcpkg](https://github.com/microsoft/vcpkg).
2. Install [Boost](https://www.boost.org/) using vcpkg. DelegateMQ does not use Boost, but some external libraries below require.<br>
   `./vcpkg install boost`<br>
   Linux, may need:<br>
   `./vcpkg install boost:x64-linux-dynamic`
3. Transport libraries
- Install [ZeroMQ](https://zeromq.org/) using vcpkg.<br>
   `./vcvpkg install zeromq`
- Clone and build [NNG](https://github.com/nanomsg/nng) (nanomsg-next-gen) `stable` branch.
- Clone and build [Paho C MQTT](https://github.com/eclipse-paho/paho.mqtt.c)
4. Serialization libraries
- Clone [MessagePack C++](https://github.com/msgpack/msgpack-c/tree/cpp_master) `cpp_master` branch.
- Clone [Cereal](https://github.com/USCiLab/cereal).
- Clone [Bitsery](https://github.com/fraillt/bitsery).
- Clone [RapidJSON](https://github.com/Tencent/rapidjson).
5. Operating systems
  - Clone [FreeRTOS](https://github.com/FreeRTOS/FreeRTOS).
6. Edit `src/delegate-mq/External.cmake` file and update external library directory locations.
7. Build any example subproject within the `example/sample-projects` directory.<br>
   `cmake -B build .`

See [Sample Projects](#sample-projects) for details regarding each sample project.

## Build Integration

Follow these steps to integrate DelegateMQ into a project.

Set the desired DMQ build options and include `DelegateMQ.cmake` within your `CMakeLists.txt` file.

```
# Set build options
set(DMQ_ASSERTS "OFF")
set(DMQ_ALLOCATOR "OFF")
set(DMQ_UTIL "ON")
set(DMQ_THREAD "DMQ_THREAD_STDLIB")
set(DMQ_SERIALIZE "DMQ_SERIALIZE_MSGPACK")
set(DMQ_TRANSPORT "DMQ_TRANSPORT_ZEROMQ")
include("${CMAKE_SOURCE_DIR}/../../src/delegate-mq/DelegateMQ.cmake")
```

Update `External.cmake` external library paths if necessary.

Add `DMQ_PREDEF_SOURCES` to your sources if using the predefined supporting DelegateMQ classes (e.g. `Thread`, `Serialize`, ...).

```
# Collect DelegateMQ predef source files
list(APPEND SOURCES ${DMQ_PREDEF_SOURCES})
```

Add external library include paths defined within `External.cmake` as necessary.

```
include_directories(    
    ${DMQ_INCLUDE_DIR}
    ${MSGPACK_INCLUDE_DIR}
)
```

Include `DelegateMQ.h` to use the delegate library features. Build and execute the project.

# Quick Start

Simple delegate examples showing basic functionality. See [Sample Projects](#sample-projects) for more sample code.

## Basic Examples

Simple function definitions.

```cpp
void FreeFunc(int value) {
    cout << "FreeFuncInt " << value << endl;
}

class TestClass {
public:
    void MemberFunc(int value) {
        cout << "MemberFunc " << value << endl;
    }
};
```

Create delegates and invoke. Function template overloaded `MakeDelegate()` function is typically used to create a delegate instance. 

```cpp
// Create a delegate bound to a free function then invoke synchronously
auto delegateFree = MakeDelegate(&FreeFunc);
delegateFree(123);

// Create a delegate bound to a member function then invoke synchronously
TestClass testClass;
auto delegateMember = MakeDelegate(&testClass, &TestClass::MemberFunc);
delegateMember(123);

// Create a delegate bound to a member function then invoke asynchronously (non-blocking)
auto delegateMemberAsync = MakeDelegate(&testClass, &TestClass::MemberFunc, workerThread);
delegateMemberAsync(123);

// Create a delegate bound to a member function then invoke asynchronously blocking
auto delegateMemberAsyncWait = MakeDelegate(&testClass, &TestClass::MemberFunc, workerThread, WAIT_INFINITE);
delegateMemberAsyncWait(123);
```

Create a delegate container, insert a delegate instance and invoke asynchronously. 

```cpp
// Create a thread-safe multicast delegate container that accepts Delegate<void(int)> delegates
MulticastDelegateSafe<void(int)> delegateSafe;

// Add a delegate to the container that will invoke on workerThread1
delegateSafe += MakeDelegate(&testClass, &TestClass::MemberFunc, workerThread1);

// Asynchronously invoke the delegate target member function TestClass::MemberFunc()
delegateSafe(123);

// Remove the delegate from the container
delegateSafe -= MakeDelegate(&testClass, &TestClass::MemberFunc, workerThread1);
```

Invoke a lambda using a delegate. 

```cpp
DelegateFunction<int(int)> delFunc([](int x) -> int { return x + 5; });
int retVal = delFunc(8);
```
Asynchronously invoke `LambdaFunc1` on `workerThread1` and block waiting for the return value. 

```cpp
std::function LambdaFunc1 = [](int i) -> int {
    cout << "Called LambdaFunc1 " << i << std::endl;
    return ++i;
};

// Asynchronously invoke lambda on workerThread1 and wait for the return value
auto lambdaDelegate1 = MakeDelegate(LambdaFunc1, workerThread1, WAIT_INFINITE);
int lambdaRetVal2 = lambdaDelegate1(123);
```

Asynchronously invoke `AddFunc` on `workerThread1` using `std::async` and do other work while waiting for the return value. 

```cpp
// Long running function 
std::function AddFunc = [](int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return a + b;
};

// Create async delegate with lambda target function
auto addDelegate = MakeDelegate(AddFunc, workerThread1, WAIT_INFINITE);

// Using std::async, invokes AddFunc on workerThread1
std::future<int> result = std::async(std::launch::async, addDelegate, 5, 3);

cout << "Do work while waiting for AddFunc to complete." << endl;

// Wait for AddFunc return value
int sum = result.get();
cout << "AddFunc return value: " << sum << " ";
```

## Publish/Subscribe Example

A simple publish/subscribe example using asynchronous delegates.

### Publisher

Typically a delegate is inserted into a delegate container. <code>AlarmCd</code> is a delegate container. 

<img src="Figure1.jpg" alt="Figure 1: AlarmCb Delegate Container" style="width:50%;">  

**Figure 1: AlarmCb Delegate Container**

1. <code>MulticastDelegateSafe</code> - the delegate container type.
2. <code>void(int, const string&)</code> - the function signature accepted by the delegate container. Any function matching can be inserted, such as a class member, static or lambda function.
3. <code>AlarmCb</code> - the delegate container name. 

Invoke delegate container to notify subscribers.

```cpp
MulticastDelegateSafe<void(int, const string&)> AlarmCb;

void NotifyAlarmSubscribers(int alarmId, const string& note)
{
    // Invoke delegate to generate callback(s) to subscribers
    AlarmCb(alarmId, note);
}
```
### Subscriber

Typically a subscriber registers with a delegate container instance to receive callbacks, either synchronously or asynchronously.

<img src="Figure2.jpg" alt="Figure 2: Insert into AlarmCb Delegate Container" style="width:65%;">

**Figure 2: Insert into AlarmCb Delegate Container**

1. <code>AlarmCb</code> - the publisher delegate container instance.
2. <code>+=</code> - add a function target to the container. 
3. <code>MakeDelegate</code> - creates a delegate instance.
4. <code>&alarmSub</code> - the subscriber object pointer.
5. <code>&AlarmSub::MemberAlarmCb</code> - the subscriber callback member function.
6. <code>workerThread1</code> - the thread the callback will be invoked on. Adding a thread argument changes the callback type from synchronous to asynchronous.

Create a function conforming to the delegate signature. Insert a callable functions into the delegate container.

```cpp
class AlarmSub
{
    void AlarmSub()
    {
        // Register to receive callbacks on workerThread1
        AlarmCb += MakeDelegate(this, &AlarmSub::HandleAlarmCb, workerThread1);
    }

    void ~AlarmSub()
    {
        // Unregister from callbacks
        AlarmCb -= MakeDelegate(this, &AlarmSub::HandleAlarmCb, workerThread1);
    }

    void HandleAlarmCb(int alarmId, const string& note)
    {
        // Handle callback here. Called on workerThread1 context.
    }
}
```
## Asynchronous API Example

`SetSystemModeAsyncAPI()` is an asynchronous function call that re-invokes on `workerThread2` if necessary. 

```cpp
void SysDataNoLock::SetSystemModeAsyncAPI(SystemMode::Type systemMode)
{
    // Is the caller executing on workerThread2?
    if (workerThread2.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and re-invoke the function call on workerThread2
        MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncAPI, workerThread2).AsyncInvoke(systemMode);
        return;
    }

    // Create the callback data
    SystemModeChanged callbackData;
    callbackData.PreviousSystemMode = m_systemMode;
    callbackData.CurrentSystemMode = systemMode;

    // Update the system mode
    m_systemMode = systemMode;

    // Callback all registered subscribers
    SystemModeChangedDelegate(callbackData);
}
```

# Background

If you're not familiar with a delegate, the concept is quite simple. A delegate can be thought of as a super function pointer. In C++, there 's no pointer type capable of pointing to all the possible function variations: instance member, virtual, const, static, free (global), and lambda. A function pointer can't point to instance member functions, and pointers to member functions have all sorts of limitations. However, delegate classes can, in a type-safe way, point to any function provided the function signature matches. In short, a delegate points to any function with a matching signature to support anonymous function invocation.

In practice, while a delegate is useful, a multicast version significantly expands its utility. The ability to bind more than one function pointer and sequentially invoke all registrars' makes for an effective publisher/subscriber mechanism. Publisher code exposes a delegate container and one or more anonymous subscribers register with the publisher for callback notifications.

The problem with callbacks on a multithreaded system, whether it be a delegate-based or function pointer based, is that the callback occurs synchronously. Care must be taken that a callback from another thread of control is not invoked on code that isn't thread-safe. Multithreaded application development is hard. It 's hard for the original designer; it 's hard because engineers of various skill levels must maintain the code; it 's hard because bugs manifest themselves in difficult ways. Ideally, an architectural solution helps to minimize errors and eases application development.

A remote delegate takes the concept further and allows invoking an endpoint function located in a separate process or processor. Extremely configurable using custom serializer and dispatcher interfaces to store callable argument data and send to a remote system.

# Usage

The DelegateMQ library is comprised of delegates and delegate containers. 

## Delegates 

A delegate binds to a single callable function. The delegate classes are:

```cpp
// Delegates
DelegateBase
    Delegate<>
        DelegateFree<>
            DelegateFreeAsync<>
            DelegateFreeAsyncWait<>
            DelegateFreeRemote<>
        DelegateMember<>
            DelegateMemberAsync<>
            DelegateMemberAsyncWait<>
            DelegateMemberRemote<>
        DelegateFunction<>
            DelegateFunctionAsync<>
            DelegateFunctionAsyncWait<>
            DelegateFunctionRemote<>

// Interfaces
IDispatcher
ISerializer
IThread
IThreadInvoker
IRemoteInvoker
``` 

`DelegateFree<>` binds to a free or static member function. `DelegateMember<>` binds to a class instance member function. `DelegateFunction<>` binds to a `std::function` target. All versions offer synchronous function invocation.

`DelegateFreeAsync<>`, `DelegateMemberAsync<>` and `DelegateFunctionAsync<>` operate in the same way as their synchronous counterparts; except these versions offer non-blocking asynchronous function execution on a specified thread of control. `IThread` and `IThreadInvoker` interfaces to send messages integrates with any OS.

`DelegateFreeAsyncWait<>`, `DelegateMemberAsyncWait<>` and `DelegateFunctionAsyncWait<>` provides blocking asynchronous function execution on a target thread with a caller supplied maximum wait timeout. The destination thread will not invoke the target function if the timeout expires.

`DelegateFreeRemote<>`, `DelegateMemberRemote<>` and `DelegateFunctionRemote<>` provides non-blocking remote function execution. `ISerialize` and `IRemoteInvoker` interfaces support integration with any system. 

The template-overloaded `MakeDelegate()` helper function eases delegate creation.

## Delegate Containers

A delegate container stores one or more delegates. A delegate container is callable and invokes all stored delegates sequentially. A unicast delegate container holds at most one delegate.

```cpp
// Delegate Containers
UnicastDelegate<>
    UnicastDelegateSafe<>
MulticastDelegate<>
    MulticastDelegateSafe<>
``` 

`UnicastDelegate<>` is a delegate container accepting a single delegate. 

`MulticastDelegate<>` is a delegate container accepting multiple delegates.

`MultcastDelegateSafe<>` is a thread-safe container accepting multiple delegates. Always use the thread-safe version if multiple threads access the container instance.

## Synchronous Delegates

Delegates can be created with the overloaded `MakeDelegate()` template function. For example, a simple free function.

```cpp
void FreeFuncInt(int value) {
      cout << "FreeCallback " << value << endl;
}
```

Bind a free function to a delegate and invoke.

```cpp
// Create a delegate bound to a free function then invoke
auto delegateFree = MakeDelegate(&FreeFuncInt);
delegateFree(123);
```

Bind a member function with two arguments to `MakeDelegate()`: the class instance and member function pointer. 

```cpp
// Create a delegate bound to a member function then invoke    
auto delegateMember = MakeDelegate(&testClass, &TestClass::MemberFunc);    
delegateMember(&testStruct);
```

Bind a lambda function is easy using `std::function`.

```cpp
std::function<bool(int)> rangeLambda = MakeDelegate(
    +[](int v) { return v > 2 && v <= 6; }
);
bool inRange = rangeLambda(6);
```

A delegate container holds one or more delegates. 

```cpp
MulticastDelegate<void(int)> delegateA;
MulticastDelegate<void(int, int)> delegateD;
MulticastDelegate<void(float, int, char)> delegateE;
MulticastDelegate<void(const MyClass&, MyStruct*, Data**)> delegateF;
```

Creating a delegate instance and adding it to the multicast delegate container.

```cpp
delegateA += MakeDelegate(&FreeFuncInt);
```

An instance member function can also be added to any delegate container.

```cpp
delegateA += MakeDelegate(&testClass, &TestClass::MemberFunc);
```

Invoke callbacks for all registered delegates. If multiple delegates are stored, each one is called sequentially.

```cpp
// Invoke the delegate target functions
delegateA(123);
```

Removing a delegate instance from the delegate container.

```cpp
delegateA -= MakeDelegate(&FreeFuncInt);
```

Alternatively, `Clear()` is used to remove all delegates within the container.

```cpp
delegateA.Clear();
```

A delegate is added to the unicast container `operator=`.

```cpp
UnicastDelegate<int(int)> delegateF;
delegateF = MakeDelegate(&FreeFuncIntRetInt);
```

Removal is with `Clear()` or assign `nullptr`.

```cpp
delegateF.Clear();
delegateF = nullptr;
```
## Asynchronous Delegates

An asynchronous delegate invokes a callable on a user-specified thread of control. Each invoked asynchronous delegate is copied into the destination message queue with an optional message priority.

### Non-Blocking

Create an asynchronous delegate by adding an extra thread argument to `MakeDelegate()`.

```cpp
Thread workerThread1("WorkerThread1");
workerThread.CreateThread();

// Create delegate and invoke FreeFuncInt() on workerThread
// Does not wait for function call to complete
auto delegateFree = MakeDelegate(&FreeFuncInt, workerThread);
delegateFree(123);
```

If the target function has a return value, it is not valid after invoke. The calling function does not wait for the target function to be called.

An asynchronous delegate instance can also be inserted into a delegate container. 

```cpp
MulticastDelegateSafe<void(TestStruct*)> delegateC;
delegateC += MakeDelegate(&testClass, &TestClass::MemberFunc, workerThread1);
delegateC(&testStruct);
```

Another example of an asynchronous delegate being invoked on `workerThread1` with `std::string` and `int` arguments.

```cpp
// Create delegate with std::string and int arguments then asynchronously    
// invoke on a member function
MulticastDelegateSafe<void(const std::string&, int)> delegateH;
delegateH += MakeDelegate(&testClass, &TestClass::MemberFuncStdString, workerThread1);
delegateH("Hello world", 2020);
```

### Blocking

Create an asynchronous blocking delegate by adding an thread and timeout arguments to `MakeDelegate()`.

```cpp
Thread workerThread1("WorkerThread1");
workerThread.CreateThread();

// Create delegate and invoke FreeFuncInt() on workerThread 
// Waits for the function call to complete
auto delegateFree = MakeDelegate(&FreeFuncInt, workerThread, WAIT_INFINITE);
delegateFree(123);
```

A blocking delegate waits until the target thread executes the bound delegate function. The function executes just as you'd expect a synchronous version including incoming/outgoing pointers and references.

Stack arguments passed by pointer/reference do not be thread-safe. The reason is that the calling thread blocks waiting for the destination thread to complete. The delegate implementation guarantees only one thread is able to access stack allocated argument data.

A blocking delegate must specify a timeout in milliseconds or `WAIT_INFINITE`. Unlike a non-blocking asynchronous delegate, which is guaranteed to be invoked, if the timeout expires on a blocking delegate, the function is not invoked. Use `IsSuccess()` to determine if the delegate succeeded or not.

```cpp
std::function LambdaFunc1 = [](int i) -> int
{
    cout << "Called LambdaFunc1 " << i << std::endl;
    return ++i;
};

// Asynchronously invoke lambda on workerThread1 and wait for the return value
auto lambdaDelegate1 = MakeDelegate(LambdaFunc1, workerThread1, WAIT_INFINITE);
int lambdaRetVal2 = lambdaDelegate1(123);
```

Delegates are callable and therefore may be passed to the standard library. The example below shows `CountLambda` executed asynchronously on `workerThread1` by `std::count_if`.

```cpp
std::vector<int> v{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };

auto CountLambda = +[](int v) -> int
{
    return v > 2 && v <= 6;
};
auto countLambdaDelegate = MakeDelegate(CountLambda, workerThread1, WAIT_INFINITE);

const auto valAsyncResult = std::count_if(v.begin(), v.end(),
    countLambdaDelegate);
cout << "Asynchronous lambda result: " << valAsyncResult << endl;
```

The delegate library supports binding with a object pointer and a `std::shared_ptr` smart pointer. Use a `std::shared_ptr` in place of the raw object pointer in the call to `MakeDelegate()`.

```cpp
// Create a shared_ptr, create a delegate, then synchronously invoke delegate function
std::shared_ptr<TestClass> spObject(new TestClass());
auto delegateMemberSp = MakeDelegate(spObject, &TestClass::MemberFuncStdString);
delegateMemberSp("Hello world using shared_ptr", 2020);
```

### Message Priority

Asynchronous delegate priority determines the dispatch order when multiple asynchronous delegates are pending in the message queue.

```cpp
// Async delegate message priority
enum class Priority
{
	LOW,
	NORMAL,
	HIGH
};
```

Use `SetPriority()` to adjust the delegate priority level.

```cpp
auto alarmDel = MakeDelegate(this, &AlarmMgr::RecvAlarmMsg, m_thread);
alarmDel.SetPriority(dmq::Priority::HIGH);  // Alarm messages high priority
NetworkMgr::AlarmMsgCb += alarmDel;
```

## Remote Delegates

A remote delegate asynchronously invokes a remote target function. The sender must implement the `ISerializer` and `IDispatcher` interfaces:

```cpp
template <class R>
struct ISerializer; // Not defined

/// @brief Delegate serializer interface for serializing and deserializing
/// remote delegate arguments. 
template<class RetType, class... Args>
class ISerializer<RetType(Args...)>
{
public:
    /// Inheriting class implements the write function to serialize
    /// data for transport. 
    /// @param[out] os The output stream
    /// @param[in] args The target function arguments 
    /// @return The input stream
    virtual std::ostream& Write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& Read(std::istream& is, Args&... args) = 0;
};

/// @brief Delegate interface class to dispatch serialized function argument data
/// to a remote destination. 
class IDispatcher
{
public:
    /// Dispatch a stream of bytes to a remote system. The implementer is responsible
    /// for sending the bytes over a communication link. 
    /// @param[in] os An outgoing stream to send to the remote destination.
    virtual int Dispatch(std::ostream& os) = 0;
};
```

The sender sends the remote delegate. All function argument data is serialized and sent using the dispatcher.

```cpp
// Dispatcher and serializer instances
Dispatcher dispatcher;
Serializer<void(int)> serializer;

// Send delegate and registers dispatcher and serializer
DelegateFreeRemote<void(int)> delegateRemote(REMOTE_ID);
delegateRemote.SetDispatcher(&dispatcher);
delegateRemote.SetSerializer(&serializer);

// Send invokes the remote function using the serializer and dispatcher interfaces
delegateRemote(123);
```

The receiver receives the serialized function argument data bytes and invokes the target function.

```cpp
// Remote target function
void FreeFuncInt(int i) { std::cout << i; }

// Serializer used to deserialize incoming data
Serializer<void(int)> serializer;

// Receiver delegate and registers serializer
DelegateFreeRemote<void(int)> delegateRemote(FreeFuncInt, REMOTE_ID);
delegateRemote.SetSerializer(&serializer);

// TODO: Receiver code obtains the serializers incoming argument data
// from your application-specific reception code and stored within a stream.
std::istream& recv_stream;

// Receiver invokes the remote target function
delegateRemote.Invoke(recv_stream);
```
## Error Handling

The DelegateMQ library uses dynamic memory to send asynchronous delegate messages to the target thread. By default, out-of-memory failures throw a `std::bad_alloc` exception. Optionally, if `DMQ_ASSERTS` is defined, exceptions are not thrown, and an assert is triggered instead. See `DelegateOpt.h` for more details.

Remote delegate error handling is captured by registering a callback with  `SetErrorHandler()`. A transport monitor (`ITransportMonitor`) is optional and provides message timeout callbacks using message sequence numbers and acknowledgments.

## Usage Summary

Synchronous delegates are created using one argument for free functions and two for instance member functions.

```cpp
auto freeDelegate = MakeDelegate(&MyFreeFunc);
auto memberDelegate = MakeDelegate(&myClass, &MyClass::MyMemberFunc);
```

Adding the thread argument creates a non-blocking asynchronous delegate.

```cpp
auto freeDelegate = MakeDelegate(&MyFreeFunc, myThread);
auto memberDelegate = MakeDelegate(&myClass, &MyClass::MyMemberFunc, myThread);
```

A `std::shared_ptr` can replace a raw instance pointer on synchronous and non-blocking asynchronous member delegates.

```cpp
std::shared_ptr<MyClass> myClass(new MyClass());
auto memberDelegate = MakeDelegate(myClass, &MyClass::MyMemberFunc, myThread);
```

Adding a `timeout` argument creates a blocking asynchronous delegate.

```cpp
auto freeDelegate = MakeDelegate(&MyFreeFunc, myThread, WAIT_INFINITE);
auto memberDelegate = MakeDelegate(&myClass, &MyClass::MyMemberFunc, myThread, std::chrono::milliseconds(5000));
```

Add to a multicast containers using `operator+=` and `operator-=`. 

```cpp
MulticastDelegate<void(int)> multicastContainer;
multicastContainer += MakeDelegate(&MyFreeFunc);
multicastContainer -= MakeDelegate(&MyFreeFunc);
```

Use the thread-safe multicast delegate container when using asynchronous delegates to allow multiple threads to safely add/remove from the container.

```cpp
MulticastDelegateSafe<void(int)> multicastContainer;
multicastContainer += MakeDelegate(&MyFreeFunc, myThread);
multicastContainer -= MakeDelegate(&MyFreeFunc, myThread);
```

Add to a unicast container using `operator=`.

```cpp
UnicastDelegate<void(int)> unicastContainer;
unicastContainer = MakeDelegate(&MyFreeFunc);
unicastContainer = 0;
```

All delegates and delegate containers are invoked using `operator()`.

```cpp
myDelegate(123)
```

Use `IsSuccess()` on blocking delegates before using the return value or outgoing arguments.

```cpp
if (myDelegate) 
{
     int outInt = 0;
     int retVal = myDelegate(&outInt);
     if (myDelegate.IsSuccess()) 
     {
          cout << outInt << retVal;
     }
}
```

# Design Details

## Library Dependencies

The `DelegateMQ` library external dependencies are based upon on the intended use. Interfaces provide the delegate library with platform-specific features to ease porting to a target system. Complete example code offer ready-made solutions or create your own.

| Class | Interface | Usage | Notes
| --- | --- | --- | --- |
| `Delegate` | n/a | Synchronous Delegates | No interfaces; use as-is without external dependencies.
| `DelegateAsync`<br>`DelegateAsyncWait` | `IThread` | Asynchronous Delegates | `IThread` used to send a delegate and argument data through an OS message queue.
| `DelegateRemote` | `ISerializer`<br>`IDispatcher`<br>`IDispatchMonitor` | Remote Delegates | `ISerializer` used to serialize callable argument data.<br>`IDispatcher` used to send serialized argument data to a remote endpoint.<br>`IDispatchMonitor` used to monitor message timeouts (optional). 

## Fixed-Block Memory Allocator

The DelegateMQ library optionally uses a fixed-block memory allocator when `DMQ_ALLOCATOR` is defined. See `DelegateOpt.h` for more details. The allocator design is available in the [stl_allocator](https://github.com/endurodave/stl_allocator) repository.

`std::function` used within class `DelegateFunction` may use the heap under certain conditions. Implement a custom `xfunction` similar to the `xlist` concept within `xlist.h` using the `xallocator` fixed-block allocator if deemed necessary.

## Function Argument Copy

The behavior of the DelegateMQ library when invoking asynchronous non-blocking delegates (e.g. `DelegateAsyncFree<>`) is to copy arguments into heap memory for safe transport to the destination thread. All arguments (if any) are duplicated. If your data is not plain old data (POD) and cannot be bitwise copied, ensure you implement an appropriate copy constructor to handle the copying.

Since argument data is duplicated, an outgoing pointer argument passed to a function invoked using an asynchronous non-blocking delegate is not updated. A copy of the pointed to data is sent to the destination target thread, and the source thread continues without waiting for the target to be invoked.

Synchronous and asynchronous blocking delegates, on the other hand, do not copy the target function's arguments when invoked. Outgoing pointer arguments passed through an asynchronous blocking delegate (e.g., `DelegateAsyncFreeWait<>`) behave exactly as if the native target function were called directly.

## Caution Using `std::bind`

`std::function` compares the function signature, not the underlying callable instance. The example below demonstrates this limitation. 

```cpp
// Example shows std::function target limitations. Not a normal usage case.
// Use MakeDelegate() to create delegates works correctly with delegate 
// containers.
Test t1, t2;
std::function<void(int)> f1 = std::bind(&Test::Func, &t1, std::placeholders::_1);
std::function<void(int)> f2 = std::bind(&Test::Func2, &t2, std::placeholders::_1);
MulticastDelegateSafe<void(int)> safe;
safe += MakeDelegate(f1);
safe += MakeDelegate(f2);
safe -= MakeDelegate(f2);   // Should remove f2, not f1!
```

To avoid this issue, use `MakeDelegate()`. In this case, `MakeDelegate()` binds to `Test::Func` using `DelegateMember<>`, not `DelegateFunction<>`, which prevents the error above.

```cpp
// Corrected example
Test t1, t2;
MulticastDelegateSafe<void(int)> safe;
safe += MakeDelegate(&t1, &Test::Func);
safe += MakeDelegate(&t2, &Test::Func2);
safe -= MakeDelegate(&t2, &Test::Func2);   // Works correctly!
```

## Caution Using Raw Object Pointers

Certain asynchronous delegate usage patterns can cause a callback invocation to occur on a deleted object. The problem is this: an object function is bound to a delegate and invoked asynchronously, but before the invocation occurs on the target thread, the target object is deleted. In other words, it is possible for an object bound to a delegate to be deleted before the target thread message queue has had a chance to invoke the callback. The following code exposes the issue:

```cpp
// Example of a bug where the testClassHeap is deleted before the asynchronous delegate
// is invoked on the workerThread1. In other words, by the time workerThread1 calls
// the bound delegate function the testClassHeap instance is deleted and no longer valid.
TestClass* testClassHeap = new TestClass();
auto delegateMemberAsync = 
       MakeDelegate(testClassHeap, &TestClass::MemberFuncStdString, workerThread1);
delegateMemberAsync("Function async invoked on deleted object. Bug!", 2020);
delegateMemberAsync.Clear();
delete testClassHeap;
```

The example above is contrived, but it does clearly show that nothing prevents an object being deleted while waiting for the asynchronous invocation to occur. In many embedded system architectures, the registrations might occur on singleton objects or objects that have a lifetime that spans the entire execution. In this way, the application's usage pattern prevents callbacks into deleted objects. However, if objects pop into existence, temporarily subscribe to a delegate for callbacks, then get deleted later the possibility of a latent delegate stuck in a message queue could invoke a function on a deleted object.

A smart pointer solves this complex object lifetime issue. A `DelegateMemberAsync` delegate binds using a `std::shared_ptr` instead of a raw object pointer. Now that the delegate has a shared pointer, the danger of the object being prematurely deleted is eliminated. The shared pointer will only delete the object pointed to once all references are no longer in use. In the code snippet below, all references to `testClassSp` are removed by the client code yet the delegate's copy placed into the queue prevents `TestClass` deletion until after the asynchronous delegate callback occurs.

```cpp
// Example of the smart pointer function version of the delegate. The testClassSp instance
// is only deleted after workerThread1 invokes the callback function thus solving the bug.
std::shared_ptr<TestClass> testClassSp(new TestClass());
auto delegateMemberSpAsync = MakeDelegate
     (testClassSp, &TestClass::MemberFuncStdString, workerThread1);
delegateMemberSpAsync("Function async invoked using smart pointer. Bug solved!", 2020);
delegateMemberSpAsync.Clear();
testClassSp.reset();
```

This technique can be used to call an object function, and then the object automatically deletes after the callback occurs. Using the above example, create a shared pointer instance, bind a delegate, and invoke the delegate. Now `testClassSp` can go out of scope and `TestClass::MemberFuncStdString` will still be safely called on `workerThread1`. The `TestClass` instance will delete by way of `std::shared_ptr<TestClass>` once the smart pointer reference count goes to 0 after the callback completes without any extra programmer involvement.

```cpp
std::shared_ptr<TestClass> testClassSp(new TestClass());
auto delegateMemberSpAsync =
    MakeDelegate(testClassSp, &TestClass::MemberFuncStdString, workerThread1);
delegateMemberSpAsync("testClassSp deletes after delegate invokes", 2020);
```

## Alternatives Considered

### `std::function`

`std::function` compares the function signature, not the underlying callable instance. The example below demonstrates this limitation.

```cpp
#include <iostream>
#include <vector>
#include <functional>

class Test {
public:
    void Func(int i) { }
    void Func2(int i) { }
};

void main() {
    Test t1, t2;

    // Create std::function objects for different instances
    std::function<void(int)> f1 = std::bind(&Test::Func, &t1, std::placeholders::_1);
    std::function<void(int)> f2 = std::bind(&Test::Func2, &t2, std::placeholders::_1);

    // Store them in a std::vector
    std::vector<std::function<void(int)>> funcs;
    funcs.push_back(f1);
    funcs.push_back(f2);

    // std::function can't determine difference!
    if (funcs[0].target_type() == funcs[1].target_type())
        std::cout << "Wrong!" << std::endl;

    return 0;
}
```

The delegate library prevents this error. See [Caution Using `std::bind`](#caution-using-stdbind) for more information.

### `std::async` and `std::future`

The DelegateMQ library's asynchronous features differ from `std::async` in that the caller-specified thread of control is used to invoke the target function bound to the delegate. The asynchronous variants copy the argument data into the event queue, ensuring safe transport to the destination thread, regardless of the argument type. This approach provides 'fire and forget' functionality, allowing the caller to avoid waiting or worrying about out-of-scope stack variables being accessed by the target thread.

In short, the DelegateMQ library offers features that are not natively available in the C++ standard library to ease multi-threaded application development.

 ### `DelegateMQ` vs. `std::async` Feature Comparisons

| Feature | `Delegate` | `DelegateAsync` | `DelegateAsyncWait` | `std::async` |
| ---- | ----| ---- | ---- | ---- |
| Callable | Yes | Yes | Yes | Yes |
| Synchronous Call | Yes | No | No | No |
| Asynchronous Call | No | Yes | Yes | Yes |
| Asynchronous Blocking Call | No | No | Yes | Yes |
| Asynchronous Wait Timeout | No | No | Yes | No |
| Specify Target Thread | n/a | Yes | Yes | No |
| Copyable | Yes | Yes | Yes | Yes |
| Equality | Yes | Yes | Yes | Yes |
| Compare with `nullptr` | Yes | Yes | Yes | No |
| Callable Argument Copy | No | Yes<sup>1</sup> | No | No |
| Dangling `Arg&`/`Arg*` Possible | No | No | No | Yes<sup>2</sup> |
| Callable Ambiguity | No | No | No | Yes<sup>3</sup> |
| Thread-Safe Container <sup>4</sup> | Yes | Yes | Yes | No |

<sup>1</sup> `DelegateAsync<>` function call operator copies all function data arguments using a copy constructor for safe transport to the target thread.   
<sup>2</sup> `std::async` could fail if dangling reference or pointer function argument is accessed. Delegates copy argument data when needed to prevent this failure mode.  
<sup>3</sup> `std::function` cannot resolve difference between functions with matching signature `std::function` instances (e.g `void Class:One(int)` and `void Class::Two(int)` are equal).  
<sup>4</sup> `MulticastDelegateSafe<>` a thread-safe container used to hold and invoke a collection of delegates.

# Library

A single include `DelegateMQ.h` provides access to all delegate library features. The library is wrapped within a `dmq` namespace. The table below shows the delegate class hierarchy.

```cpp
// Delegates
DelegateBase
    Delegate<>
        DelegateFree<>
            DelegateFreeAsync<>
            DelegateFreeAsyncWait<>
            DelegateFreeRemote<>
        DelegateMember<>
            DelegateMemberAsync<>
            DelegateMemberAsyncWait<>
            DelegateMemberRemote<>
        DelegateFunction<>
            DelegateFunctionAsync<>
            DelegateFunctionAsyncWait<>
            DelegateFunctionRemote<>

// Delegate Containers
UnicastDelegate<>
    UnicastDelegateSafe<>
MulticastDelegate<>
    MulticastDelegateSafe<>
```

Some degree of code duplication exists within the delegate inheritance hierarchy. This arises because the `Free`, `Member`, and `Function` classes support different target function types, making code sharing via inheritance difficult. Alternative solutions to share code either compromised type safety, caused non-intuitive user syntax, or significantly increased implementation complexity and code readability. Extensive unit tests ensure a reliable implementation.

The Python script `src_dup.py` helps mitigate some of the maintenance overhead. See the script source for details.

## Heap Template Parameter Pack

Non-blocking asynchronous invocations means that all argument data must be copied into the heap for transport to the destination thread. Arguments come in different styles: by value, by reference, pointer and pointer to pointer. For non-blocking delegates, the data is copied to the heap to ensure the data is valid on the destination thread. The key to being able to save each parameter into `DelegateMsgHeapArgs<>` is the `make_tuple_heap()` function. This template metaprogramming function creates a `tuple` of arguments where each tuple element is created on the heap.

```cpp
/// @brief Terminate the template metaprogramming argument loop
template<typename... Ts>
auto make_tuple_heap(xlist<std::shared_ptr<heap_arg_deleter_base>>& heapArgs, std::tuple<Ts...> tup)
{
    return tup;
}

/// @brief Creates a tuple with all tuple elements created on the heap using
/// operator new. Call with an empty list and empty tuple. The empty tuple is concatenated
/// with each heap element. The list contains heap_arg_deleter_base objects for each 
/// argument heap memory block that will be automatically deleted after the bound
/// function is invoked on the target thread. 
template<typename Arg1, typename... Args, typename... Ts>
auto make_tuple_heap(xlist<std::shared_ptr<heap_arg_deleter_base>>& heapArgs, std::tuple<Ts...> tup, Arg1 arg1, Args... args)
{
    static_assert(!(
        (is_shared_ptr<Arg1>::value && (std::is_lvalue_reference_v<Arg1> || std::is_pointer_v<Arg1>))),
        "std::shared_ptr reference argument not allowed");
    static_assert(!std::is_same<Arg1, void*>::value, "void* argument not allowed");

    auto new_tup = tuple_append(heapArgs, tup, arg1);
    return make_tuple_heap(heapArgs, new_tup, args...);
}
```

Template metaprogramming uses the C++ template system to perform compile-time computations within the code. Notice the recursive compiler call of `make_tuple_heap()` as the `Arg1` template parameter gets consumed by the function until no arguments remain and the recursion is terminated. The snippet above shows the concatenation of heap allocated tuple function arguments. This allows for the arguments to be copied into dynamic memory for transport to the target thread through a message queue.</p>

This bit of code inside `make_tuple_heap.h` was tricky to create in that each argument must have memory allocated, data copied, appended to the tuple, then subsequently deallocated all based on its type. To further complicate things, this all has to be done generically with N number of disparate template argument parameters. This was the key to getting a template parameter pack of arguments through a message queue. `DelegateMsgHeapArgs` then stores the tuple parameters for easy usage by the target thread. The target thread uses `std::apply()` to invoke the bound function with the heap allocated tuple argument(s).

The pointer argument `tuple_append()` implementation is shown below. It creates dynamic memory for the argument, argument data copied, adds to a deleter list for subsequent later cleanup after the target function is invoked, and finally returns the appended tuple.

```cpp
/// @brief Append a pointer argument to the tuple
template <typename Arg, typename... TupleElem>
auto tuple_append(xlist<std::shared_ptr<heap_arg_deleter_base>>& heapArgs, const std::tuple<TupleElem...> &tup, Arg* arg)
{
    Arg* heap_arg = nullptr;
    if (arg != nullptr) {
        heap_arg = new(std::nothrow) Arg(*arg);  // Only create a new Arg if arg is not nullptr
        if (!heap_arg) {
            BAD_ALLOC();
        }
    }
    std::shared_ptr<heap_arg_deleter_base> deleter(new(std::nothrow) heap_arg_deleter<Arg*>(heap_arg));
    if (!deleter) {
        BAD_ALLOC();
    }
    try {
        heapArgs.push_back(deleter);
        return std::tuple_cat(tup, std::make_tuple(heap_arg));
    }
    catch (...) {
        BAD_ALLOC();
        throw;
    }
}
```

The pointer argument deleter is implemented below. When the target function invocation is complete, the `heap_arg_deleter` destructor will `delete` the heap argument memory. The heap argument cannot be a changed to a smart pointer because it would change the argument type used in the target function signature. Therefore, the `heap_arg_deleter` is used as a smart pointer wrapper around the (potentially) non-smart heap argument.

```cpp
/// @brief Frees heap memory for pointer heap argument
template<typename T>
class heap_arg_deleter<T*> : public heap_arg_deleter_base
{
public:
    heap_arg_deleter(T* arg) : m_arg(arg) { }
    virtual ~heap_arg_deleter()
    {
        delete m_arg;
    }
private:
    T* m_arg;
};
```

### Argument Heap Copy

Asynchronous non-blocking delegate invocations mean that all argument data must be copied to the heap for transport to the destination thread. All arguments, regardless of their type, will be duplicated, including: value, pointer, pointer to pointer, and reference. If your data is not plain old data (POD) and cannot be bitwise copied, be sure to implement an appropriate copy constructor to handle the copying yourself.

For instance, invoking this function asynchronously the argument `TestStruct` will be copied.

```cpp
void TestFunc(TestStruct* data);
```

### Bypassing Argument Heap Copy

Occasionally, you may not want the delegate library to copy your arguments. Instead, you just want the destination thread to have a pointer to the original copy. A `std::shared_ptr` function argument does not copy the object pointed to when using an asynchronous delegate target function. 

```cpp
std::function lambFunc = [](std::shared_ptr<TestStruct> s) {};
auto del = MakeDelegate(lambFunc, workerThread1);
std::shared_ptr<TestStruct> sp = std::make_shared<TestStruct>();

// Invoke lambFunc and TestStruct instance is not copied
del(sp);   
```

### Array Argument Heap Copy

Array function arguments are adjusted to a pointer per the C standard. In short, any function parameter declared as `T a[]` or `T a[N]` is treated as though it were declared as `T *a`. Since the array size is not known, the library cannot copy the entire array. For instance, the function below:

```cpp
void ArrayFunc(char a[]) {}
```

Requires a delegate argument `char*` because the `char a[]` was "adjusted" to `char *a`.

```cpp
auto delegateArrayFunc = MakeDelegate(&ArrayFunc, workerThread1);
delegateArrayFunc(cArray);
```

There is no way to asynchronously pass a C-style array by value. Avoid C-style arrays if possible when using asynchronous delegates to avoid confusion and mistakes.

# Interfaces

DelegateMQ interface classes allow customizing the library runtime behavior.

## `IThread`

The `IThread` interface is used to send a delegate and argument data through a message queue. A delegate thread is required to dispatch asynchronous delegates to a specified target thread. The delegate library automatically constructs a `DelegateMsg` containing everything necessary for the destination thread.

```cpp
class IThread
{
public:
    /// Destructor
    virtual ~IThread() = default;

    /// Dispatch a DelegateMsg onto this thread. The implementer is responsible
    /// for getting the DelegateMsg into an OS message queue. Once DelegateMsg
    /// is on the correct thread of control, the DelegateInvoker::Invoke() function
    /// must be called to execute the delegate. 
    /// @param[in] msg - a pointer to the delegate message that must be created dynamically.
    /// @pre Caller *must* create the DelegateMsg argument dynamically.
    /// @post The destination thread calls Invoke().
    virtual void DispatchDelegate(std::shared_ptr<DelegateMsg> msg) = 0;
};
```

### Send `DelegateMsg`

An asynchronous delegate library function operator `RetType operator()(Args... args)` calls `DispatchDelegate()` to send a delegate message to the destination target thread.

```cpp
auto thread = this->GetThread();
if (thread) {
    // Dispatch message onto the callback destination thread. Invoke()
    // will be called by the destination thread. 
    thread->DispatchDelegate(msg);
}
```

`DispatchDelegate()` inserts a message into the thread message queue. `Thread` class uses a underlying `std::thread`. `Thread` is an implementation detail; create a unique `DispatchDelegate()` function based on the platform operating system API.

```cpp
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    if (m_thread == nullptr)
        throw std::invalid_argument("Thread pointer is null");

    // Create a new ThreadMsg
    std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_DISPATCH_DELEGATE, msg));

    // Add dispatch delegate msg to queue and notify worker thread
    std::unique_lock<std::mutex> lk(m_mutex);
    m_queue.push(threadMsg);
    m_cv.notify_one();
}
```

### Receive `DelegateMsg`

 Inherit from `IDelegateInvoker` and implement the `Invoke()` function. The implementation typically inserts a `std::shared_ptr<DelegateMsg>` into the thread's message queue for processing.

```cpp
/// @brief Abstract base class to support asynchronous delegate function invoke
/// on destination thread of control. 
/// 
/// @details Inherit form this class and implement `Invoke()`. The implementation
/// typically posts a message into the destination thread message queue. The destination
/// thread receives the message and invokes the target bound function. 
class IDelegateInvoker
{
public:
    /// Called to invoke the bound target function by the destination thread of control.
    /// @param[in] msg - the incoming delegate message.
    /// @return `true` if function was invoked; `false` if failed. 
    virtual bool Invoke(std::shared_ptr<DelegateMsg> msg) = 0;
};
```

The `Thread::Process()` thread loop is shown below. `Invoke()` is called for each incoming `MSG_DISPATCH_DELEGATE` queue message.

```cpp
void Thread::Process()
{
    m_timerExit = false;
    std::thread timerThread(&Thread::TimerThread, this);

    while (1)
    {
        std::shared_ptr<ThreadMsg> msg;
        {
            // Wait for a message to be added to the queue
            std::unique_lock<std::mutex> lk(m_mutex);
            while (m_queue.empty())
                m_cv.wait(lk);

            if (m_queue.empty())
                continue;

            msg = m_queue.front();
            m_queue.pop();
        }

        switch (msg->GetId())
        {
            case MSG_DISPATCH_DELEGATE:
            {
                // Get pointer to DelegateMsg data from queue msg data
                auto delegateMsg = msg->GetData();
                ASSERT_TRUE(delegateMsg);

                auto invoker = delegateMsg->GetDelegateInvoker();
                ASSERT_TRUE(invoker);

                // Invoke the delegate destination target function
                bool success = invoker->Invoke(delegateMsg);
                ASSERT_TRUE(success);
                break;
            }

            case MSG_TIMER:
                Timer::ProcessTimers();
                break;

            case MSG_EXIT_THREAD:
            {
                m_timerExit = true;
                timerThread.join();
                return;
            }

            default:
                throw std::invalid_argument("Invalid message ID");
        }
    }
}
```

## `ISerializer`

The `ISerializer` interface is used to serializes argument data for sending to a remote destination endpoint. Each target function built-in or user defined argument is serialized and deserialized based on the system requirements. Custom serialization such as [MessagePack](https://msgpack.org/index.html). The `examples/sample-projects` folder contains sample projects.

```cpp
template <class R>
struct ISerializer; // Not defined

/// @brief Delegate serializer interface for serializing and deserializing
/// remote delegate arguments. Implemented by application code if remote 
/// delegates are used.
/// 
/// @details All argument data is serialized into a stream. `write()` is called
/// by the sender when the delegate is invoked. `read()` is called by the receiver
/// upon reception of the remote message data bytes. 
template<class RetType, class... Args>
class ISerializer<RetType(Args...)>
{
public:
    /// Inheriting class implements the write function to serialize
    /// data for transport. 
    /// @param[out] os The output stream
    /// @param[in] args The target function arguments 
    /// @return The input stream
    virtual std::ostream& Write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& Read(std::istream& is, Args&... args) = 0;
};
```

## `IDispatcher`

The `IDispatcher` interface is used to dispatch the target function argument data to a remote destination endpoint. Custom dispatchers such as sockets, named pipes, or `ZeroMQ` is supported. The `examples/sample-projects` folder contains sample projects.

```cpp
/// @brief Delegate interface class to dispatch serialized function argument data
/// to a remote destination. Implemented by the application if using remote delegates.
/// 
/// @details Incoming data from the remote must the `IDispatcher::Dispatch()` to 
/// invoke the target function using argument data. The argument data is serialized 
/// for transport using a concrete class implementing the `ISerialzer` interface 
/// allowing any data argument serialization method is supported.
/// @post The receiver calls `IRemoteInvoker::Invoke()` when the dispatched message
/// is received.
class IDispatcher
{
public:
    /// Dispatch a stream of bytes to a remote system. The implementer is responsible
    /// for sending the bytes over a communication transport (UDP, TCP, shared memory, 
    /// serial, ...). 
    /// @param[in] os An outgoing stream to send to the remote destination.
    virtual int Dispatch(std::ostream& os) = 0;
};
```

# Examples

## Callback Example

Here are a few real-world examples that demonstrate common delegate usage patterns. First, `SysData` is a simple class that exposes an outgoing asynchronous interface. It stores system data and provides asynchronous notifications to subscribers when the mode changes. The class interface is shown below:

```cpp
class SysData
{
public:
    /// Clients register with MulticastDelegateSafe1 to get callbacks when system mode changes
    MulticastDelegateSafe<void(const SystemModeChanged&)> SystemModeChangedDelegate;

    /// Get singleton instance of this class
    static SysData& GetInstance();

    /// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
    /// @param[in] systemMode - the new system mode. 
    void SetSystemMode(SystemMode::Type systemMode);    

private:
    SysData();
    ~SysData();

    /// The current system mode data
    SystemMode::Type m_systemMode;

    /// Lock to make the class thread-safe
    LOCK m_lock;
};
```

The subscriber interface for receiving callbacks is `SystemModeChangedDelegate`. Calling `SetSystemMode()` updates `m_systemMode` with the new value and notifies all registered subscribers.

```cpp
void SysData::SetSystemMode(SystemMode::Type systemMode)
{
    LockGuard lockGuard(&m_lock);

    // Create the callback data
    SystemModeChanged callbackData;
    callbackData.PreviousSystemMode = m_systemMode;
    callbackData.CurrentSystemMode = systemMode;

    // Update the system mode
    m_systemMode = systemMode;

    // Callback all registered subscribers
    SystemModeChangedDelegate(callbackData);
}
```

## Register Callback Example

`SysDataClient` is a delegate subscriber and registers for `SysData::SystemModeChangedDelegate` notifications within the constructor.

```cpp
// Constructor
SysDataClient() :
    m_numberOfCallbacks(0)
{
    // Register for async delegate callbacks
    SysData::GetInstance().SystemModeChangedDelegate += 
             MakeDelegate(this, &SysDataClient::CallbackFunction, workerThread1);
    SysDataNoLock::GetInstance().SystemModeChangedDelegate += 
                   MakeDelegate(this, &SysDataClient::CallbackFunction, workerThread1);
}
```

`SysDataClient::CallbackFunction()` is now called on `workerThread1` when the system mode changes.

```cpp
void CallbackFunction(const SystemModeChanged& data)
{
    m_numberOfCallbacks++;
    cout << "CallbackFunction " << data.CurrentSystemMode << endl;
}
```

When `SetSystemMode()` is called, anyone interested in the mode changes are notified synchronously or asynchronously depending on the delegate type registered.

```cpp
// Set new SystemMode values. Each call will invoke callbacks to all
// registered client subscribers.
SysData::GetInstance().SetSystemMode(SystemMode::STARTING);
SysData::GetInstance().SetSystemMode(SystemMode::NORMAL);
```

## Asynchronous API Examples

An async-API look like a normal function call to a caller. However, an async delegate invokes the callable on a specific thread of control either blocking or non-blocking.

### No Locks

`SysDataNoLock` is an alternate implementation that uses a private `MulticastDelegateSafe<>` for setting the system mode asynchronously and without locks.

```cpp
class SysDataNoLock
{
public:
    /// Clients register with MulticastDelegateSafe to get callbacks when system mode changes
    MulticastDelegateSafe<void(const SystemModeChanged&)> SystemModeChangedDelegate;

    /// Get singleton instance of this class
    static SysDataNoLock& GetInstance();

    /// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
    /// @param[in] systemMode - the new system mode. 
    void SetSystemMode(SystemMode::Type systemMode);    

    /// Sets the system mode and notify registered clients via a temporary stack created
    /// asynchronous delegate. 
    /// @param[in] systemMode - The new system mode. 
    void SetSystemModeAsyncAPI(SystemMode::Type systemMode);    

    /// Sets the system mode and notify registered clients via a temporary stack created
    /// asynchronous delegate. This version blocks (waits) until the delegate callback
    /// is invoked and returns the previous system mode value. 
    /// @param[in] systemMode - The new system mode. 
    /// @return The previous system mode. 
    SystemMode::Type SetSystemModeAsyncWaitAPI(SystemMode::Type systemMode);

private:
    SysDataNoLock();
    ~SysDataNoLock();

    /// Private callback to get the SetSystemMode call onto a common thread
    MulticastDelegateSafe<void(SystemMode::Type)> SetSystemModeDelegate; 

    /// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
    /// @param[in] systemMode - the new system mode. 
    void SetSystemModePrivate(SystemMode::Type);    

    /// The current system mode data
    SystemMode::Type m_systemMode;
};
```

The constructor registers `SetSystemModePrivate()` with the private `SetSystemModeDelegate`.

```cpp
SysDataNoLock::SysDataNoLock() :
    m_systemMode(SystemMode::STARTING)
{
    SetSystemModeDelegate += MakeDelegate
                 (this, &SysDataNoLock::SetSystemModePrivate, workerThread2);
    workerThread2.CreateThread();
}
```

The `SetSystemMode()` function below is an example of an asynchronous incoming interface. To the caller, it looks like a normal function, but under the hood, a private member call is invoked asynchronously using a delegate. In this case, invoking `SetSystemModeDelegate` causes `SetSystemModePrivate()` to be called on `workerThread2`.

```cpp
void SysDataNoLock::SetSystemMode(SystemMode::Type systemMode)
{
    // Invoke the private callback. SetSystemModePrivate() will be called on workerThread2.
    SetSystemModeDelegate(systemMode);
}
```

Since this private function is always invoked asynchronously on `workerThread2`, it doesn't require locks.

```cpp
void SysDataNoLock::SetSystemModePrivate(SystemMode::Type systemMode)
{
      // Create the callback data
      SystemModeChanged callbackData;

      callbackData.PreviousSystemMode = m_systemMode;
      callbackData.CurrentSystemMode = systemMode;

      // Update the system mode
      m_systemMode = systemMode;

      // Callback all registered subscribers
      SystemModeChangedDelegate(callbackData);
}
```

### Reinvoke

While creating a separate private function for an asynchronous API works, delegates allow you to simply reinvoke the same function on a different thread. A quick check determines if the caller is executing on the desired thread. If not, a temporary asynchronous delegate is created on the stack and invoked. The delegate and all original function arguments are duplicated on the heap, and the function is then reinvoked on `workerThread2`.

```cpp
void SysDataNoLock::SetSystemModeAsyncAPI(SystemMode::Type systemMode)
{
    // Is the caller executing on workerThread2?
    if (workerThread2.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and re-invoke the function call on workerThread2
        auto delegate = 
             MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncAPI, workerThread2);
        delegate(systemMode);
        return;
    }

    // Create the callback data
    SystemModeChanged callbackData;
    callbackData.PreviousSystemMode = m_systemMode;
    callbackData.CurrentSystemMode = systemMode;

    // Update the system mode
    m_systemMode = systemMode;

    // Callback all registered subscribers
    SystemModeChangedDelegate(callbackData);
}
```

### Blocking Reinvoke

A blocking asynchronous API can be encapsulated within a class member function. The following function sets the current mode on `workerThread2` and returns the previous mode. If the caller is not executing on `workerThread2`, a blocking delegate is created and invoked on that thread. To the caller, the function appears synchronous, but the delegate ensures the function is executed on the correct thread before returning.

```cpp
SystemMode::Type SysDataNoLock::SetSystemModeAsyncWaitAPI(SystemMode::Type systemMode)
{
    // Is the caller executing on workerThread2?
    if (workerThread2.GetThreadId() != Thread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and re-invoke the function call on workerThread2
        auto delegate = MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncWaitAPI, workerThread2, WAIT_INFINITE);
        return delegate(systemMode);
    }

    // Create the callback data
    SystemModeChanged callbackData;
    callbackData.PreviousSystemMode = m_systemMode;
    callbackData.CurrentSystemMode = systemMode;

    // Update the system mode
    m_systemMode = systemMode;

    // Callback all registered subscribers
    SystemModeChangedDelegate(callbackData);

    return callbackData.PreviousSystemMode;
}
```

### Blocking Remote Send

`SendActuatorMsgWait()` is an asynchronous API using delegates to send a `ActuatorMsg` object to a remote receiver. The caller is blocked until the receiver receives the message or timeout. See [system-architecture](#system-architecture) project for complete example.

```cpp
// Async send an actuator command and block the caller waiting until the remote 
// client successfully receives the message or timeout. The execution order sequence 
// numbers shown below.
bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg)
{
    // If caller is not executing on m_thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        // *** Caller's thread executes this control branch ***

        std::atomic<bool> success(false);
        std::mutex mtx;
        std::condition_variable cv;

        // 7. Callback lambda handler for transport monitor when remote receives message (success or failure).
        //    Callback context is m_thread.
        SendStatusCallback statusCb = [&success, &cv](dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status) {
            if (id == ids::ACTUATOR_MSG_ID) {
                // Client received ActuatorMsg?
                if (status == TransportMonitor::Status::SUCCESS)
                    success.store(true);
                cv.notify_one();
            }
        };

        // 1. Register for send status callback (success or failure)
        m_transportMonitor.SendStatusCb += dmq::MakeDelegate(statusCb);

        // 2. Async reinvoke SendActuatorMsgWait() on m_thread context and wait for send to complete
        auto del = MakeDelegate(this, &NetworkMgr::SendActuatorMsgWait, m_thread, SEND_TIMEOUT);
        auto retVal = del.AsyncInvoke(msg);

        // 5. Check that the remote delegate send succeeded
        if (retVal.has_value() &&      // If async function call succeeded AND
            retVal.value() == true)    // async function call returned true
        {
            // 6. Wait for statusCb callback to be invoked. Callback invoked when the 
            //    receiver ack's the message or timeout.
            std::unique_lock<std::mutex> lock(mtx);
            if (cv.wait_for(lock, RECV_TIMEOUT) == std::cv_status::timeout) 
            {
                // Timeout waiting for remote delegate message ack
                std::cout << "Timeout SendActuatorMsgWait()" << std::endl;
            }
        }

        // 8. Unregister from status callback
        m_transportMonitor.SendStatusCb -= dmq::MakeDelegate(statusCb);

        // 9. Return the blocking async function invoke status to caller
        return success.load();
    }
    else
    {
        // *** NetworkMgr::m_thread executes this control branch ***

        // 3. Send actuator command to remote on m_thread
        m_actuatorMsgDel(msg);        

        // 4. Check if send succeeded
        if (m_actuatorMsgDel.GetError() == DelegateError::SUCCESS)
        {
            return true;
        }

        std::cout << "Send failed!" << std::endl;
        return false;
    }
}
```

## Timer Example

Creating a timer callback service is trivial. A `UnicastDelegate<void(void)>` used inside a `Timer` class solves this nicely.

```cpp
/// @brief A timer class provides periodic timer callbacks on the client's 
/// thread of control. Timer is thread safe.
class Timer
{
public:
    /// Client's register with Expired to get timer callbacks
    UnicastDelegate<void(void)> Expired;

    /// Starts a timer for callbacks on the specified timeout interval.
    /// @param[in] timeout - the timeout in milliseconds.
    void Start(std::chrono::milliseconds timeout);

    /// Stops a timer.
    void Stop();
    
    ///...
};
```

Users create an instance of the timer and register for the expiration. In this case, `MyClass::MyCallback()` is called in 1000ms.

```cpp
m_timer.Expired = MakeDelegate(&myClass, &MyClass::MyCallback, myThread);
m_timer.Start(std::chrono::milliseconds(1000));
```
## `std::async` Thread Targeting Example

An example combining `std::async`/`std::future` and an asynchronous delegate to target a specific worker thread during communication transmission.

```cpp
Thread commThread("CommunicationThread");

// Assume SendMsg() is not thread-safe and may only be called on commThread context.
// A random std::async thread from the pool is unacceptable and causes cross-threading.
size_t SendMsg(const std::string& data)
{
    // Simulate sending data takes a long time
    std::this_thread::sleep_for(std::chrono::seconds(2));  

    // Return the "bytes sent" result
    return data.size();  
}

// Send a message asynchronously. Function does other work while SendMsg() 
// is executing on commThread.
size_t SendMsgAsync(const std::string& msg)
{
    // Create an async blocking delegate targeted at SendMsg()
    auto sendMsgDelegate = MakeDelegate(&SendMsg, commThread, WAIT_INFINITE);

    // Start the asynchronous task using std::async. SendMsg() will be called on 
    // commThread context.
    std::future<size_t> result = std::async(std::launch::async, sendMsgDelegate, msg);

    // Do other work while SendMsg() is executing on commThread
    std::cout << "Doing other work in main thread while SendMsg() completes...\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Get bytes sent. Blocks until SendMsg() completes.
    size_t bytesSent = result.get();
    return bytesSent;
}

int main(void)
{
    // Send message asynchronously
    commThread.CreateThread();
    size_t bytesSent = SendMsgAsync("Hello world!");
    return 0;
}
```
## Remote Delegate Example

Use a remote delegate to invoke a remote target function using [MessagePack](https://github.com/msgpack/msgpack-c/tree/cpp_master) (serialization) and [ZeroMQ](https://zeromq.org/) (transport). Code snippets below express the concept. See [system-architecture](#system-architecture) to execute this example.

`NetworkMgr` class used by client and server applications handles communication with remote device. `SendAlarmMsg()` sends a message to the remote. `AlarmMsgCb` callback is used to receive the incoming message.

```cpp
#include "DelegateMQ.h"

// NetworkMgr sends and receives data using a DelegateMQ transport implemented
// with ZeroMQ and MessagePack libraries.
class NetworkMgr
{
public:
    static const dmq::DelegateRemoteId ALARM_MSG_ID = 1;

    using AlarmFunc = void(AlarmMsg&, AlarmNote&);
    using AlarmDel = dmq::MulticastDelegateSafe<AlarmFunc>;

    // Receive callbacks for incoming messages by registering with this container
    static AlarmDel AlarmMsgCb;

    int Create();

    // Send alarm message to the remote
    void SendAlarmMsg(AlarmMsg& msg, AlarmNote& note);

private:
    // Serialize function argument data into a stream
    xostringstream m_argStream;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    ZeroMqTransport m_transport;

    // Dispatcher using DelegateMQ library
    Dispatcher m_dispatcher;

    // Alarm message remote delegate
    Serializer<AlarmFunc> m_alarmMsgSer;
    dmq::DelegateMemberRemote<AlarmDel, AlarmFunc> m_alarmMsgDel;
};
```

At runtime, setup the remote delegate interface. A key point is the remote delegate (`m_alarmMsgDel`) binds directly to the `AlarmMsgCb` container (`dmq::MulticastDelegateSafe<AlarmFunc>`). Every incoming message generates one or more callbacks to registered listeners by binding `m_alarmMsgDel` remote delegate to the `AlarmMsgCb` container. A delegate may bind to any callable, and a delegate container is callable.

```cpp
int NetworkMgr::Create()
{
    // Reinvoke call onto NetworkMgr thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Create, m_thread, WAIT_INFINITE)();

    // Setup the remote delegate interfaces
    m_alarmMsgDel.SetStream(&m_argStream);
    m_alarmMsgDel.SetSerializer(&m_alarmMsgSer);
    m_alarmMsgDel.SetDispatcher(&m_dispatcher);
    m_alarmMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));

    // Bind the remote delegate to the AlarmMsgCb delegate container
    m_alarmMsgDel.Bind(&AlarmMsgCb, &AlarmDel::operator(), ALARM_MSG_ID);

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_transport);

    return 1;
}
```

Send message to the remote by calling the `m_alarmMsgDel` delegate on `m_thread` context. A ZeroMQ socket instance is not thread safe so ZeroMQ socket access is always on the `NetworkMgr` internal thread.

```cpp
void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    // Reinvoke call onto NetworkMgr thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);

    // Send alarm to remote. Invoke remote delegate on m_thread only.
    m_alarmMsgDel(msg, note);
}
```

Client registers to receive remote delegate callbacks on the `AlarmMgr` private thread. 

```cpp
class AlarmMgr
{
    AlarmMgr()
    {
        // Register for alarm callback on m_thread
        NetworkMgr::AlarmMsgCb += MakeDelegate(this, &AlarmMgr::RecvAlarmMsg, m_thread);
    }

    void RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note)
    {
        // Handle incoming alarm message from remote
    }
};
```

## More Examples

See the `examples/sample-code` directory for additional examples.

## Sample Projects

Each project focuses on a transport and serialization pair, but you can freely mix and match any transport with any serializer. See the `examples/sample-projects` directory for example projects.

### External Dependencies

The following remote delegate sample projects have no external library dependencies:

* [bare-metal](../example/sample-projects/bare-metal/) - simple remote delegate app on Windows and Linux.
* [system-architecture-no-deps](../example/sample-projects/system-architecture-no-deps/) - complex remote delegate client/server apps using UDP on Windows or Linux.

All other projects require external 3rd party library support. See [Examples Setup](#examples-setup) for external library installation setup.

### system-architecture

The System Architecture example demonstrates a complex client-server DelegateMQ application. This example implements the acquisition of sensor and actuator data across two applications. It showcases communication and collaboration between subsystems, threads, and processes or processors. Delegate communication, callbacks, asynchronous APIs, and error handing are also highlighted. Notice how easily DelegateMQ transfers event data between threads and processes with minimal application code. The application layer is completely isolated from message passing details.

`NetworkMgr` has three types of remote delegate API examples:

1. **Non-Blocking** - message is sent without waiting. 
2. **Blocking** - message is send and blocks waiting for the remote to acknowledge or timeout.
3. **Future** - message is send without waiting and a `std::future` is used to capture the return value later.

Two System Architecture build projects exist:

* [system-architecture](../example/sample-projects/system-architecture/) - builds on Windows and Linux. Requires MessagePack and ZeroMQ external libraries. See [Examples Setup](#examples-setup).
* [system-architecture-no-deps](../example/sample-projects/system-architecture-no-deps/) - builds on Windows or Linux. No external libraries required.

Follow the steps below to execute the projects.

1. Setup ZeroMQ and MessagePack external library dependencies, if necessary ([Examples Setup](#examples-setup))
2. Execute CMake command in `client` and `server` directories.  
   `cmake -B build .`
3. Build client and server applications within the `build` directory.
4. Start `delegate_server_app` first.
5. Start `delegate_client_app` second.
6. Client and server communicate and output debug data to the console.

| Class | Location | Details |
| --- | --- | --- |
| `ClientApp` | Client | Main client application. Handles local and remote sensors and actuators. |
| `ServerApp` | Server | Main server application. Slave sensors and actuators application. |
| `NetworkMgr` | Client<br>Server | Centralized, shared client and server communication interface using remote delegates. |
| `DataMgr` | Client | Collects data from client (local) and server (remote) data sources |
| `AlarmMgr` | Client<br>Server | Handles system alarms. Server sends alarms to client for processing. |
| `Sensor` | Client<br>Server | Reads sensor data |
| `Actuator` | Client<br>Server | Controls actuator |
| `RemoteEndpoint` | Client<br>Server | Delegate endpoint for bidirectional remote communication. |

Interface implementation details. 

| Interface | Implementation | Details |
| --- | --- | --- |
| `IThread` | `Thread` | Implemented using `std::thread` | 
| `ISerializer` | `Serializer` | Implemented using MessagePack library | 
| `IDispatcher` | `Dispatcher` | Universal delegate dispatcher |
| `ITransport` | `ZeroMqTransport` | ZeroMQ message transport |
| `ITransportMonitor` | `TransportMonitor` | Monitor message timeouts |

### bare-metal

Remote delegate example with no external libraries. 

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | Insertion `operator<<` and extraction `operator>>` operators. 
| `IDispatcher` | Shared sender/receiver `std::stringstream` for dispatcher transport.

### freertos-bare-metal

Remote delegate example using FreeRTOS and no external libraries. Tested using the FreeRTOS Windows port and Visual Studio. Must build as 32-bit application. See CMakeList.txt for details.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using FreeRTOS. 
| `ISerializer` | Insertion `operator<<` and extraction `operator>>` operators. 
| `IDispatcher` | Shared sender/receiver `std::stringstream` for dispatcher transport.

### mqtt-rapidjson

Remote delegate example with MQTT and RapidJSON. Execute the pub and sub projects to run the example. See CMakeList.txt for details.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | RapidJSON library. 
| `IDispatcher` | Shared sender/receiver MQTT library for dispatcher transport.

### nng-bitsery

Remote delegate example using NNG and Bitsery libraries.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | Bitsery serialization library.  
| `IDispatcher` | NNG library for dispatcher transport.

### win32-pipe-serializer

Remote delegate example with Windows pipe and `serialize`. 

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | `serialize` class.
| `IDispatcher` | Windows data pipe for dispatcher transport.

### win32-upd-serializer

Remote delegate example with Windows UDP socket and `serialize`. 

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | `serialize` class.
| `IDispatcher` | Windows UDP socket for dispatcher transport.

### zeromq-cereal

Remote delegate example using ZeroMQ and Cereal libraries.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | Cereal library.  
| `IDispatcher` | ZeroMQ library for dispatcher transport.

### zeromq-bitsery

Remote delegate example using ZeroMQ and Bitsery libraries.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | Bitsery library.  
| `IDispatcher` | ZeroMQ library for dispatcher transport.

### zeromq-serializer

Remote delegate example using ZeroMQ and `serialize`.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | `serialize` class.  
| `IDispatcher` | ZeroMQ library for dispatcher transport.

### zeromq-msgpack-cpp

Remote delegate example using ZeroMQ and MessagePack libraries.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | MessagePack library.  
| `IDispatcher` | ZeroMQ library for dispatcher transport.

### zeromq-rapidjson

Remote delegate example using ZeroMQ and RapidJSON libraries.

| Interface | Implementation |
| --- | --- |
| `IThread` | `Thread` class implemented using `std::thread`. 
| `ISerializer` | RapidJSON library.  
| `IDispatcher` | ZeroMQ library for dispatcher transport.

# Tests

The current master branch build passes all unit tests and Valgrind memory tests.

## Unit Tests

Build and execute runs all the unit tests contained within the `tests\UnitTests` directory.

## Valgrind Memory Tests

[Valgrind](https://valgrind.org/) dynamic storage allocation tests were performed using the heap and fixed-block allocator builds. Valgrind is a programming tool for detecting memory leaks, memory errors, and profiling performance in applications, primarily for Linux-based systems. All tests run on Linux.

### Heap Memory Test Results

The DelegateMQ library Valgrind test results using the heap.

```
==1779805== HEAP SUMMARY:
==1779805==     in use at exit: 0 bytes in 0 blocks
==1779805==   total heap usage: 923,465 allocs, 923,465 frees, 50,733,258 bytes allocated
==1779805== 
==1779805== All heap blocks were freed -- no leaks are possible
==1779805== 
==1779805== For lists of detected and suppressed errors, rerun with: -s
==1779805== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

### Fixed-Block Memory Allocator Test Results

Test results with the fixed-block allocator. Notice the fixed-block runtime uses 22MB verses 50MB for the heap build. Heap storage *recycling* mode was used by the fixed-block allocator. See [stl_allocator](https://github.com/endurodave/stl_allocator) and [xallocator](https://github.com/endurodave/xallocator) for information about the memory allocators.

```
==1780037== HEAP SUMMARY:
==1780037==     in use at exit: 0 bytes in 0 blocks
==1780037==   total heap usage: 644,606 allocs, 644,606 frees, 22,091,451 bytes allocated
==1780037== 
==1780037== All heap blocks were freed -- no leaks are possible
==1780037== 
==1780037== For lists of detected and suppressed errors, rerun with: -s
==1780037== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

# Library Comparison

The table below summarizes the various asynchronous function invocation implementations available in C and C++.

| Repository                                                                                            | Language | Key Delegate Features                                                                                                                                                                                                               | Notes                                                                                                                                                                                                      |
|-------------------------------------------------------------------------------------------------------|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| <a href="https://github.com/endurodave/DelegateMQ">DelegateMQ</a> | C++17    | * Function-like template syntax<br> * Any delegate target function type (member, static, free, lambda)<br>  * N target function arguments<br> * N delegate subscribers<br>  * Optional fixed-block allocator     | * Modern C++ implementation<br> * Extensive unit tests<br> * Metaprogramming and variadic templates used for library implementation<br> * Any C++17 and higher compiler |
<a href="https://github.com/endurodave/AsyncCallback">AsyncCallback</a>                               | C++      | * Traditional template syntax<br> * Callback target function type (static, free)<br> * 1 target function argument<br> * N callback subscribers                                                                                      | * Low lines of source code<br> * Compact C++ implementation<br> * Any C++ compiler                                                                                                                    |
| <a href="https://github.com/endurodave/C_AsyncCallback">C_AsyncCallback</a>                           | C        | * Macros provide type-safety<br> * Callback target function type (static, free)<br> * 1 target function argument<br> * Fixed callback subscribers (set at compile time)<br> * Optional fixed-block allocator                        | * Low lines of source code<br> * Compact C implementation<br> * Any C compiler                                            

# References

Repositories utilizing the DelegateMQ library within different multithreaded applications.

* <a href="https://github.com/endurodave/StateMachineWithModernDelegates">C++ State Machine with Asynchronous Delegates</a> - a framework combining a C++ state machine with the asynchronous delegate library.
* <a href="https://github.com/endurodave/AsyncStateMachine">Asynchronous State Machine Design in C++</a> - an asynchronous C++ state machine implemented using an asynchronous delegate library.
* <a href="https://github.com/endurodave/IntegrationTestFramework">Integration Test Framework using Google Test and Delegates</a> - a multi-threaded C++ software integration test framework using Google Test and Delegate libraries.
* <a href="https://github.com/endurodave/Async-SQLite">Asynchronous SQLite API using C++ Delegates</a> - an asynchronous SQLite wrapper implemented using an asynchronous delegate library.

