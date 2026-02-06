#ifndef _THREAD_THREADX_H
#define _THREAD_THREADX_H

/// @file Thread.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief ThreadX implementation of the DelegateMQ IThread interface.
///
/// @details
/// This class provides a concrete implementation of the `IThread` interface using 
/// Azure RTOS ThreadX primitives. It enables DelegateMQ to dispatch asynchronous 
/// delegates to a dedicated ThreadX thread.
///
/// **Key Features:**
/// * **Task Integration:** Wraps `tx_thread_create` to establish a dedicated worker loop.
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

class ThreadMsg;

class Thread : public dmq::IThread
{
public:
    /// Default queue size if 0 is passed
    static const ULONG DEFAULT_QUEUE_SIZE = 20;

    /// Constructor
    /// @param threadName Name for the ThreadX thread
    /// @param maxQueueSize Max number of messages in queue (0 = Default 20)
    Thread(const std::string& threadName, size_t maxQueueSize = 0);

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

    /// Set the ThreadX Priority (0 = Highest). 
    /// Can be called before or after CreateThread().
    void SetThreadPriority(UINT priority);

    /// Get current priority
    UINT GetThreadPriority();

    /// Get thread name
    std::string GetThreadName() { return THREAD_NAME; }

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Entry point for the thread
    static void Process(ULONG instance);

    // Run loop called by Process
    void Run();

    // ThreadX Control Blocks
    TX_THREAD m_thread;
    TX_QUEUE m_queue;
    TX_SEMAPHORE m_exitSem; // Semaphore to signal thread completion

    // Memory buffers required by ThreadX (Managed by RAII)
    // Using ULONG[] ensures correct alignment for ThreadX stacks and queues
    std::unique_ptr<ULONG[]> m_stackMemory;
    std::unique_ptr<ULONG[]> m_queueMemory;
    
    const std::string THREAD_NAME;
    
    // Configurable stack size (bytes)
    static const ULONG STACK_SIZE = 2048; 
    
    size_t m_queueSize; // Stored queue size
    UINT m_priority;    // Stored priority
};

#endif // _THREAD_THREADX_H