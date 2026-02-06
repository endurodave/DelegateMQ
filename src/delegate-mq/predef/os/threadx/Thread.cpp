#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>
#include <cstring> // for memset
#include <new> // for std::nothrow

// Define ASSERT_TRUE if not already defined
#ifndef ASSERT_TRUE
#define ASSERT_TRUE(x) if(!(x)) { while(1); } // Replace with your fault handler
#endif

using namespace dmq;

//----------------------------------------------------------------------------
// Thread Constructor
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName, size_t maxQueueSize) 
    : THREAD_NAME(threadName)
{
    // If 0 is passed, use the default size
    m_queueSize = (maxQueueSize == 0) ? DEFAULT_QUEUE_SIZE : maxQueueSize;

    // Zero out control blocks for safety
    memset(&m_thread, 0, sizeof(m_thread));
    memset(&m_queue, 0, sizeof(m_queue));
    
    // Initialize exit semaphore (Count 0)
    tx_semaphore_create(&m_exitSem, (CHAR*)"ExitSem", 0);

    // Default Priority
    m_priority = 10;
}

//----------------------------------------------------------------------------
// Thread Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();

    // Guard against deleting invalid semaphore
    if (m_exitSem.tx_semaphore_id != 0) {
        tx_semaphore_delete(&m_exitSem);
    }
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    // Check if thread is already created (tx_thread_id is non-zero if created)
    if (m_thread.tx_thread_id == 0)
    {
        // --- 1. Create Queue ---
        // ThreadX queues store "words" (ULONGs). 
        // We are passing a pointer (ThreadMsg*), so we need enough words to hold a pointer.
        
        // Round Up Logic (Ceiling division)
        // Ensures we allocate enough words even if pointer size isn't a perfect multiple of ULONG
        UINT msgSizeWords = (sizeof(ThreadMsg*) + sizeof(ULONG) - 1) / sizeof(ULONG);

        // Calculate total ULONGs needed for the queue buffer
        ULONG queueMemSizeWords = m_queueSize * msgSizeWords;
        m_queueMemory.reset(new (std::nothrow) ULONG[queueMemSizeWords]);
        ASSERT_TRUE(m_queueMemory != nullptr);

        UINT ret = tx_queue_create(&m_queue, 
                                   (CHAR*)THREAD_NAME.c_str(), 
                                   msgSizeWords, 
                                   m_queueMemory.get(), 
                                   queueMemSizeWords * sizeof(ULONG));
        ASSERT_TRUE(ret == TX_SUCCESS);

        // --- 2. Create Thread ---
        // Stack must be ULONG aligned.
        
        // Stack Size Rounding
        ULONG stackSizeWords = (STACK_SIZE + sizeof(ULONG) - 1) / sizeof(ULONG);
        
        m_stackMemory.reset(new (std::nothrow) ULONG[stackSizeWords]);
        ASSERT_TRUE(m_stackMemory != nullptr);

        ret = tx_thread_create(&m_thread,
                               (CHAR*)THREAD_NAME.c_str(),
                               &Thread::Process,
                               (ULONG)this, // Pass 'this' as entry input
                               m_stackMemory.get(),
                               stackSizeWords * sizeof(ULONG),
                               m_priority,     
                               m_priority,
                               TX_NO_TIME_SLICE,
                               TX_AUTO_START);
        
        ASSERT_TRUE(ret == TX_SUCCESS);
    }
    return true;
}

//----------------------------------------------------------------------------
// SetThreadPriority
//----------------------------------------------------------------------------
void Thread::SetThreadPriority(UINT priority)
{
    m_priority = priority;

    // If the thread is already running, update it live
    if (m_thread.tx_thread_id != 0) {
        UINT oldPriority;
        tx_thread_priority_change(&m_thread, m_priority, &oldPriority);
    }
}

//----------------------------------------------------------------------------
// GetThreadPriority
//----------------------------------------------------------------------------
UINT Thread::GetThreadPriority()
{
    return m_priority;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_queue.tx_queue_id != 0) 
    {
        // Send exit message
        ThreadMsg* msg = new (std::nothrow) ThreadMsg(MSG_EXIT_THREAD);
        if (msg)
        {
            // Timeout 100 ticks
            if (tx_queue_send(&m_queue, &msg, 100) != TX_SUCCESS) 
            {
                delete msg; // Failed to send, prevent leak
            }
        }

        // Wait for thread to terminate using semaphore logic.
        // We only wait if we are NOT the thread itself (avoid deadlock).
        // If tx_thread_identify() returns NULL (ISR context), we also shouldn't block.
        TX_THREAD* currentThread = tx_thread_identify();
        if (currentThread != &m_thread && currentThread != nullptr) {
            // Wait for Run() to signal completion
            tx_semaphore_get(&m_exitSem, TX_WAIT_FOREVER);
        }

        // Force terminate if still running (safety net)
        // tx_thread_terminate returns TX_SUCCESS if terminated or TX_THREAD_ERROR if already terminated
        tx_thread_terminate(&m_thread);
        tx_thread_delete(&m_thread);
        
        // Delete queue
        tx_queue_delete(&m_queue);

        // Clear control blocks so CreateThread could potentially be called again
        memset(&m_thread, 0, sizeof(m_thread));
        memset(&m_queue, 0, sizeof(m_queue));
    }
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
TX_THREAD* Thread::GetThreadId()
{
    return &m_thread;
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
TX_THREAD* Thread::GetCurrentThreadId()
{
    return tx_thread_identify();
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    // Safety check if queue is valid
    if (m_queue.tx_queue_id == 0) return;

    // 1. Allocate message container
    ThreadMsg* threadMsg = new (std::nothrow) ThreadMsg(MSG_DISPATCH_DELEGATE, msg);
    if (!threadMsg)
    {
        // OOM Handling
        return;
    }

    // 2. Send pointer to queue
    // Wait 10 ticks if full. 
    UINT ret = tx_queue_send(&m_queue, &threadMsg, 10);
    
    if (ret != TX_SUCCESS)
    {
        //