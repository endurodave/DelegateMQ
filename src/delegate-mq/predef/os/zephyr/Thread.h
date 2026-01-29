#ifndef _THREAD_ZEPHYR_H
#define _THREAD_ZEPHYR_H

/// @file Thread.h
/// @brief Zephyr RTOS implementation of the DelegateMQ IThread interface.

#include "delegate/IThread.h"
#include <zephyr/kernel.h>
#include <string>
#include <memory>

class ThreadMsg;

class Thread : public dmq::IThread
{
public:
    Thread(const std::string& threadName);
    ~Thread();

    bool CreateThread();
    void ExitThread();

    // Note: k_tid_t is a struct k_thread* in Zephyr
    k_tid_t GetThreadId();
    static k_tid_t GetCurrentThreadId();
    std::string GetThreadName() { return THREAD_NAME; }

    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    // Thread entry point
    static void Process(void* p1, void* p2, void* p3);
    void Run();

    // Zephyr Kernel Objects
    struct k_thread m_thread;
    struct k_msgq m_msgq;
    struct k_sem m_exitSem; // Semaphore to signal thread completion

    // Define pointer type for the message queue
    using MsgPtr = ThreadMsg*;

    // Custom deleter for Zephyr kernel memory (wraps k_free)
    using ZephyrDeleter = void(*)(void*);

    // Dynamically allocated stack and message queue buffer
    // Managed by unique_ptr but allocated via k_aligned_alloc and freed via k_free
    std::unique_ptr<char, ZephyrDeleter> m_stackMemory{nullptr, k_free};
    std::unique_ptr<char, ZephyrDeleter> m_msgqBuffer{nullptr, k_free};

    const std::string THREAD_NAME;
    
    // Stack size in bytes
    static const size_t STACK_SIZE = 2048;
    // Max items in queue
    static const size_t MSGQ_MAX_MSGS = 20;
    // Size of one message item (the pointer)
    static const size_t MSG_SIZE = sizeof(MsgPtr);
};

#endif // _THREAD_ZEPHYR_H