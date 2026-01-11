#ifndef _THREAD_FREERTOS_H
#define _THREAD_FREERTOS_H

#include "delegate/IThread.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string>
#include <memory>

class ThreadMsg;

class Thread : public dmq::IThread
{
public:
    /// Constructor
    Thread(const std::string& threadName);

    /// Destructor
    ~Thread();

    /// Called once to create the worker thread
    /// @return TRUE if thread is created. FALSE otherwise. 
    bool CreateThread();

    /// Terminate the thread gracefully
    void ExitThread();

    /// Get the ID of this thread instance
    TaskHandle_t GetThreadId();

    /// Get the ID of the currently executing thread
    static TaskHandle_t GetCurrentThreadId();

    /// Get thread name
    std::string GetThreadName() { return THREAD_NAME; }

    // IThread Interface Implementation
    virtual void DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg) override;

private:
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

    /// Entry point for the thread
    static void Process(void* instance);

    // Run loop called by Process
    void Run();

    TaskHandle_t m_thread = nullptr;
    QueueHandle_t m_queue = nullptr;
    const std::string THREAD_NAME;
};

#endif