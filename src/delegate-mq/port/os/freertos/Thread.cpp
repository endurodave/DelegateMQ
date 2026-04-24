#ifndef DMQ_THREAD_FREERTOS
#error "port/os/freertos/Thread.cpp requires DMQ_THREAD_FREERTOS. Remove this file from your build configuration or define DMQ_THREAD_FREERTOS."
#endif

#include "DelegateMQ.h"
#include "Thread.h"
#include "ThreadMsg.h"
#include <cstdio>

#ifndef ASSERT_TRUE
#define ASSERT_TRUE(x) configASSERT(x)
#endif

namespace dmq::os {

using namespace dmq;
using namespace dmq::util;

//----------------------------------------------------------------------------
// Thread Constructor
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName, size_t maxQueueSize, FullPolicy fullPolicy)
    : THREAD_NAME(threadName)
    , FULL_POLICY(fullPolicy)
    , m_exit(false)
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
bool Thread::CreateThread(std::optional<dmq::Duration> watchdogTimeout)
{
    if (IsThreadCreated())
        return true;

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

    {
        std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
        m_lastAliveTime = Timer::GetNow();
    }

    if (watchdogTimeout.has_value())
    {
        {
            std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
            m_watchdogTimeout = watchdogTimeout.value();
        }

        m_threadTimer = std::unique_ptr<Timer>(new Timer());
        m_threadTimerConn = m_threadTimer->OnExpired.Connect(
            MakeDelegate(this, &Thread::ThreadCheck, *this));
        
        dmq::Duration timeout;
        {
            std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
            timeout = m_watchdogTimeout;
        }
        m_threadTimer->Start(timeout / 4);

        m_watchdogTimer = std::unique_ptr<Timer>(new Timer());
        m_watchdogTimerConn = m_watchdogTimer->OnExpired.Connect(
            MakeDelegate(this, &Thread::WatchdogCheck));
        m_watchdogTimer->Start(timeout / 2);
    }

    return true;
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_queue) {
        if (m_watchdogTimer)
        {
            m_watchdogTimer->Stop();
            m_watchdogTimerConn.Disconnect();
        }
        if (m_threadTimer)
        {
            m_threadTimer->Stop();
            m_threadTimerConn.Disconnect();
        }

        m_exit.store(true);

        ThreadMsg* msg = new (std::nothrow) ThreadMsg(MSG_EXIT_THREAD);

        if (msg) {
            // Block until message is sent. This ensures the thread WILL receive it.
            // If the thread is deadlocked, we hang here, which is better than 
            // hanging at the semaphore after dropping the message.
            if (xQueueSend(m_queue, &msg, portMAX_DELAY) != pdPASS) {
                delete msg;
            }
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
bool Thread::IsCurrentThread()
{
    return GetThreadId() == GetCurrentThreadId();
}

//----------------------------------------------------------------------------
// GetQueueSize
//----------------------------------------------------------------------------
size_t Thread::GetQueueSize()
{
    if (m_queue) {
        return (size_t)uxQueueMessagesWaiting(m_queue);
    }
    return 0;
}

void Thread::Sleep(dmq::Duration timeout) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
    vTaskDelay(pdMS_TO_TICKS(ms));
}

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
    ThreadMsg* threadMsg = new (std::nothrow) ThreadMsg(MSG_DISPATCH_DELEGATE, msg);

    if (threadMsg == nullptr) {
        printf("[Thread] CRITICAL: 'new ThreadMsg' returned NULL! System Heap full? (%s)\n", THREAD_NAME.c_str());
        return;
    }

    // DROP: non-blocking — discard if queue is full, caller is never stalled.
    // BLOCK: wait indefinitely — guarantees delivery, caller blocks until space is available.
    // FAULT: non-blocking - trigger fault if queue full.
    TickType_t timeout = (FULL_POLICY == FullPolicy::BLOCK) ? portMAX_DELAY : 0;

    // Option #2: Implement High priority using xQueueSendToFront. 
    // All other priorities use default FIFO (SendToBack).
    BaseType_t status;
    if (msg->GetPriority() == Priority::HIGH)
        status = xQueueSendToFront(m_queue, &threadMsg, timeout);
    else
        status = xQueueSendToBack(m_queue, &threadMsg, timeout);

    if (status != pdPASS) {
        if (FULL_POLICY == FullPolicy::FAULT) {
            printf("[Thread] CRITICAL: Queue full on thread '%s'! TRIGGERING FAULT.\n", THREAD_NAME.c_str());
            ASSERT_TRUE(status == pdPASS);
        }
        delete threadMsg;  // queue full, message dropped per DROP/FAULT policy
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

//----------------------------------------------------------------------------
// WatchdogCheck
//----------------------------------------------------------------------------
void Thread::WatchdogCheck()
{
    auto now = Timer::GetNow();
    dmq::TimePoint lastAlive;
    dmq::Duration watchdogTimeout;

    {
        std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
        lastAlive = m_lastAliveTime;
        watchdogTimeout = m_watchdogTimeout;
    }

    auto delta = now - lastAlive;
    if (delta > watchdogTimeout)
    {
        printf("[Thread] Watchdog detected unresponsive thread: %s\n", THREAD_NAME.c_str());
        // @TODO trigger recovery or fault handler
    }
}

//----------------------------------------------------------------------------
// ThreadCheck
//----------------------------------------------------------------------------
void Thread::ThreadCheck()
{
    std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
    m_lastAliveTime = Timer::GetNow();
}

void Thread::Run()
{
    ThreadMsg* msg = nullptr;
    while (!m_exit.load())
    {
        {
            std::lock_guard<dmq::RecursiveMutex> lock(m_watchdogMtx);
            m_lastAliveTime = Timer::GetNow();
        }

        if (xQueueReceive(m_queue, &msg, portMAX_DELAY) == pdPASS)
        {
            if (!msg) continue;

            int msgId = msg->GetId();
            if (msgId == MSG_DISPATCH_DELEGATE)
            {
                auto delegateMsg = msg->GetData();
                if (delegateMsg) {
                    auto invoker = delegateMsg->GetInvoker();
                    if (invoker) {
                        invoker->Invoke(delegateMsg);
                    }
                }
            }
            
            delete msg;

            if (msgId == MSG_EXIT_THREAD) {
                break;
            }
        }
    }

    if (m_exitSem) {
        xSemaphoreGive(m_exitSem);
    }
}

} // namespace dmq::os

