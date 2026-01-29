#ifndef _THREAD_STD_H
#define _THREAD_STD_H

/// @file Thread.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
///
/// @brief Standard C++ implementation of the DelegateMQ IThread interface.
///
/// @details
/// This class provides a cross-platform implementation of the `IThread` interface using 
/// standard C++11 primitives (`std::thread`, `std::mutex`, `std::condition_variable`). 
/// It creates a dedicated worker thread with an event loop capable of processing 
/// asynchronous delegates and system messages.
///
/// **Key Features:**
/// * **Priority Queue:** Uses `std::priority_queue` to ensure high-priority delegate 
///   messages (e.g., system signals) are processed before lower-priority ones.
/// * **Back Pressure:** Supports a configurable `maxQueueSize`. If the queue is full, 
///   `DispatchDelegate()` blocks the caller until space is available, preventing memory exhaustion.
/// * **Watchdog Integration:** Includes a built-in heartbeat mechanism. If the thread loop 
///   stalls (deadlock or infinite loop), the watchdog timer detects the failure.
/// * **Synchronized Start:** Uses `std::promise` and `std::future` to ensure the thread 
///   is fully initialized and running before `CreateThread()` returns.
/// * **Debug Support:** Sets the native thread name (on supported OSs like Windows) to 
///   aid debugging in IDEs.

#include "delegate/IThread.h"
#include "./predef/util/Timer.h"
#include "ThreadMsg.h"
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <future>
#include <optional>

// Comparator for priority queue
struct ThreadMsgComparator {
    bool operator()(const std::shared_ptr<ThreadMsg>& a, const std::shared_ptr<ThreadMsg>& b) const {
        return static_cast<int>(a->GetPriority()) < static_cast<int>(b->GetPriority());
    }
};

/// @brief Cross-platform thread for any system supporting C++11 std::thread (e.g. Windows, Linux).
/// @details The Thread class creates a worker thread capable of dispatching and 
/// invoking asynchronous delegates.
class Thread : public dmq::IThread
{
public:
    /// Constructor
    /// @param threadName The name of the thread for debugging.
    /// @param maxQueueSize The maximum number of messages allowed in the queue. 
    ///                     0 means unlimited (no back pressure).
    Thread(const std::string& threadName, size_t maxQueueSize = 0);

    /// Destructor
    ~Thread();

    /// Called once to create the worker thread. If watchdogTimeout value 
    /// provided, the maximum watchdog interval is used. Otherwise no watchdog.
    /// @param[in] watchdogTimeout - optional watchdog timeout.
    /// @return TRUE if thread is created. FALSE otherwise. 
    bool CreateThread(std::optional<dmq::Duration> watchdogTimeout = std::nullopt);

    /// Called once at program exit to shut down the worker thread
    void ExitThread();

    /// Get the ID of this thread instance
    std::thread::id GetThreadId();

    /// Get the ID of the currently executing thread
    static std::thread::id GetCurrentThreadId();

    /// Get thread name
    std::string GetThreadName() { return THREAD_NAME; }

    /// Get size of thread message queue.
    size_t GetQueueSize();

    /// Dispatch and invoke a delegate target on the destination thread.
    /// @param[in] msg - Delegate message containing target function 
    /// arguments.
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg);

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Entry point for the thread
    void Process();

    void SetThreadName(std::thread::native_handle_type handle, const std::string& name);

    /// Check watchdog is expired. This function is called by the thread 
    /// the calls Timer::ProcessTimers(). This function is thread-safe.
    /// In a real-time OS, Timer::ProcessTimers() typically is called by the highest
    /// priority task in the system.
    void WatchdogCheck();

    /// Timer expiration function used to check that thread loop is running.
    /// This function is called by this thread context (m_thread). The 
    /// Thread::Process() function must be called periodically even if no
    /// other user delegate events to be handled.
    void ThreadCheck();

    std::unique_ptr<std::thread> m_thread;
    std::priority_queue<std::shared_ptr<ThreadMsg>,
        std::vector<std::shared_ptr<ThreadMsg>>,
        ThreadMsgComparator> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cv;

    // Condition variable to wake up blocked producers when space is available
    std::condition_variable m_cvNotFull;

    const std::string THREAD_NAME;

    // Max queue size for back pressure (0 = unlimited)
    const size_t MAX_QUEUE_SIZE;

    // Promise and future to synchronize thread start
    std::promise<void> m_threadStartPromise;
    std::future<void> m_threadStartFuture;

    std::atomic<bool> m_exit;

    // Watchdog related members
    std::atomic<dmq::TimePoint> m_lastAliveTime;
    std::unique_ptr<Timer> m_watchdogTimer;
    dmq::ScopedConnection m_watchdogTimerConn;
    std::unique_ptr<Timer> m_threadTimer;
    dmq::ScopedConnection m_threadTimerConn;
    std::atomic<dmq::Duration> m_watchdogTimeout;
};

#endif
