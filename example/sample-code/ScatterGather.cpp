/// @file
/// @brief Scatter-gather: dispatch work to N threads simultaneously, then collect all results.
///
/// Scatter: fire N async delegates in parallel — no blocking between dispatches.
/// Gather:  wait on N futures — blocks only until the *slowest* result arrives.
/// Wall time = max(task durations), not sum — true parallel execution.
///
/// Contrast with sequential WAIT_INFINITE delegates:
///   Sequential:     fast_op(50ms) + mid_op(100ms) + slow_op(150ms) = 300ms
///   Scatter-gather: max(50ms, 100ms, 150ms)                        = 150ms
///
/// The gather uses std::promise/future to carry typed return values back to the
/// calling thread.  Each lambda captures its promise by reference; the futures
/// are joined before the function returns, so the promises always outlive the
/// async calls.

#include "ScatterGather.h"
#include "DelegateMQ.h"
#include <iostream>
#include <future>
#include <chrono>

using namespace dmq;
using namespace dmq::os;
using namespace std;

namespace Example
{
    static int fast_task(int x)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return x * 2;
    }

    static int mid_task(int x)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return x * 3;
    }

    static int slow_task(int x)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return x * 4;
    }

    // -------------------------------------------------------------------------
    // ScatterGatherExample
    //
    // Three tasks run on three separate worker threads at the same time.
    // The main thread scatters the work (non-blocking), then gathers results
    // using futures.  Total wall time should be ~150ms regardless of the
    // 50+100+150 = 300ms sum of individual task durations.
    // -------------------------------------------------------------------------
    void ScatterGatherExample()
    {
        Thread thread_a("ThreadA");
        Thread thread_b("ThreadB");
        Thread thread_c("ThreadC");
        thread_a.CreateThread();
        thread_b.CreateThread();
        thread_c.CreateThread();

        // -------------------------------------------------------------------------
        // Sequential baseline — each WAIT_INFINITE call blocks the caller until
        // the target thread responds, so tasks execute one at a time.
        // -------------------------------------------------------------------------
        auto seq_start = std::chrono::steady_clock::now();

        int seq_a = MakeDelegate(&fast_task, thread_a, WAIT_INFINITE)(10);
        int seq_b = MakeDelegate(&mid_task,  thread_b, WAIT_INFINITE)(20);
        int seq_c = MakeDelegate(&slow_task, thread_c, WAIT_INFINITE)(30);

        auto seq_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - seq_start).count();

        cout << "[Sequential]    a=" << seq_a << " b=" << seq_b << " c=" << seq_c
             << "  elapsed=" << seq_ms << "ms\n";

        // -------------------------------------------------------------------------
        // Scatter-gather — all three are dispatched before any result is awaited.
        // Each lambda captures its promise by reference; the futures guarantee the
        // promises are still alive when the async calls complete.
        // -------------------------------------------------------------------------
        std::promise<int> pa, pb, pc;
        auto fa = pa.get_future();
        auto fb = pb.get_future();
        auto fc = pc.get_future();

        auto sg_start = std::chrono::steady_clock::now();

        // Scatter: AsyncInvoke returns immediately — all three threads start now
        MakeDelegate([&pa](int x) { pa.set_value(fast_task(x)); }, thread_a).AsyncInvoke(10);
        MakeDelegate([&pb](int x) { pb.set_value(mid_task(x));  }, thread_b).AsyncInvoke(20);
        MakeDelegate([&pc](int x) { pc.set_value(slow_task(x)); }, thread_c).AsyncInvoke(30);

        // Gather: block until all results arrive
        int sg_a = fa.get();
        int sg_b = fb.get();
        int sg_c = fc.get();

        auto sg_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - sg_start).count();

        cout << "[Scatter-gather] a=" << sg_a << " b=" << sg_b << " c=" << sg_c
             << "  elapsed=" << sg_ms << "ms\n";

        thread_a.ExitThread();
        thread_b.ExitThread();
        thread_c.ExitThread();
    }
}
