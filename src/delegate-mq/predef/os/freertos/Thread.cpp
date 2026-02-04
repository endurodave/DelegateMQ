#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>

// Use configASSERT for embedded checking
#ifndef ASSERT_TRUE
#define ASSERT_TRUE(x) configASSERT(x)
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
}

//----------------------------------------------------------------------------
// Thread Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
    // Safe to delete semaphore now that the thread is definitely gone
    if (m_exitSem) {
        vSemaphoreDelete(m_exitSem);
        m_exitSem = nullptr;
    }
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    if (!m_thread)
    {
        // 1. Create Exit Synchronization Semaphore
        m_exitSem = xSemaphoreCreateBinary();
        ASSERT_TRUE(m_exitSem != nullptr);

        // 2. Create the Queue
        // Holds pointers to ThreadMsg objects (heap allocated)
        // Uses m_queueSize determined in constructor
        m_queue = xQueueCreate(m_queueSize, sizeof(ThreadMsg*));
        ASSERT_TRUE(m_queue != nullptr);

        // 3. Create the Task
        // Note: Stack size is in words (e.g. 4 bytes), not bytes.
        BaseType_t xReturn = xTaskCreate(
            (TaskFunction_t)&Thread::Process,
            THREAD_NAME.c_str(),
            configMINIMAL_STACK_SIZE * 4, // Ensure enough stack for delegates
            this,
            configMAX_PRIORITIES - 2, // Normal priority
            &m_thread);

        ASSERT_TRUE(xReturn == pdPASS);
    }
    return true;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_queue) {
        // Send exit message
        ThreadMsg* msg = new ThreadMsg(MSG_EXIT_THREAD);

        // Wait 100ms to send.
        // Note: If queue is full, we might fail to exit cleanly if we don't force it.
        if (xQueueSend(m_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
            delete msg; // Failed to send, clean up
        }

        // Wait for the thread to actually finish to avoid Use-After-Free.
        // We only wait if we are NOT the thread itself (prevent deadlock).
        if (xTaskGetCurrentTaskHandle() != m_thread && m_exitSem) {
            xSemaphoreTake(m_exitSem, portMAX_DELAY);
        }

        // Now safe to clean up resources
        // Delete Queue
        if (m_queue) {
            vQueueDelete(m_queue);
            m_queue = nullptr;
        }

        // Note: m_thread handle is invalid after the task deletes itself
        m_thread = nullptr;
    }
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
TaskHandle_t Thread::GetThreadId()
{
    return m_thread;
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
TaskHandle_t Thread::GetCurrentThreadId()
{
    return xTaskGetCurrentTaskHandle();
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    ASSERT_TRUE(m_queue != nullptr);

    // 1. Allocate message container on heap
    ThreadMsg* threadMsg = new ThreadMsg(MSG_DISPATCH_DELEGATE, msg);

    // 2. Send pointer to queue
    // Use a finite block time (e.g., 10ms) so we don't lock up the system if full
    if (xQueueSend(m_queue, &threadMsg, pdMS_TO_TICKS(10)) != pdPASS)
    {
        // 3. Handle failure: Delete to prevent memory leak
        delete threadMsg;
        // printf("Error: Thread '%s' queue full! Delegate dropped.\n", THREAD_NAME.c_str());
    }
}

//----------------------------------------------------------------------------
// Process (Static Entry Point)
//----------------------------------------------------------------------------
void Thread::Process(void* instance)
{
    Thread* thread = static_cast<Thread*>(instance);
    ASSERT_TRUE(thread != nullptr);

    thread->Run();

    // Self-delete when Run() returns (ExitThread called)
    vTaskDelete(NULL);
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
        if (xQueueReceive(m_queue, &msg, portMAX_DELAY) == pdPASS)
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
                    xSemaphoreGive(m_exitSem);
                }
                return; // Breaks loop, Process() calls vTaskDelete
            }

            default:
                break;
            }

            // Important: Delete the message container we 'new'ed in DispatchDelegate
            delete msg;
        }
    }
}
