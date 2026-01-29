#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>
#include <cstring> // for memset

// Define ASSERT_TRUE if not already defined
#ifndef ASSERT_TRUE
#define ASSERT_TRUE(x) __ASSERT(x, "DelegateMQ Assertion Failed")
#endif

using namespace dmq;

//----------------------------------------------------------------------------
// Thread Constructor
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName) 
    : THREAD_NAME(threadName)
{
    // Initialize objects to zero
    memset(&m_thread, 0, sizeof(m_thread));
    memset(&m_msgq, 0, sizeof(m_msgq));
    
    // Initialize exit semaphore (Initial count 0, Limit 1)
    k_sem_init(&m_exitSem, 0, 1);
}

//----------------------------------------------------------------------------
// Thread Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    // Check if thread is already created (dummy check on stack ptr)
    if (!m_stackMemory)
    {
        // 1. Create Message Queue
        // We use k_aligned_alloc to ensure buffer meets strict alignment requirements
        size_t qBufferSize = MSG_SIZE * MSGQ_MAX_MSGS;
        char* qBuf = (char*)k_aligned_alloc(sizeof(void*), qBufferSize);
        ASSERT_TRUE(qBuf != nullptr);
        
        m_msgqBuffer.reset(qBuf); // Ownership passed to unique_ptr

        k_msgq_init(&m_msgq, m_msgqBuffer.get(), MSG_SIZE, MSGQ_MAX_MSGS);

        // 2. Create Thread
        // CRITICAL: Stacks must be aligned to Z_KERNEL_STACK_OBJ_ALIGN for MPU/Arch reasons.
        // We use k_aligned_alloc instead of new char[].
        // K_THREAD_STACK_LEN calculates the correct size including guard pages/metadata.
        size_t stackBytes = K_THREAD_STACK_LEN(STACK_SIZE);
        char* stackBuf = (char*)k_aligned_alloc(Z_KERNEL_STACK_OBJ_ALIGN, stackBytes);
        ASSERT_TRUE(stackBuf != nullptr);

        m_stackMemory.reset(stackBuf); // Ownership passed to unique_ptr

        k_tid_t tid = k_thread_create(&m_thread,
                                      (k_thread_stack_t*)m_stackMemory.get(),
                                      STACK_SIZE,
                                      (k_thread_entry_t)Thread::Process,
                                      this, NULL, NULL,
                                      K_PRIO_PREEMPT(5), // Priority 5 (Adjust as needed)
                                      0, 
                                      K_NO_WAIT);
        
        ASSERT_TRUE(tid != nullptr);
        
        // Optional: Set thread name for debug
        k_thread_name_set(tid, THREAD_NAME.c_str());
    }
    return true;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_stackMemory) 
    {
        // Send exit message
        ThreadMsg* msg = new (std::nothrow) ThreadMsg(MSG_EXIT_THREAD);
        if (msg)
        {
            // Send pointer to queue. 100ms timeout
            if (k_msgq_put(&m_msgq, &msg, K_MSEC(100)) != 0) 
            {
                // If queue is full, we must delete msg to prevent leak, 
                // but this means the thread might not exit if it was the only attempt.
                delete msg; 
                // Log error here if logging available
            }
        }
        
        // Wait for thread to actually finish to avoid use-after-free of the stack.
        // We only wait if we are NOT the thread itself (avoid deadlock).
        if (k_current_get() != &m_thread) {
            k_sem_take(&m_exitSem, K_FOREVER);
        }

        // Now safe to let unique_ptr destructors run (freeing stack/msgq)
        
        // Note: k_thread_abort is not needed because the thread will 
        // return from Run() and terminate naturally.
    }
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
k_tid_t Thread::GetThreadId()
{
    return &m_thread;
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
k_tid_t Thread::GetCurrentThreadId()
{
    return k_current_get();
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    ASSERT_TRUE(m_stackMemory != nullptr);

    // 1. Allocate message container
    ThreadMsg* threadMsg = new (std::nothrow) ThreadMsg(MSG_DISPATCH_DELEGATE, msg);
    if (!threadMsg) return;

    // 2. Send pointer to queue (Wait 10ms if full)
    if (k_msgq_put(&m_msgq, &threadMsg, K_MSEC(10)) != 0)
    {
        delete threadMsg;
        // Optional: LOG_ERR("Thread '%s' queue full!", THREAD_NAME.c_str());
    }
}

//----------------------------------------------------------------------------
// Process (Static Entry Point)
//----------------------------------------------------------------------------
void Thread::Process(void* p1, void* p2, void* p3)
{
    Thread* thread = static_cast<Thread*>(p1);
    if (thread)
    {
        thread->Run();
    }
    // Returning from entry point automatically terminates the thread in Zephyr
}

//----------------------------------------------------------------------------
// Run (Member Function Loop)
//----------------------------------------------------------------------------
void Thread::Run()
{
    ThreadMsg* msg = nullptr;
    while (true)
    {
        // Block forever waiting for a message
        if (k_msgq_get(&m_msgq, &msg, K_FOREVER) == 0)
        {
            if (!msg) continue;

            switch (msg->GetId())
            {
            case MSG_DISPATCH_DELEGATE:
            {
                auto delegateMsg = msg->GetData();
                if (delegateMsg) {
                    auto invoker = delegateMsg->GetInvoker();
                    if (invoker) {
                        invoker->Invoke(delegateMsg);
                    }
                }
                break;
            }

            case MSG_EXIT_THREAD:
            {
                delete msg;
                // Signal that we are about to exit
                k_sem_give(&m_exitSem);
                return; 
            }

            default:
                break;
            }

            delete msg;
        }
    }
}