#ifndef _THREAD_THREADX_H
#define _THREAD_THREADX_H

/// @file Thread.h
/// @brief ThreadX implementation of the DelegateMQ IThread interface.

#include "delegate/IThread.h"
#include "tx_api.h"
#include <string>
#include <memory>
#include <vector>

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
    TX_THREAD* GetThreadId();

    /// Get the ID of the currently executing thread
    static TX_THREAD* GetCurrentThreadId();

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
    
    // Configurable stack size (bytes) and queue depth
    static const ULONG STACK_SIZE = 2048; 
    static const ULONG QUEUE_SIZE = 20;   
};

#endif // _THREAD_THREADX_H
