#ifndef _THREAD_CMSIS_RTOS2_H
#define _THREAD_CMSIS_RTOS2_H

/// @file Thread.h
/// @brief CMSIS-RTOS2 implementation of the DelegateMQ IThread interface.

#include "delegate/IThread.h"
#include "cmsis_os2.h"
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

    osThreadId_t GetThreadId();
    static osThreadId_t GetCurrentThreadId();
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
    static const uint32_t MSGQ_SIZE = 20;    // Number of msgs
};

#endif // _THREAD_CMSIS_RTOS2_H