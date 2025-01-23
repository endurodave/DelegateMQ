![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml)

# Asynchronous Delegates in C++

A C++ delegate library capable of anonymously invoking any callable function either synchronously or asynchronously on a user-specified thread of control. 

# Table of Contents

- [Asynchronous Delegates in C++](#asynchronous-delegates-in-c)
- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
- [Delegates Background](#delegates-background)
  - [`std::function`](#stdfunction)
  - [`std::async` and `std::future`](#stdasync-and-stdfuture)
  - [`Delegate` vs. `std::async` Feature Comparisons](#delegate-vs-stdasync-feature-comparisons)
- [Using the Code](#using-the-code)
  - [Delegates](#delegates)
  - [Delegate Containers](#delegate-containers)
  - [Synchronous Delegates](#synchronous-delegates)
  - [Asynchronous Non-Blocking Delegates](#asynchronous-non-blocking-delegates)
  - [Asynchronous Blocking Delegates](#asynchronous-blocking-delegates)
  - [Remote Delegates](#remote-delegates)
  - [Fixed-Block Memory Allocator](#fixed-block-memory-allocator)
  - [Error Handling](#error-handling)
  - [Function Argument Copy](#function-argument-copy)
  - [Caution Using `std::bind`](#caution-using-stdbind)
  - [Caution Using Raw Object Pointers](#caution-using-raw-object-pointers)
  - [Usage Summary](#usage-summary)
- [Delegate Library](#delegate-library)
  - [Heap Template Parameter Pack](#heap-template-parameter-pack)
    - [Argument Heap Copy](#argument-heap-copy)
    - [Bypassing Argument Heap Copy](#bypassing-argument-heap-copy)
    - [Array Argument Heap Copy](#array-argument-heap-copy)
- [Delegate Library Interfaces](#delegate-library-interfaces)
  - [`IThread`](#ithread)
    - [Send `DelegateMsg`](#send-delegatemsg)
    - [Receive `DelegateMsg`](#receive-delegatemsg)
  - [`ISerializer`](#iserializer)
  - [`IDispatcher`](#idispatcher)
- [Examples](#examples)
  - [Callback Example](#callback-example)
  - [Register Callback Example](#register-callback-example)
  - [Asynchronous API No Locks Example](#asynchronous-api-no-locks-example)
  - [Asynchronous API Reinvoke Example](#asynchronous-api-reinvoke-example)
  - [Asynchronous API Blocking Reinvoke Example](#asynchronous-api-blocking-reinvoke-example)
  - [Timer Example](#timer-example)
  - [`std::async` Thread Targeting Example](#stdasync-thread-targeting-example)
  - [More Examples](#more-examples)
- [Testing](#testing)
  - [Unit Tests](#unit-tests)
  - [Valgrind Memory Tests](#valgrind-memory-tests)
    - [Heap Memory Test Results](#heap-memory-test-results)
    - [Fixed-Block Memory Allocator Test Results](#fixed-block-memory-allocator-test-results)
- [Library Comparison](#library-comparison)
- [References](#references)


# Introduction

In other languages, delegates are first-class features, allowing developers to use these well-understood constructs. However, in C++, a delegate is not natively available. This doesn't stop us, as programmers, from attempting to emulate the ease with which a delegate can store and invoke any callable function.

Delegates typically support synchronous execution, meaning the bound function is executed within the caller's thread of control when invoked. In multi-threaded applications, it is ideal to specify both the target function and the thread on which it should execute, without imposing restrictions on the function signature. The library handles the complexities of transferring the delegate and all argument data to the destination thread. The goal is to introduce a C++ delegate library with a consistent API that supports both synchronous and asynchronous invocations for any callable function.

The features of the modern C++ delegate library are:

1. **Header-Only Library** – implemented using a small number of header files
2. **Any Compiler** – standard C++17 code for any compiler
3. **Any Function** – invoke any callable function: member, static, free, or lambda
4. **Any Argument Type** – supports any argument type: value, reference, pointer, pointer to pointer
5. **Multiple Arguments** – supports N number of function arguments for the bound function
6. **Synchronous Invocation** – call the bound function synchronously
7. **Asynchronous Invocation** – call the bound function asynchronously on a client specified thread
8. **Blocking Asynchronous Invocation** - invoke asynchronously using blocking or non-blocking delegates
9. **Smart Pointer Support** - bind an instance function using a raw object pointer or `std::shared_ptr`
10. **Lambda Support** - bind and invoke lambda functions asynchronously using delegates
11. **Automatic Heap Handling** – automatically copy argument data to the heap for safe transport through a message queue
12. **Memory Management** - support for global heap or fixed-block memory allocator
13. **Any OS** – easy porting to any OS. C++11 `std::thread` port included
14. **32/64-bit** - Support for 32 and 64-bit projects
15. **Dynamic Storage Allocation** - optional fixed-block memory allocator
16. **CMake Build** - CMake supports most toolchains including Windows and Linux
17. **Unit Tests** - extensive unit testing of the delegate library included
18. **No External Libraries** – delegate does not rely upon external libraries
19. **Ease of Use** – function signature template arguments (e.g., `DelegateFree<void(TestStruct*)>`)

The delegate implementation greatly simplifies multithreaded application development by executing the delegate function with all of its arguments on the specified thread of control. The framework manages all the low-level operations needed to safely invoke any function signature on the target thread. CMake build files allow experimentation on Windows, Linux, and other platforms.

# Delegates Background

If you're not familiar with a delegate, the concept is quite simple. A delegate can be thought of as a super function pointer. In C++, there 's no pointer type capable of pointing to all the possible function variations: instance member, virtual, const, static, free (global), and lambda. A function pointer can't point to instance member functions, and pointers to member functions have all sorts of limitations. However, delegate classes can, in a type-safe way, point to any function provided the function signature matches. In short, a delegate points to any function with a matching signature to support anonymous function invocation.

In practice, while a delegate is useful, a multicast version significantly expands its utility. The ability to bind more than one function pointer and sequentially invoke all registrars' makes for an effective publisher/subscriber mechanism. Publisher code exposes a delegate container and one or more anonymous subscribers register with the publisher for callback notifications.

The problem with callbacks on a multithreaded system, whether it be a delegate-based or function pointer based, is that the callback occurs synchronously. Care must be taken that a callback from another thread of control is not invoked on code that isn't thread-safe. Multithreaded application development is hard. It 's hard for the original designer; it 's hard because engineers of various skill levels must maintain the code; it 's hard because bugs manifest themselves in difficult ways. Ideally, an architectural solution helps to minimize errors and eases application development.

A remote delegate takes the concept further and allows invoking an endpoint function located in a separate process or processor. Extremely configurable using custom serializer and dispatcher interfaces to store callable argument data and send to a remote system.

This C++ delegate implementation is full featured and allows calling any function, even instance member functions, with any arguments either synchronously or asynchronously. The delegate library makes binding to and invoking any function a snap.

## `std::function`

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

## `std::async` and `std::future`

The delegate library's asynchronous features differ from `std::async` in that the caller-specified thread of control is used to invoke the target function bound to the delegate. The asynchronous variants copy the argument data into the event queue, ensuring safe transport to the destination thread, regardless of the argument type. This approach provides 'fire and forget' functionality, allowing the caller to avoid waiting or worrying about out-of-scope stack variables being accessed by the target thread.

In short, the delegate library offers features that are not natively available in the C++ standard library to ease multi-threaded application development.

 ## `Delegate` vs. `std::async` Feature Comparisons

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

# Using the Code

The delegate library is comprised of delegates and delegate containers. 

## Delegates 

A delegate binds to a single callable function. The delegate classes are:

```cpp
// Delegates
DelegateBase
    Delegate<>
        DelegateFree<>
            DelegateFreeAsync<>
            DelegateFreeAsyncWait<>
        DelegateMember<>
            DelegateMemberAsync<>
            DelegateMemberAsyncWait<>
        DelegateFunction<>
            DelegateFunctionAsync<>
            DelegateFunctionAsyncWait<>

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

## Asynchronous Non-Blocking Delegates

Create an asynchronous delegate by adding an extra thread argument to `MakeDelegate()`.

```cpp
WorkerThread workerThread1("WorkerThread1");
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

## Asynchronous Blocking Delegates

Create an asynchronous blocking delegate by adding an thread and timeout arguments to `MakeDelegate()`.

```cpp
WorkerThread workerThread1("WorkerThread1");
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
    virtual std::ostream& write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& read(std::istream& is, Args&... args) = 0;
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

## Fixed-Block Memory Allocator

The delegate library optionally uses a fixed-block memory allocator when `USE_ALLOCATOR` is defined. See `DelegateOpt.h`, `CMakeLists.txt`, and the `Allocator` directory for more details. The allocator design is available in the [stl_allocator](https://github.com/endurodave/stl_allocator) repository.

`std::function` used within class `DelegateFunction` may use the heap under certain conditions. Implement a custom `xfunction` similar to the `xlist` concept within `xlist.h` using the `xallocator` fixed-block allocator if deemed necessary.

## Error Handling

The delegate library uses dynamic memory to send asynchronous delegate messages to the target thread. By default, out-of-memory failures throw a `std::bad_alloc` exception. Optionally, if `USE_ASSERTS` is defined, exceptions are not thrown, and an assert is triggered instead. See `DelegateOpt.h` for more details.

## Function Argument Copy

The behavior of the delegate library when invoking asynchronous non-blocking delegates (e.g. `DelegateAsyncFree<>`) is to copy arguments into heap memory for safe transport to the destination thread. All arguments (if any) are duplicated. If your data is not plain old data (POD) and cannot be bitwise copied, ensure you implement an appropriate copy constructor to handle the copying.

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

# Delegate Library

A single include `DelegateLib.h` provides access to all delegate library features. The library is wrapped within a `DelegateLib` namespace. The table below shows the delegate class hierarchy.

```cpp
// Delegates
DelegateBase
    Delegate<>
        DelegateFree<>
            DelegateFreeAsync<>
            DelegateFreeAsyncWait<>
        DelegateMember<>
            DelegateMemberAsync<>
            DelegateMemberAsyncWait<>
        DelegateFunction<>
            DelegateFunctionAsync<>
            DelegateFunctionAsyncWait<>

// Delegate Containers
UnicastDelegate<>
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

# Delegate Library Interfaces

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

`DispatchDelegate()` inserts a message into the thread message queue. `WorkerThread` class uses a underlying `std::thread`. `WorkerThread` is an implementation detail; create a unique `DispatchDelegate()` function based on the platform operating system API.

```cpp
void WorkerThread::DispatchDelegate(std::shared_ptr<DelegateLib::DelegateMsg> msg)
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

The `WorkerThread::Process()` thread loop is shown below. `Invoke()` is called for each incoming `MSG_DISPATCH_DELEGATE` queue message.

```cpp
void WorkerThread::Process()
{
	m_timerExit = false;
	std::thread timerThread(&WorkerThread::TimerThread, this);

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

The `ISerializer` interface is used to serializes argument data for sending to a remote destination endpoint. Each target function built-in or user defined argument is serialized and deserialized based on the system requirements. Custom serialization such as Protocol Buffers (protobuf), MessagePack, or JSON. The `examples` folder contains sample projects.

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
    virtual std::ostream& write(std::ostream& os, Args... args) = 0;

    /// Inheriting class implements the read function to unserialize data
    /// from transport. 
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments 
    /// @return The input stream
    virtual std::istream& read(std::istream& is, Args&... args) = 0;
};
```

## `IDispatcher`

The `IDispatcher` interface is used to dispatch the target function argument data to a remote destination endpoint. Custom dispatchers such as sockets, named pipes, or `ZeroMQ` is supported. The `examples` folder contains sample projects.

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

## Asynchronous API No Locks Example

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

## Asynchronous API Reinvoke Example

While creating a separate private function for an asynchronous API works, delegates allow you to simply reinvoke the same function on a different thread. A quick check determines if the caller is executing on the desired thread. If not, a temporary asynchronous delegate is created on the stack and invoked. The delegate and all original function arguments are duplicated on the heap, and the function is then reinvoked on `workerThread2`.

```cpp
void SysDataNoLock::SetSystemModeAsyncAPI(SystemMode::Type systemMode)
{
    // Is the caller executing on workerThread2?
    if (workerThread2.GetThreadId() != WorkerThread::GetCurrentThreadId())
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

## Asynchronous API Blocking Reinvoke Example

A blocking asynchronous API can be encapsulated within a class member function. The following function sets the current mode on `workerThread2` and returns the previous mode. If the caller is not executing on `workerThread2`, a blocking delegate is created and invoked on that thread. To the caller, the function appears synchronous, but the delegate ensures the function is executed on the correct thread before returning.

```cpp
SystemMode::Type SysDataNoLock::SetSystemModeAsyncWaitAPI(SystemMode::Type systemMode)
{
    // Is the caller executing on workerThread2?
    if (workerThread2.GetThreadId() != WorkerThread::GetCurrentThreadId())
    {
        // Create an asynchronous delegate and re-invoke the function call on workerThread2
        auto delegate =
            MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncWaitAPI, 
                         workerThread2, WAIT_INFINITE);
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
m_timer.Start(1000);
```
## `std::async` Thread Targeting Example

An example combining `std::async`/`std::future` and an asynchronous delegate to target a specific worker thread during communication transmission.

```cpp
static WorkerThread comm_thread("CommunicationThread");

// Assume send_data() is not thread-safe and may only be called on comm_thread context.
// A random std::async thread from the pool is unacceptable and causes cross-threading.
size_t send_data(const std::string& data) 
{
    std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
    return data.size();  // Return the 'bytes_sent' sent result
}

void AsyncFutureExample() 
{
    comm_thread.CreateThread();

    // Create an async delegate targeted at send_data()
    auto send_data_delegate = MakeDelegate(&send_data, comm_thread, WAIT_INFINITE);

    // Start the asynchronous task using std::async. send_data() will be called on 
    // comm_thread context.
    std::future<size_t> result = std::async(std::launch::async, send_data_delegate, "send_data message");

    // Do other work while send_data() is executing on comm_thread
    std::cout << "Doing other work in main thread while data is sent...\n";

    // Continue other work in the main thread while the task runs asynchronously
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Get bytes sent. This will block until send_data() completes.
    size_t bytes_sent = result.get(); 

    std::cout << "Result from send_data: " << bytes_sent << std::endl;
    comm_thread.ExitThread();
}
```
## More Examples

See the `examples` directory for additional examples.

# Testing

The current master branch build passes all unit tests and Valgrind memory tests.

## Unit Tests

Build and execute runs all the unit tests contained within the `tests\UnitTests` directory.

## Valgrind Memory Tests

[Valgrind](https://valgrind.org/) dynamic storage allocation tests were performed using the heap and fixed-block allocator builds. Valgrind is a programming tool for detecting memory leaks, memory errors, and profiling performance in applications, primarily for Linux-based systems. All tests run on Linux.

### Heap Memory Test Results

The delegate library Valgrind test results using the heap.

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

Test results with the `ENABLE_ALLOCATOR` fixed-block allocator build option enabled. Notice the fixed-block runtime uses 22MB verses 50MB for the heap build. Heap storage *recycling* mode was used by the fixed-block allocator. See [stl_allocator](https://github.com/endurodave/stl_allocator) and [xallocator](https://github.com/endurodave/xallocator) for information about the memory allocators.

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
| <a href="https://github.com/endurodave/cpp-async-delegate">cpp-async-delegate</a> | C++17    | * Function-like template syntax<br> * Any delegate target function type (member, static, free, lambda)<br>  * N target function arguments<br> * N delegate subscribers<br>  * Optional fixed-block allocator     | * Modern C++ implementation<br> * Extensive unit tests<br> * Metaprogramming and variadic templates used for library implementation<br> * Any C++17 and higher compiler |
<a href="https://github.com/endurodave/AsyncCallback">AsyncCallback</a>                               | C++      | * Traditional template syntax<br> * Callback target function type (static, free)<br> * 1 target function argument<br> * N callback subscribers                                                                                      | * Low lines of source code<br> * Compact C++ implementation<br> * Any C++ compiler                                                                                                                    |
| <a href="https://github.com/endurodave/C_AsyncCallback">C_AsyncCallback</a>                           | C        | * Macros provide type-safety<br> * Callback target function type (static, free)<br> * 1 target function argument<br> * Fixed callback subscribers (set at compile time)<br> * Optional fixed-block allocator                        | * Low lines of source code<br> * Compact C implementation<br> * Any C compiler                                            

# References

Repositories utilizing the delegate library within different multithreaded applications.

* <a href="https://github.com/endurodave/AsyncStateMachine">Asynchronous State Machine Design in C++</a> - an asynchronous C++ state machine implemented using an asynchronous delegate library.
* <a href="https://github.com/endurodave/IntegrationTestFramework">Integration Test Framework using Google Test and Delegates</a> - a multi-threaded C++ software integration test framework using Google Test and Delegate libraries.
* <a href="https://github.com/endurodave/Async-SQLite">Asynchronous SQLite API using C++ Delegates</a> - an asynchronous SQLite wrapper implemented using an asynchronous delegate library.

