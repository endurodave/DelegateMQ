#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>
#include <new> 

// Define ASSERT_TRUE if not already defined
#ifndef ASSERT_TRUE
#define ASSERT_TRUE(x) if(!(x)) { while(1); }
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
    
    // Default Priority
    m_priority = osPriorityNormal;
}

//----------------------------------------------------------------------------
// Thread Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
    // Cleanup semaphore if it exists
    if (m_exitSem) {
        osSemaphoreDelete(m_exitSem);
        m_exitSem = NULL;
    }
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    if (m_thread == NULL)
    {
        // 1. Create Exit Semaphore (Max 1, Initial 0)
        // We use this to wait for the thread to shut down gracefully.
        m_exitSem = osSemaphoreNew(1, 0, NULL);
        ASSERT_TRUE(m_exitSem != NULL);

        // 2. Create Message Queue
        // We store pointers (ThreadMsg*), so msg_size = sizeof(ThreadMsg*)
        m_msgq = osMessageQueueNew(m_queueSize, sizeof(ThreadMsg*), NULL);
        ASSERT_TRUE(m_msgq != NULL);

        // 3. Create Thread
        osThreadAttr_t attr = {0};
        attr.name = THREAD_NAME.c_str();
        attr.stack_size = STACK_SIZE;
        attr.priority = m_priority;
        
        m_thread = osThreadNew(Thread::Process, this, &attr);
        ASSERT_TRUE(m_thread != NULL);
    }
    return true;
}

//----------------------------------------------------------------------------
// SetThreadPriority
//----------------------------------------------------------------------------
void Thread::SetThreadPriority(osPriority_t priority)
{
    m_priority = priority;

    // If the thread is already running, update it live
    if (m_thread != NULL) {
        osThreadSetPriority(m_thread, m_priority);
    }
}

//----------------------------------------------------------------------------
// GetThreadPriority
//----------------------------------------------------------------------------
osPriority_t Thread::GetThreadPriority()
{
    return m_priority;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_msgq != NULL) 
    {
        // Send exit message
        ThreadMsg* msg = new (std::nothrow) ThreadMsg(MSG_EXIT_THREAD);
        if (msg)
        {
            // Send pointer, wait 100 ticks max
            if (osMessageQueuePut(m_msgq, &msg, 0, 100) != osOK) 
            {
                delete msg; // Failed to send
            }
        }

        // Wait for thread to process the exit message and signal completion.
        // We only wait if we are NOT the thread itself (prevent deadlock).
        // If osThreadGetId() returns NULL or error, we skip wait.
        if (osThreadGetId() != m_thread && m_exitSem != NULL) {
            osSemaphoreAcquire(m_exitSem, osWaitForever);
        }

        // Thread has finished Run(). Now we can safely clean up resources.
        
        // Terminate ensures the thread is in INACTIVE state and resources are reclaimed.
        if (m_thread) {
             osThreadTerminate(m_thread);
             m_thread = NULL;
        }

        if (m_msgq) {
             osMessageQueueDelete(m_msgq);
             m_msgq = NULL;
        }
    }
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
osThreadId_t Thread::GetThreadId()
{
    return m_thread;
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
osThreadId_t Thread::GetCurrentThreadId()
{
    return osThreadGetId();
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    ASSERT_TRUE(m_msgq != NULL);

    // 1. Allocate message container
    ThreadMsg* threadMsg = new (std::nothrow) ThreadMsg(MSG_DISPATCH_DELEGATE, msg);
    if (!threadMsg) return;

    // 2. Send pointer to queue (Wait 10 ticks if full)
    // Priority 0 (default)
    if (osMessageQueuePut(m_msgq, &threadMsg, 0, 10) != osOK)
    {
        delete threadMsg;
        // Optional: printf("Error: Thread '%s' queue full!\n", THREAD_NAME.c_str());
    }
}

//----------------------------------------------------------------------------
// Process (Static Entry Point)
//----------------------------------------------------------------------------
void Thread::Process(void* argument)
{
    Thread* thread = static_cast<Thread*>(argument);
    if (thread)
    {
        thread->Run();
    }
    
    // Thread terminates automatically when function returns.
    osThreadExit();
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
        // msg is a pointer to ThreadMsg*. The queue holds the pointer.
        if (osMessageQueueGet(m_msgq, &msg, NULL, osWaitForever) == osOK)
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
                // Signal ExitThread() that we are done
                if (m_exitSem) {
                    osSemaphoreRelease(m_exitSem);
                }
                return; 
            }

            default:
                break;
            }

            delete msg;
        }
    }
}