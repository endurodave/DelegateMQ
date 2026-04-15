#ifndef _THREAD_THREADX_H
#define _THREAD_THREADX_H

/// @file Thread.h
/// @see https://github.com/DelegateMQ/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief ThreadX implementation of the DelegateMQ IThread interface.
///
/// @details
/// This class provides a concrete implementation of the `IThread` interface using 
/// Azure RTOS ThreadX primitives. It enables DelegateMQ to dispatch asynchronous 
/// delegates to a dedicated ThreadX thread.
///
/// @note This implementation is a basic port. For reference, the stdlib and win32
/// implementations provide additional features:
/// 1. Watchdog: Includes a ThreadCheck() heartbeat mechanism.
/// 2. Synchronized Startup: CreateThread() blocks until the worker thread is ready.
///
/// **Key Features:**
/// * **Task Integration:** Wraps `tx_thread_create` to establish a dedicated worker loop.
/// * **FullPolicy Support:** Configurable back-pressure (BLOCK or DROP) when the 
///   message queue is full.
/// * **Priority Support:** Normal and High priorities (uses `tx_queue_front_send`).
/// * **Queue-Based Dispatch:** Uses a `TX_QUEUE` to receive and process incoming 
///   delegate messages in a thread-safe manner.
/// * **Priority Control:** Supports runtime priority configuration via `SetThreadPriority`.
/// * **Dynamic Configuration:** Allows configuring stack size and queue depth at construction.
/// * **Graceful Shutdown:** Implements robust termination logic using semaphores to ensure 
///   the thread exits cleanly before destruction.

#include "delegate/IThread.h"
#include "tx_api.h"
#include <string>
#include <memory>
#include <vector>
#include <atomic>

class ThreadMsg;

/// @brief Policy applied when the thread message queue is full.
/// @details Only meaningful when maxQueueSize > 0.
///   - BLOCK: DispatchDelegate() blocks the caller until space is available (back pressure).
///   - DROP:  DispatchDelegate() silently discards the message and returns immediately.
///
/// Use DROP for high-rate best-effort topics (sensor telemetry, display updates) where
/// a stale sample is preferable to stalling the publisher. Use BLOCK for critical topics
/// (commands, state transitions) where no message may be lost.
enum class FullPolicy { BLOCK, DROP };

class Thread : public dmq::IThread
{
public:
    /// Default queue size if 0 is passed
    static const ULONG DEFAULT_QUEUE_SIZE = 20;

    /// Constructor
    /// @param threadName Name for the ThreadX thread
    /// @param maxQueueSize Max number of messages in queue (0 = Default 20)
    /// @param fullPolicy Action when queue is full: BLOCK the caller or DROP the message.
    Thread(const std::string& threadName, size_t maxQueueSize = 0, FullPolicy fullPolicy = FullPolicy::BLOCK);

    /// Destructor
    ~Thread();

    /// Called once to create the worker thread
    /// @return TRUE if thread is created. FALSE otherwise. 
    bool CreateThread();

    /// Terminate the thread gracefully
    void ExitThread();

    /// Get the ID of this thread instance
    TX_THREAD* GetThreadId();

    /// Get the ID of the currently executing thread
    static TX_THREAD* GetCurrentThreadId();

    /// Returns true if the calling thread is this thread
    virtual bool IsCurrentThread() override;

    /// Set the ThreadX Priority (0 = Highest). 
    /// Can be called before or after CreateThread().
    void SetThreadPriority(UINT priority);

    /// Get current priority
    UINT GetThreadPriority();

    /// Get thread name
    std::string GetThreadName() { return THREAD_NAME; }

    /// Get current queue size
    size_t GetQueueSize();

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Entry point for the thread
    static void Process(ULONG instance);

    // Run loop called by Process
    void Run();

    const std::string THREAD_NAME;
    const size_t m_queueSize; // Stored queue size
    const FullPolicy FULL_POLICY;
    UINT m_priority;    // Stored priority

    // ThreadX Control Blocks
    TX_THREAD m_thread;
    TX_QUEUE m_queue;
    TX_SEMAPHORE m_exitSem; // Semaphore to signal thread completion
    std::atomic<bool> m_exit = false;

    // Memory buffers required by ThreadX (Managed by RAII)
    // Using ULONG[] ensures correct alignment for ThreadX stacks and queues
    std::unique_ptr<ULONG[]> m_stackMemory;
    std::unique_ptr<ULONG[]> m_queueMemory;
    
    // Configurable stack size (bytes)
    static const ULONG STACK_SIZE = 2048; 
};

#endif // _THREAD_THREADX_H