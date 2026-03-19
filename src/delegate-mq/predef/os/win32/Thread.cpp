#include "DelegateMQ.h"
#include "Thread.h"
#include "predef/util/Fault.h"

#define MSG_DISPATCH_DELEGATE    1
#define MSG_EXIT_THREAD          2

//----------------------------------------------------------------------------
// Thread
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName, size_t maxQueueSize)
    : THREAD_NAME(threadName)
    , MAX_QUEUE_SIZE(maxQueueSize)
    , m_exit(false)
{
    InitializeCriticalSection(&m_cs);
    InitializeConditionVariable(&m_cvNotEmpty);
    InitializeConditionVariable(&m_cvNotFull);
}

//----------------------------------------------------------------------------
// ~Thread
//----------------------------------------------------------------------------
Thread::~Thread()
{
    ExitThread();
    DeleteCriticalSection(&m_cs);
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread(std::optional<dmq::Duration> watchdogTimeout)
{
    if (m_hThread == NULL)
    {
        m_exit = false;

        // Manual-reset event for startup synchronization
        m_hStartEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        m_hThread = ::CreateThread(NULL, 0, ThreadProc, this, 0, &m_threadId);
        if (m_hThread == NULL) return false;

        // Set the thread name so it shows in the Visual Studio Debug Location toolbar
        std::wstring wname(THREAD_NAME.begin(), THREAD_NAME.end());
        SetThreadDescription(m_hThread, wname.c_str());

        // Wait for the thread to enter the Process method
        WaitForSingleObject(m_hStartEvent, INFINITE);
        CloseHandle(m_hStartEvent);
        m_hStartEvent = NULL;

        m_lastAliveTime.store(Timer::GetNow());

        // Caller wants a watchdog timer?
        if (watchdogTimeout.has_value())
        {
            // Create watchdog timer
            m_watchdogTimeout = watchdogTimeout.value();

            // Timer to ensure the Thread instance runs periodically.
            m_threadTimer = std::unique_ptr<Timer>(new Timer());
            m_threadTimerConn = m_threadTimer->OnExpired.Connect(MakeDelegate(this, &Thread::ThreadCheck, *this));
            m_threadTimer->Start(m_watchdogTimeout.load() / 4);

            // Timer to check that this Thread instance runs.
            m_watchdogTimer = std::unique_ptr<Timer>(new Timer());
            m_watchdogTimerConn = m_watchdogTimer->OnExpired.Connect(MakeDelegate(this, &Thread::WatchdogCheck));
            m_watchdogTimer->Start(m_watchdogTimeout.load() / 2);
        }

        LOG_INFO("Thread::CreateThread {}", THREAD_NAME);
    }
    return true;
}

//----------------------------------------------------------------------------
// ThreadProc
//----------------------------------------------------------------------------
DWORD WINAPI Thread::ThreadProc(LPVOID lpParam)
{
    static_cast<Thread*>(lpParam)->Process();
    return 0;
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
    if (m_exit.load() || m_hThread == NULL) return;

    // If using XALLOCATOR explicit operator new required. See xallocator.h.
    std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_DISPATCH_DELEGATE, msg));

    EnterCriticalSection(&m_cs);

    // [BACK PRESSURE LOGIC]
    if (MAX_QUEUE_SIZE > 0)
    {
        // Wait while queue is full, BUT stop waiting if m_exit is true.
        while (m_queue.size() >= MAX_QUEUE_SIZE && !m_exit.load())
        {
            SleepConditionVariableCS(&m_cvNotFull, &m_cs, INFINITE);
        }
    }

    // If we woke up because of exit (or exit happened while waiting), abort
    if (!m_exit.load())
    {
        m_queue.push(threadMsg);
        WakeConditionVariable(&m_cvNotEmpty);
    }

    LeaveCriticalSection(&m_cs);

    LOG_INFO("Thread::DispatchDelegate\n   thread={}\n   target={}",
        THREAD_NAME,
        typeid(*threadMsg->GetData()->GetInvoker()).name());
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void Thread::Process()
{
    // Signal that the thread has started processing to notify CreateThread
    SetEvent(m_hStartEvent);

    LOG_INFO("Thread::Process Start {}", THREAD_NAME);

    while (true)
    {
        m_lastAliveTime.store(Timer::GetNow());
        std::shared_ptr<ThreadMsg> msg;

        EnterCriticalSection(&m_cs);

        // Wait for message to be added to the queue.
        while (m_queue.empty() && !m_exit.load())
        {
            SleepConditionVariableCS(&m_cvNotEmpty, &m_cs, INFINITE);
        }

        // If empty and exit is true, we should exit.
        if (m_queue.empty() && m_exit.load())
        {
            LeaveCriticalSection(&m_cs);
            break;
        }

        // Get highest priority message within queue
        msg = m_queue.top();
        m_queue.pop();

        // Unblock producers now that space is available
        if (MAX_QUEUE_SIZE > 0)
            WakeConditionVariable(&m_cvNotFull);

        LeaveCriticalSection(&m_cs);

        switch (msg->GetId())
        {
            case MSG_DISPATCH_DELEGATE:
            {
                // @TODO: Update error handling below if necessary

                auto delegateMsg = msg->GetData();
                ASSERT_TRUE(delegateMsg);

                auto invoker = delegateMsg->GetInvoker();
                ASSERT_TRUE(invoker);

                // Invoke the delegate destination target function
                bool success = invoker->Invoke(delegateMsg);
                ASSERT_TRUE(success);
                break;
            }

            case MSG_EXIT_THREAD:
            {
                LOG_INFO("Thread::Process Exit Thread {}", THREAD_NAME);
                return;
            }

            default:
            {
                LOG_INFO("Thread::Process Invalid Message {}", THREAD_NAME);
                ASSERT_TRUE(false);
                break;
            }
        }
    }
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
    if (m_hThread == NULL) return;

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

    EnterCriticalSection(&m_cs);

    // Set exit flag INSIDE lock before notifying.
    // This ensures that when a blocked producer wakes up, it sees m_exit == true immediately.
    m_exit.store(true);

    // Explicitly allow Exit message to bypass the MAX_QUEUE_SIZE limit.
    // We do not wait on m_cvNotFull here to prevent deadlock during shutdown.
    m_queue.push(std::make_shared<ThreadMsg>(MSG_EXIT_THREAD, nullptr));

    // Wake up consumers
    WakeConditionVariable(&m_cvNotEmpty);

    // Wake up blocked producers (DispatchDelegate)
    WakeAllConditionVariable(&m_cvNotFull);

    LeaveCriticalSection(&m_cs);

    // Prevent deadlock if ExitThread is called from within the thread itself
    if (::GetCurrentThreadId() != m_threadId)
    {
        WaitForSingleObject(m_hThread, INFINITE);
    }

    CloseHandle(m_hThread);
    m_hThread = NULL;

    LOG_INFO("Thread::ExitThread {}", THREAD_NAME);
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
DWORD Thread::GetThreadId() { return m_threadId; }

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
DWORD Thread::GetCurrentThreadId() { return ::GetCurrentThreadId(); }

//----------------------------------------------------------------------------
// IsCurrentThread
//----------------------------------------------------------------------------
bool Thread::IsCurrentThread() { return GetThreadId() == GetCurrentThreadId(); }

//----------------------------------------------------------------------------
// GetQueueSize
//----------------------------------------------------------------------------
size_t Thread::GetQueueSize()
{
    EnterCriticalSection(&m_cs);
    size_t size = m_queue.size();
    LeaveCriticalSection(&m_cs);
    return size;
}

//----------------------------------------------------------------------------
// WatchdogCheck
//----------------------------------------------------------------------------
void Thread::WatchdogCheck()
{
    auto now = Timer::GetNow();
    auto lastAlive = m_lastAliveTime.load();

    auto delta = now - lastAlive;

    // Watchdog expired?
    if (delta > m_watchdogTimeout.load())
    {
        LOG_ERROR("Watchdog detected unresponsive thread: {}", THREAD_NAME);

        // @TODO Optionally trigger recovery, restart, or further actions here
        // For example, throw or notify external system
    }
}

//----------------------------------------------------------------------------
// ThreadCheck
//----------------------------------------------------------------------------
void Thread::ThreadCheck()
{
}
