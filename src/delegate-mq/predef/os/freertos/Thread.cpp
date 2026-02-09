#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>

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
    m_queueSize = (maxQueueSize == 0) ? DEFAULT_QUEUE_SIZE : maxQueueSize;
    m_priority = configMAX_PRIORITIES > 2 ? configMAX_PRIORITIES - 2 : tskIDLE_PRIORITY + 1;
}

//----------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
    if (m_exitSem) {
        vSemaphoreDelete(m_exitSem);
        m_exitSem = nullptr;
    }
}

//----------------------------------------------------------------------------
// SetStackMem (Static Stack Configuration)
//----------------------------------------------------------------------------
void Thread::SetStackMem(StackType_t* stackBuffer, uint32_t stackSizeInWords)
{
    if (stackBuffer && stackSizeInWords > 0) {
        m_stackBuffer = stackBuffer;
        m_stackSize = stackSizeInWords;
    }
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
    //if (IsThreadCreated())
    //    return true;

    // 1. Create Synchronization Semaphore (Critical for cleanup)
    if (!m_exitSem) {
        m_exitSem = xSemaphoreCreateBinary();
        ASSERT_TRUE(m_exitSem != nullptr);
    }

    // 2. Create the Queue NOW (Synchronously)
    // We must do this BEFORE creating the task so it's ready for immediate use.
    if (!m_queue) {
        m_queue = xQueueCreate(m_queueSize, sizeof(ThreadMsg*));
        if (m_queue == nullptr) {
            printf("Error: Thread '%s' failed to create queue.\n", THREAD_NAME.c_str());
            return false;
        }
    }

    // 3. Create Task
    if (m_stackBuffer != nullptr)
    {
        // --- STATIC ALLOCATION ---
        m_thread = xTaskCreateStatic(
            (TaskFunction_t)&Thread::Process,
            THREAD_NAME.c_str(),
            m_stackSize,
            this,
            m_priority,
            m_stackBuffer,
            &m_tcb
        );
    }
    else
    {
        // --- DYNAMIC ALLOCATION (Heap) ---
        // Increase default stack to 1024 words (4KB) for safety
        const uint32_t DYNAMIC_STACK_SIZE = 1024; 
        
        BaseType_t xReturn = xTaskCreate(
            (TaskFunction_t)&Thread::Process,
            THREAD_NAME.c_str(),
            DYNAMIC_STACK_SIZE, 
            this,
            m_priority,
            &m_thread);

        if (xReturn != pdPASS) {
            printf("Error: Failed to create task '%s'. OOM?\n", THREAD_NAME.c_str());
            return false; 
        }
    }

    ASSERT_TRUE(m_thread != nullptr);
    return true;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_queue) {
        ThreadMsg* msg = new ThreadMsg(MSG_EXIT_THREAD);

        if (xQueueSend(m_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
            delete msg;
        }

        if (xTaskGetCurrentTaskHandle() != m_thread && m_exitSem) {
            xSemaphoreTake(m_exitSem, portMAX_DELAY);
        }

        if (m_queue) {
            vQueueDelete(m_queue);
            m_queue = nullptr;
        }
        m_thread = nullptr;
    }
}

//----------------------------------------------------------------------------
// Getters / Setters
//----------------------------------------------------------------------------
TaskHandle_t Thread::GetThreadId() { return m_thread; }
TaskHandle_t Thread::GetCurrentThreadId() { return xTaskGetCurrentTaskHandle(); }

void Thread::SetThreadPriority(int priority) {
    m_priority = priority;
    if (m_thread) {
        vTaskPrioritySet(m_thread, (UBaseType_t)m_priority);
    }
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    if (!m_queue) {
        printf("[Thread] Error: Dispatch called but queue is null (%s)\n", THREAD_NAME.c_str());
        return; 
    }

    // DEBUG: Print attempt
    //printf("[Thread] Dispatching to %s...\n", THREAD_NAME.c_str());

    // C++ 'new' uses System Heap (not FreeRTOS heap). 
    // If this fails, increase Heap_Size in linker script.
    ThreadMsg* threadMsg = new ThreadMsg(MSG_DISPATCH_DELEGATE, msg);
    
    if (threadMsg == nullptr) {
        printf("[Thread] CRITICAL: 'new ThreadMsg' returned NULL! System Heap full? (%s)\n", THREAD_NAME.c_str());
        return;
    }

    if (xQueueSend(m_queue, &threadMsg, pdMS_TO_TICKS(10)) != pdPASS) {
        printf("[Thread] Error: Queue Full (%s)\n", THREAD_NAME.c_str());
        delete threadMsg;
    } else {
        //printf("[Thread] Dispatch Success (%s)\n", THREAD_NAME.c_str());
    }
}

//----------------------------------------------------------------------------
// Process & Run
//----------------------------------------------------------------------------
void Thread::Process(void* instance)
{
    Thread* thread = static_cast<Thread*>(instance);
    ASSERT_TRUE(thread != nullptr);
    thread->Run();
    vTaskDelete(NULL);
}

void Thread::Run()
{
    ThreadMsg* msg = nullptr;
    while (true)
    {
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
                if (m_exitSem) {
                    xSemaphoreGive(m_exitSem);
                }
                return;
            }
            default: break;
            }
            delete msg;
        }
    }
}
