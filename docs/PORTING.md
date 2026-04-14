# Porting Guide

Numerous predefined platforms are already supported — Windows, Linux, FreeRTOS, ARM bare-metal, ThreadX, Zephyr, CMSIS-RTOS2, and Qt. Ready-made plugins for threading and communication interfaces exist, or you can create new ones.

---

## Table of Contents

- [Porting Checklist](#porting-checklist)
- [Embedded Systems](#embedded-systems)
- [Interfaces](#interfaces)
  - [`IThread`](#ithread)
    - [Send `DelegateMsg`](#send-delegatemsg)
    - [Receive `DelegateMsg`](#receive-delegatemsg)
  - [`ISerializer`](#iserializer)
  - [`IDispatcher`](#idispatcher)

---

## Porting Checklist

1. **Search codebase for `@TODO`** — find specific decision locations tagged in the source files.
2. **Implement `IThread`** — required to use **Asynchronous** delegates.
3. **Implement `ISerializer` and `ITransport`** — required to use **Remote** delegates across processes/processors.
   - *Optional:* Implement `ITransportMonitor` if your application layer requires command acknowledgments (ACKs).
   - See [Sample Projects](DETAILS.md#sample-projects) for numerous remote delegate examples.
4. **Check System Clock** — ensure `std::chrono::steady_clock` is supported on your target hardware, as it is required for timers and transport timeouts. Otherwise, change `dmq::Clock` in `DelegateOpt.h` to a new clock type.
5. **Call `Timer::ProcessTimers()`** — periodically call `ProcessTimers()` (e.g., from a main loop or hardware timer ISR) to support timers and thread watchdogs.
6. **Configure Build Options** — set CMake DMQ library build options within `CMakeLists.txt`.
   - Example: `DMQ_ASSERTS` for debug assertions.
   - Example: `DMQ_ALLOCATOR` to switch between standard heap (`new`/`delete`) and the deterministic Fixed Block Allocator.
7. **Implement Fault Handling** — customize `Fault.cpp` to route errors to your system's logger or crash handler.

---

## Embedded Systems

Running C++ messaging on embedded targets (like STM32) requires specific attention to resources.

1. **Stack Usage & Debug Mode**

   **Issue:** In Debug mode (`-O0`), C++ templates generate deep call stacks, possibly causing stack overflows.  
   **Fix:** Increase task stack (e.g. 8 KB) or use Release mode (`-O2`), where stacks shrink significantly.

2. **Transport Implementation (if using remote delegates)**

   **Issue:** Blocking UART calls (e.g., `HAL_UART_Receive`) starve high-priority tasks.  
   **Fix:** Use an interrupt-driven ring buffer. The ISR captures data immediately, and the receive task sleeps on a semaphore until data is available.

See the `stm32-freertos` example `README.md` for a complete implementation of static stacks, interrupt-driven UART, and correct FreeRTOS configuration.

---

## Interfaces

DelegateMQ interface classes allow customizing the library's runtime behavior.

### `IThread`

The `IThread` interface sends a delegate and its argument data through a message queue. A delegate thread is required to dispatch asynchronous delegates to a specified target thread. The delegate library automatically constructs a `DelegateMsg` containing everything necessary for the destination thread.

```cpp
class IThread
{
public:
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

#### Send `DelegateMsg`

An asynchronous delegate's `operator()(Args... args)` calls `DispatchDelegate()` to send a delegate message to the destination thread.

```cpp
auto thread = this->GetThread();
if (thread) {
    // Dispatch message onto the callback destination thread. Invoke()
    // will be called by the destination thread.
    thread->DispatchDelegate(msg);
}
```

`DispatchDelegate()` inserts a message into the thread's message queue. The `Thread` class uses an underlying `std::thread` on stdlib/Win32 platforms and a FreeRTOS task on embedded. Create a unique `DispatchDelegate()` implementation based on your platform's OS API.

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

#### Receive `DelegateMsg`

Inherit from `IDelegateInvoker` and implement the `Invoke()` function. The destination thread calls `Invoke()` once the message is dequeued.

```cpp
/// @brief Abstract base class to support asynchronous delegate function invoke
/// on destination thread of control.
///
/// @details Inherit from this class and implement `Invoke()`. The implementation
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

The `Thread::Process()` loop below shows the dispatch pattern. `Invoke()` is called for each incoming `MSG_DISPATCH_DELEGATE` queue message.

```cpp
void Thread::Process()
{
    m_timerExit = false;
    std::thread timerThread(&Thread::TimerThread, this);

    while (1)
    {
        std::shared_ptr<ThreadMsg> msg;
        {
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
                auto delegateMsg = msg->GetData();
                ASSERT_TRUE(delegateMsg);

                auto invoker = delegateMsg->GetDelegateInvoker();
                ASSERT_TRUE(invoker);

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

---

### `ISerializer`

The `ISerializer` interface serializes argument data for sending to a remote destination endpoint. Each argument is serialized and deserialized according to system requirements. Custom serialization libraries such as [MessagePack](https://msgpack.org/index.html) are supported. The `examples/sample-projects` folder contains working examples.

```cpp
template <class R>
struct ISerializer; // Not defined

/// @brief Delegate serializer interface for serializing and deserializing
/// remote delegate arguments. Implemented by application code if remote
/// delegates are used.
///
/// @details All argument data is serialized into a stream. `Write()` is called
/// by the sender when the delegate is invoked. `Read()` is called by the receiver
/// upon reception of the remote message data bytes.
template<class RetType, class... Args>
class ISerializer<RetType(Args...)>
{
public:
    /// Serialize data for transport.
    /// @param[out] os The output stream
    /// @param[in] args The target function arguments
    /// @return The output stream
    virtual std::ostream& Write(std::ostream& os, Args... args) = 0;

    /// Deserialize data from transport.
    /// @param[in] is The input stream
    /// @param[out] args The target function arguments
    /// @return The input stream
    virtual std::istream& Read(std::istream& is, Args&... args) = 0;
};
```

---

### `IDispatcher`

The `IDispatcher` interface dispatches serialized argument data to a remote destination endpoint. Custom dispatchers using sockets, named pipes, ZeroMQ, or any other transport are supported. The `examples/sample-projects` folder contains working examples.

```cpp
/// @brief Delegate interface class to dispatch serialized function argument data
/// to a remote destination. Implemented by the application if using remote delegates.
///
/// @details Incoming data from the remote must call `IDispatcher::Dispatch()` to
/// invoke the target function using argument data. The argument data is serialized
/// for transport using a concrete class implementing the `ISerializer` interface,
/// allowing any serialization method to be used.
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
