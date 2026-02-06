#ifndef _THREAD_CMSIS_RTOS2_H
#define _THREAD_CMSIS_RTOS2_H

/// @file Thread.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief CMSIS-RTOS2 implementation of the DelegateMQ IThread interface.
///
/// @details
/// This class provides a concrete implementation of the `IThread` interface using 
/// the CMSIS-RTOS2 standard API (`cmsis_os2.h`). It enables DelegateMQ to dispatch 
/// asynchronous delegates to a dedicated thread on any CMSIS-compliant RTOS 
/// (e.g., Keil RTX, FreeRTOS wrapped by CMSIS, Zephyr, etc.).
///
/// **Key Features:**
/// * **Task Integration:** Wraps `osThreadNew` to establish a dedicated worker loop.
/// * **Queue-Based Dispatch:** Uses `osMessageQueue` to receive and process incoming 
///   delegate messages in a thread-safe manner.
/// * **Priority Control:** Supports runtime priority configuration via `SetThreadPriority`
///   using standard `osPriority_t` levels.
/// * **Graceful Shutdown:** Implements robust termination logic using semaphores to ensure 
///   the thread exits cleanly before destruction.

#include "delegate/IThread.h"
#include "cmsis_os2.h"
#include <string>
#include <memory>

class ThreadMsg;

class Thread : public dmq::IThread
{
public:
    /// Default queue size if 0 is passed
    static const uint32_t DEFAULT_QUEUE_SIZE = 20;

    /// Constructor
    /// @param threadName Name for the thread
    /// @param maxQueueSize Max number of messages in queue (0 = Default 20)
    Thread(const std::string& threadName, size_t maxQueueSize = 0);
    
    ~Thread();

    bool CreateThread();
    void ExitThread();

    osThreadId_t GetThreadId();
    static osThreadId_t GetCurrentThreadId();

    /// Set the thread priority.
    /// Can be called before or after CreateThread().
    void SetThreadPriority(osPriority_t priority);

    /// Get current priority
    osPriority_t GetThreadPriority();

    std::string GetThreadName() { return THREAD_NAME; }

    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Entry point
    static void Process(void* argument);
    void Run();

    osThreadId_t m_thread = NULL;
    osMessageQueueId_t m_msgq = NULL;
    osSemaphoreId_t m_exitSem = NULL; // Semaphore to signal thread completion
    
    const std::string THREAD_NAME;
    
    // Configurable sizes
    static const uint32_t STACK_SIZE = 2048; // Bytes
    
    size_t m_queueSize;
    osPriority_t m_priority;
};

#endif // _THREAD_CMSIS_RTOS2_H