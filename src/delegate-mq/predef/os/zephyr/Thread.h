#ifndef _THREAD_ZEPHYR_H
#define _THREAD_ZEPHYR_H

/// @file Thread.h
/// @brief Zephyr RTOS implementation of the DelegateMQ IThread interface.
///
/// @note This implementation is a basic port. For reference, the stdlib and win32
/// implementations provide additional features:
/// 1. Priority Support: Uses a priority queue to respect dmq::Priority.
/// 2. Back Pressure: DispatchDelegate() blocks if the queue is full.
/// 3. Watchdog: Includes a ThreadCheck() heartbeat mechanism.
/// 4. Synchronized Startup: CreateThread() blocks until the worker thread is ready.
///
#include "delegate/IThread.h"
#include <zephyr/kernel.h>
#include <string>
#include <memory>
#include <atomic>

class ThreadMsg;

class Thread : public dmq::IThread
{
public:
    /// Default queue size if 0 is passed
    static const size_t DEFAULT_QUEUE_SIZE = 20;

    Thread(const std::string& threadName, size_t maxQueueSize = 0);
    ~Thread();

    bool CreateThread();
    void ExitThread();

    // Note: k_tid_t is a struct k_thread* in Zephyr
    k_tid_t GetThreadId();
    static k_tid_t GetCurrentThreadId();

    /// Returns true if the calling thread is this thread
    virtual bool IsCurrentThread() override;

    /// Set the Zephyr Priority.
    /// Can be called before or after CreateThread().
    void SetThreadPriority(int priority);

    std::string GetThreadName() { return THREAD_NAME; }

    /// Get current queue size
    size_t GetQueueSize();

    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Thread entry point
    static void Process(void* p1, void* p2, void* p3);
    void Run();

    const std::string THREAD_NAME;
    size_t m_queueSize;
    int m_priority;

    // Zephyr Kernel Objects
    struct k_thread m_thread;
    struct k_msgq m_msgq;
    struct k_sem m_exitSem; // Semaphore to signal thread completion
    std::atomic<bool> m_exit = false;

    // Define pointer type for the message queue
    using MsgPtr = ThreadMsg*;

    // Custom deleter for Zephyr kernel memory (wraps k_free)
    using ZephyrDeleter = void(*)(void*);

    // Dynamically allocated stack and message queue buffer
    // Managed by unique_ptr but allocated via k_aligned_alloc and freed via k_free
    std::unique_ptr<char, ZephyrDeleter> m_stackMemory{nullptr, k_free};
    std::unique_ptr<char, ZephyrDeleter> m_msgqBuffer{nullptr, k_free};

    // Stack size in bytes
    static const size_t STACK_SIZE = 2048;
    // Size of one message item (the pointer)
    static const size_t MSG_SIZE = sizeof(MsgPtr);
};

#endif // _THREAD_ZEPHYR_H