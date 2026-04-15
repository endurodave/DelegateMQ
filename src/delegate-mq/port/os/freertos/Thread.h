#ifndef _THREAD_FREERTOS_H
#define _THREAD_FREERTOS_H

/// @file Thread.h
/// @see https://github.com/DelegateMQ/DelegateMQ
/// David Lafreniere, 2025.
///
/// @brief FreeRTOS implementation of the DelegateMQ IThread interface.
///
/// @details
/// This class provides a concrete implementation of the `IThread` interface using 
/// FreeRTOS primitives (Tasks and Queues). It enables DelegateMQ to dispatch 
/// asynchronous delegates to a dedicated FreeRTOS task.
///
/// @note This implementation is a basic port. For reference, the stdlib and win32
/// implementations provide additional features:
/// 1. Watchdog: Includes a ThreadCheck() heartbeat mechanism.
/// 2. Synchronized Startup: CreateThread() blocks until the worker thread is ready.
///
/// **Key Features:**
/// * **Task Integration:** Wraps a FreeRTOS `xTaskCreate` call to establish a
///   dedicated worker loop.
/// * **FullPolicy Support:** Configurable back-pressure (BLOCK or DROP) when the 
///   message queue is full.
/// * **Priority Support:** Normal and High priorities (uses `xQueueSendToFront`).
/// * **Queue-Based Dispatch:** Uses a FreeRTOS `QueueHandle_t` to receive and
///   process incoming delegate messages in a thread-safe manner.
/// * **Thread Identification:** Implements `GetThreadId()` using `TaskHandle_t`
///   to ensure correct thread context checks (used by `AsyncInvoke` optimizations).
/// * **Graceful Shutdown:** Provides mechanisms (`ExitThread`) to cleanup resources,
///   though typical embedded tasks often run forever.

#include "delegate/IThread.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string>
#include <memory>
#include <atomic>

class ThreadMsg;

/// @brief Policy applied when the FreeRTOS task queue is full.
/// @details Controls the behavior of DispatchDelegate() when the queue has no space.
///   - DROP:  xQueueSend() with timeout 0 — returns immediately, message discarded.
///   - BLOCK: xQueueSend() with portMAX_DELAY — caller blocks until space is available.
///
/// BLOCK is the default to match all other ports. For embedded targets where the
/// caller may be an ISR or high-priority task, consider using DROP to avoid
/// priority inversion or blocking at an unsafe context.
enum class FullPolicy { BLOCK, DROP };

class Thread : public dmq::IThread
{
public:
    /// Default queue size if 0 is passed
    static const size_t DEFAULT_QUEUE_SIZE = 20;

    /// Constructor
    /// @param threadName Name for the FreeRTOS task
    /// @param maxQueueSize Max number of messages in queue (0 = Default 20)
    /// @param fullPolicy Action when queue is full: BLOCK the caller (default) or DROP the message.
    Thread(const std::string& threadName, size_t maxQueueSize = 0, FullPolicy fullPolicy = FullPolicy::BLOCK);

    /// Destructor
    ~Thread();

    /// Called once to create the worker thread
    /// @return TRUE if thread is created. FALSE otherwise. 
    bool CreateThread();

    /// Returns true if the thread is created
    bool IsThreadCreated() const { return m_thread != nullptr; }

    /// Terminate the thread gracefully
    void ExitThread();

    /// Get the ID of this thread instance
    TaskHandle_t GetThreadId();

    /// Get the ID of the currently executing thread
    static TaskHandle_t GetCurrentThreadId();

    /// Returns true if the calling thread is this thread
    virtual bool IsCurrentThread() override;

    /// Get thread name
    std::string GetThreadName() { return THREAD_NAME; }

    /// Get current queue size
    size_t GetQueueSize();

    /// Set the FreeRTOS Task Priority.
    /// Can be called before or after CreateThread().
    /// @param priority FreeRTOS priority level (0 to configMAX_PRIORITIES-1)
    void SetThreadPriority(int priority);

    /// Optional: Provide a static buffer for the task stack to avoid Heap usage.
    /// @param stackBuffer Pointer to a buffer of type StackType_t. 
    /// @param stackSizeInWords Size of the buffer in WORDS (not bytes).
    void SetStackMem(StackType_t* stackBuffer, uint32_t stackSizeInWords);

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Entry point for the thread
    static void Process(void* instance);

    // Run loop called by Process
    void Run();

    const std::string THREAD_NAME;
    const FullPolicy FULL_POLICY;
    size_t m_queueSize;
    int m_priority;

    TaskHandle_t m_thread = nullptr;
    QueueHandle_t m_queue = nullptr;
    SemaphoreHandle_t m_exitSem = nullptr; // Synchronization for safe destruction
    std::atomic<bool> m_exit = false;

    // Static allocation support
    StackType_t* m_stackBuffer = nullptr;
    uint32_t m_stackSize = 1024; // Default size (words)
    StaticTask_t m_tcb;          // TCB storage for static creation
};

#endif
