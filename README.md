![License MIT](https://img.shields.io/github/license/BehaviorTree/BehaviorTree.CPP?color=blue)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_ubuntu.yml)
[![conan Ubuntu](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_clang.yml)
[![conan Windows](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml/badge.svg)](https://github.com/endurodave/cpp-async-delegate/actions/workflows/cmake_windows.yml)
[![Codecov](https://codecov.io/gh/endurodave/cpp-async-delegate/branch/master/graph/badge.svg)](https://app.codecov.io/gh/endurodave/cpp-async-delegate)

# Asynchronous Delegates in C++

The C++ delegate library allows for anonymous asynchronous invocation of any callable function on a user-specified thread of control. It is a header-only template library that is thread-safe, unit-tested, and easy to use. Multi-threaded application development is simplified through the use of delegates and delegate containers. 

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

In C++, a delegate function object encapsulates a callable entity, such as a function, method, or lambda, so it can be invoked later. A delegate is a type-safe wrapper around a callable function that allows it to be passed around, stored, or invoked at a later time, typically within different contexts or on different threads. Delegates are particularly useful for event-driven programming, callbacks, asynchronous APIs, or when you need to pass functions as arguments.

Synchronous and asynchronous delegates are available. Asynchronous variants handle both non-blocking and blocking modes with a timeout. The library supports all types of target functions, including free functions, class member functions, static class functions, lambdas, and `std::function`. It is capable of handling any function signature, regardless of the number of arguments or return value. All argument types are supported, including by value, pointers, pointers to pointers, and references. The delegate library takes care of the intricate details of function invocation across thread boundaries. 

It is always safe to call the delegate. In its null state, a call will not perform any action and will return a default-constructed return value. A delegate behaves like a normal pointer type: it can be copied, compared for equality, called, and compared to `nullptr`. Const correctness is maintained; stored const objects can only be called by const member functions.

 A delegate instance can be:

 * Copied freely.
 * Compared to same type delegates and `nullptr`.
 * Reassigned.
 * Called.
 
Typical use cases are:

* Inter-Thread Publish/Subscribe (Observer) Pattern
* Thread-Safe Asynchronous API
* Asynchronous Method Invocation (AMI)
* Anonymous, Asynchronous Thread-Safe Callbacks
* Event-Driven Programming
* Design Patterns (Active Object)
* `std::async` Thread Targeting

The delegate library's asynchronous features differ from `std::async` in that the caller-specified thread of control is used to invoke the target function bound to the delegate, rather than a random thread from the thread pool. The asynchronous variants copy the argument data into an event queue, ensuring safe transport to the destination thread, regardless of the argument type. This approach provides 'fire and forget' functionality, allowing the caller to avoid waiting or worrying about out-of-scope stack variables being accessed by the target thread.

In short, the delegate library offers features that are not natively available in the C++ standard library to ease multi-threaded application development.

Originally published on CodeProject at: <a href="https://www.codeproject.com/Articles/5277036/Asynchronous-Multicast-Delegates-in-Modern-Cpluspl">Asynchronous Multicast Delegates in Modern C++</a>

# Design Documentation

 See [Design Details](docs/DETAILS.md) for implementation design documentation and more examples.

 See [Doxygen Documentation](https://endurodave.github.io/cpp-async-delegate/html/index.html) for source code documentation. 

 See [Unit Test Code Coverage](https://app.codecov.io/gh/endurodave/cpp-async-delegate) test results.

# Quick Start

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

<img src="docs/Figure1.jpg" alt="Figure 1: AlarmCb Delegate Container" style="width:65%;">  

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

<img src="docs/Figure2.jpg" alt="Figure 2: Insert into AlarmCb Delegate Container" style="width:75%;">

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

## All Delegate Types Example

A delegate container inserting and removing all delegate types.

```cpp
WorkerThread workerThread1("WorkerThread1");

static int callCnt = 0;

void FreeFunc(int value) {
    cout << "FreeFunc " << value << " " << ++callCnt << endl;
}

// Simple test invoking all target types
void TestAllTargetTypes() {
    class Class {
    public:
        static void StaticFunc(int value) {
            cout << "StaticFunc " << value << " " << ++callCnt << endl;
        }

        void MemberFunc(int value) {
            cout << "MemberFunc " << value << " " << ++callCnt << endl;
        }

        void MemberFuncConst(int value) const {
            cout << "MemberFuncConst " << value << " " << ++callCnt << endl;
        }
    };

    int stackVal = 100;
    std::function<void(int)> LambdaCapture = [stackVal](int i) {
        std::cout << "LambdaCapture " << i + stackVal << " " << ++callCnt << endl;
    };

    std::function<void(int)> LambdaNoCapture = [](int i) {
        std::cout << "LambdaNoCapture " << i << " " << ++callCnt << endl;
    };

    std::function<void(int)> LambdaForcedCapture = +[](int i) {
        std::cout << "LambdaForcedCapture " << i << " " << ++callCnt << endl;
    };

    Class testClass;
    std::shared_ptr<Class> testClassSp = std::make_shared<Class>();

    // Create a multicast delegate container that accepts Delegate<void(int)> delegates.
    // Any function with the signature "void Func(int)".
    MulticastDelegateSafe<void(int)> delegateA;

    // Add all callable function targets to the delegate container
    // Synchronous delegates
    delegateA += MakeDelegate(&FreeFunc);
    delegateA += MakeDelegate(LambdaCapture);
    delegateA += MakeDelegate(LambdaNoCapture);
    delegateA += MakeDelegate(LambdaForcedCapture);
    delegateA += MakeDelegate(&Class::StaticFunc);
    delegateA += MakeDelegate(&testClass, &Class::MemberFunc);
    delegateA += MakeDelegate(&testClass, &Class::MemberFuncConst);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFunc);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFuncConst);

    // Asynchronous delegates
    delegateA += MakeDelegate(&FreeFunc, workerThread1);
    delegateA += MakeDelegate(LambdaCapture, workerThread1);
    delegateA += MakeDelegate(LambdaNoCapture, workerThread1);
    delegateA += MakeDelegate(LambdaForcedCapture, workerThread1);
    delegateA += MakeDelegate(&Class::StaticFunc, workerThread1);
    delegateA += MakeDelegate(&testClass, &Class::MemberFunc, workerThread1);
    delegateA += MakeDelegate(&testClass, &Class::MemberFuncConst, workerThread1);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFunc, workerThread1);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFuncConst, workerThread1);

    // Asynchronous blocking delegates
    delegateA += MakeDelegate(&FreeFunc, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(LambdaCapture, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(LambdaNoCapture, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(LambdaForcedCapture, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(&Class::StaticFunc, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(&testClass, &Class::MemberFunc, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(&testClass, &Class::MemberFuncConst, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFunc, workerThread1, WAIT_INFINITE);
    delegateA += MakeDelegate(testClassSp, &Class::MemberFuncConst, workerThread1, WAIT_INFINITE);

    // Invoke all callable function targets stored within the delegate container
    delegateA(123);

    // Wait for async delegate invocations to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Remove all callable function targets from the delegate container
    // Synchronous delegates
    delegateA -= MakeDelegate(&FreeFunc);
    delegateA -= MakeDelegate(LambdaCapture);
    delegateA -= MakeDelegate(LambdaNoCapture);
    delegateA -= MakeDelegate(LambdaForcedCapture);
    delegateA -= MakeDelegate(&Class::StaticFunc);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFunc);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFuncConst);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFunc);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFuncConst);

    // Asynchronous delegates
    delegateA -= MakeDelegate(&FreeFunc, workerThread1);
    delegateA -= MakeDelegate(LambdaCapture, workerThread1);
    delegateA -= MakeDelegate(LambdaNoCapture, workerThread1);
    delegateA -= MakeDelegate(LambdaForcedCapture, workerThread1);
    delegateA -= MakeDelegate(&Class::StaticFunc, workerThread1);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFunc, workerThread1);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFuncConst, workerThread1);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFunc, workerThread1);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFuncConst, workerThread1);

    // Asynchronous blocking delegates
    delegateA -= MakeDelegate(&FreeFunc, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(LambdaCapture, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(LambdaNoCapture, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(LambdaForcedCapture, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(&Class::StaticFunc, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFunc, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(&testClass, &Class::MemberFuncConst, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFunc, workerThread1, WAIT_INFINITE);
    delegateA -= MakeDelegate(testClassSp, &Class::MemberFuncConst, workerThread1, WAIT_INFINITE);

    ASSERT_TRUE(delegateA.Size() == 0);
    ASSERT_TRUE(callCnt == 27);
}
```

## Asynchronous API Example

`SetSystemModeAsyncAPI()` is an asynchronous function call that re-invokes on `workerThread2` if necessary. 

```cpp
void SysDataNoLock::SetSystemModeAsyncAPI(SystemMode::Type systemMode)
{
	// Is the caller executing on workerThread2?
	if (workerThread2.GetThreadId() != WorkerThread::GetCurrentThreadId())
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

# Delegate Classes

Primary delegate library classes.

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

`DelegateFreeRemote<>`, `DelegateMemberRemote<>` and `DelegateFunctionRemote<>` provides non-blocking remote function execution. `ISerializer` and `IRemoteInvoker` interfaces support integration with any system. 

The three main delegate container classes are:

```cpp
// Delegate Containers
UnicastDelegate<>
MulticastDelegate<>
    MulticastDelegateSafe<>
``` 

`UnicastDelegate<>` is a delegate container accepting a single delegate.

`MulticastDelegate<>` is a delegate container accepting multiple delegates.

`MultcastDelegateSafe<>` is a thread-safe container accepting multiple delegates. Always use the thread-safe version if multiple threads access the container instance.

# Project Build

<a href="https://www.cmake.org">CMake</a> is used to create the build files. CMake is free and open-source software. Windows, Linux and other toolchains are supported. Example CMake console commands executed inside the project root directory: 

## Windows Visual Studio

`cmake -G "Visual Studio 17 2022" -A Win32 -B build -S .`

`cmake -G "Visual Studio 17 2022" -A x64 -B build -S .`

`cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DENABLE_ALLOCATOR=ON`

After executed, open the Visual Studio project from within the `build` directory.

<img src="docs/Figure3.jpg" alt="Figure 3: Visual Studio Build" style="width:100%;">

**Figure 3: Visual Studio Build**

## Linux Make

`cmake -G "Unix Makefiles" -B build -S .`

`cmake -G "Unix Makefiles" -B build -S . -DENABLE_ALLOCATOR=ON`

After executed, build the software from within the `build` directory using the command `make`. Run the console app using `./DelegateApp`.

<img src="docs/Figure4.jpg" alt="Figure 4: Linux Makefile Build" style="width:70%;">

**Figure 4: Linux Makefile Build**

# Related Repositories

## Alternative Implementations

Alternative asynchronous implementations similar in concept to C++ delegate.

* <a href="https://github.com/endurodave/AsyncCallback">Asynchronous Callbacks in C++</a> - A C++ asynchronous callback framework simplifies passing data between threads.
* <a href="https://github.com/endurodave/C_AsyncCallback">Asynchronous Callbacks in C</a> - A C language asynchronous callback framework simplifies passing data between threads.

## Projects Using Delegates

Repositories utilizing the delegate library within different multithreaded applications.

* <a href="https://github.com/endurodave/AsyncStateMachine">Asynchronous State Machine Design in C++</a> - an asynchronous C++ state machine implemented using an asynchronous delegate library.
* <a href="https://github.com/endurodave/IntegrationTestFramework">Integration Test Framework using Google Test and Delegates</a> - a multi-threaded C++ software integration test framework using Google Test and Delegate libraries.
* <a href="https://github.com/endurodave/Async-SQLite">Asynchronous SQLite API using C++ Delegates</a> - an asynchronous SQLite wrapper implemented using an asynchronous delegate library.







