/// @file
/// @brief C++20 coroutine integration with DelegateMQ async delegates using co_await.
///
/// KEY CONCEPT -co_await vs. blocking AsyncInvoke (WAIT_INFINITE):
///
///   Both return a typed result and both look sequential in the source. The
///   difference is what the CALLING THREAD does while the target thread works:
///
///   WAIT_INFINITE (blocking):
///     Calling thread is FROZEN -it cannot process other messages or do any
///     work until the target thread finishes. On an RTOS task this stalls the
///     entire message queue for the duration.
///
///   co_await (suspending):
///     Calling thread is RELEASED at each co_await and returns to its normal
///     work. The coroutine resumes automatically once the target thread
///     finishes. The calling thread stays responsive the whole time.
///
///   The CoroutineExample() below demonstrates this by having the main thread
///   print progress ticks while sensor_thread runs Read(). With WAIT_INFINITE
///   the ticks would print only AFTER all reads complete. With co_await they
///   interleave -the main thread is genuinely free between each read.
///
/// IMPLEMENTATION NOTE:
///   DelegateAwaitable is a thin adapter over existing DelegateMQ primitives.
///   await_suspend posts work to the target thread via MakeDelegate AsyncInvoke
///   (fire-and-forget). No core library changes are required.

#if defined(_MSVC_LANG) && _MSVC_LANG >= 202002L || __cplusplus >= 202002L

#include "Coroutine.h"
#include "DelegateMQ.h"
#include <coroutine>
#include <functional>
#include <iostream>
#include <chrono>

using namespace dmq;
using namespace dmq::os;
using namespace std;

namespace Example
{
    // -------------------------------------------------------------------------
    // Minimal fire-and-forget coroutine task type.
    // initial_suspend returns suspend_never so the coroutine starts running
    // immediately on the caller's thread up to the first co_await.
    // -------------------------------------------------------------------------
    struct Task {
        struct promise_type {
            Task get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() { std::terminate(); }
        };
    };

    // -------------------------------------------------------------------------
    // DelegateAwaitable: co_await adapter over existing DelegateMQ dispatch.
    //
    // Three methods are required by the coroutine protocol:
    //
    //   await_ready()   -return false: always suspend, never skip the wait.
    //
    //   await_suspend() -called when the coroutine suspends. Posts a lambda
    //                     to the target thread using the existing MakeDelegate
    //                     AsyncInvoke (fire-and-forget) path. The lambda calls
    //                     the stored task, saves the result, then resumes the
    //                     coroutine handle. The calling thread is free as soon
    //                     as await_suspend() returns.
    //
    //   await_resume()  -called when the coroutine is resumed. Returns the
    //                     result that was stored by the lambda on the target
    //                     thread. After this point the coroutine continues
    //                     running on the target thread.
    // -------------------------------------------------------------------------
    template<typename Ret>
    struct DelegateAwaitable {
        std::function<Ret()> m_task;
        Thread& m_thread;
        Ret m_result{};

        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            // Post to target thread. Calling thread is released when this returns.
            auto work = [this, h]() mutable {
                m_result = m_task();
                h.resume();  // coroutine continues on m_thread from here
            };
            MakeDelegate(work, m_thread).AsyncInvoke();
        }

        Ret await_resume() noexcept { return m_result; }
    };

    // Helper: build an awaitable from a shared_ptr, member function, and target thread.
    template<typename Obj, typename Ret, typename... Args>
    auto CoAwait(std::shared_ptr<Obj> obj, Ret(Obj::*func)(Args...), Thread& thread, Args... args)
    {
        return DelegateAwaitable<Ret>{
            [obj, func, args...]() mutable { return (obj.get()->*func)(args...); },
            thread
        };
    }

    // -------------------------------------------------------------------------
    // Sensor: thread-affine -Read() must only be called on sensor_thread.
    // Sleeps 30ms to simulate hardware I/O latency.
    // -------------------------------------------------------------------------
    class Sensor : public std::enable_shared_from_this<Sensor>
    {
    public:
        int Read(int channel)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));  // simulate I/O
            int value = channel * 100 + 42;
            cout << "[sensor_thread] ch" << channel << " = " << value << "\n";
            return value;
        }
    };

    // -------------------------------------------------------------------------
    // Process: three sequential channel reads as a coroutine.
    //
    // At each co_await the coroutine suspends and Read() is dispatched to
    // sensor_thread. The calling thread is released immediately -it does not
    // wait here. When sensor_thread finishes the read it resumes the coroutine
    // and execution continues with the result in hand.
    //
    // The logic is written top-to-bottom, like synchronous code, with no
    // callbacks and no state machine enum needed to carry ch0/ch1 across calls.
    // -------------------------------------------------------------------------
    static Task Process(std::shared_ptr<Sensor> sensor, Thread& sensor_thread)
    {
        cout << "[coroutine]     started\n";

        // co_await suspends here. sensor_thread calls Read(0). Calling thread is free.
        int ch0 = co_await CoAwait(sensor, &Sensor::Read, sensor_thread, 0);
        cout << "[coroutine]     ch0 = " << ch0 << "\n";

        // co_await suspends again. Calling thread remains free during Read(1).
        int ch1 = co_await CoAwait(sensor, &Sensor::Read, sensor_thread, 1);
        cout << "[coroutine]     ch1 = " << ch1 << "\n";

        // co_await suspends again. Calling thread remains free during Read(2).
        int ch2 = co_await CoAwait(sensor, &Sensor::Read, sensor_thread, 2);
        cout << "[coroutine]     ch2 = " << ch2 << "\n";

        cout << "[coroutine]     sum = " << (ch0 + ch1 + ch2) << "\n";
    }

    // -------------------------------------------------------------------------
    // CoroutineExample
    //
    // Expected output shows the main thread ticking while sensor_thread reads:
    //
    //   [coroutine]     started
    //   [main]          tick 1 -free while sensor_thread works
    //   [sensor_thread] ch0 = 42
    //   [coroutine]     ch0 = 42
    //   [main]          tick 2 -free while sensor_thread works
    //   [main]          tick 3 -free while sensor_thread works
    //   [sensor_thread] ch1 = 142
    //   ...
    //
    // With WAIT_INFINITE (blocking) all ticks would print AFTER ch2 completes
    // because the calling thread cannot proceed until all three reads finish.
    // -------------------------------------------------------------------------
    void CoroutineExample()
    {
        Thread sensor_thread("SensorThread");
        sensor_thread.CreateThread();

        auto sensor = xmake_shared<Sensor>();

        // Launch the coroutine. It runs synchronously until the first co_await,
        // then suspends and returns control here. sensor_thread drives the rest.
        Process(sensor, sensor_thread);

        // The main thread is now FREE. It prints ticks while sensor_thread works.
        // These ticks interleave with the sensor reads, proving the calling thread
        // was never blocked. With WAIT_INFINITE the calling thread would be frozen
        // and no ticks would appear until after all three reads had finished.
        for (int i = 1; i <= 9; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cout << "[main]          tick " << i << " - free while sensor_thread works\n";
        }

        sensor_thread.ExitThread();
    }
}

#endif  // C++20
