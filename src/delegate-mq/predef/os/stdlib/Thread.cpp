#include "DelegateMQ.h"
#include "Thread.h"
#include "predef/util/Fault.h"

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace std;
using namespace dmq;

#define MSG_DISPATCH_DELEGATE	1
#define MSG_EXIT_THREAD			2

//----------------------------------------------------------------------------
// Thread
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName) : m_thread(nullptr), m_exit(false), THREAD_NAME(threadName)
{
}

//----------------------------------------------------------------------------
// ~Thread
//----------------------------------------------------------------------------
Thread::~Thread()
{
	ExitThread();
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread(std::optional<dmq::Duration> watchdogTimeout)
{
	if (!m_thread)
	{
		m_threadStartPromise = std::promise<void>();
		m_threadStartFuture = m_threadStartPromise.get_future();
		m_exit = false;

		m_thread = std::unique_ptr<std::thread>(new thread(&Thread::Process, this));

		auto handle = m_thread->native_handle();
		SetThreadName(handle, THREAD_NAME);

		// Wait for the thread to enter the Process method
		m_threadStartFuture.get();

		m_lastAliveTime.store(Timer::GetNow());

		// Caller wants a watchdog timer?
		if (watchdogTimeout.has_value())
		{
			// Create watchdog timer
			m_watchdogTimeout = watchdogTimeout.value();

			// Timer to ensure the Thread instance runs periodically. ThreadCheck invoked
			// on this thread instance.
			m_threadTimer = std::make_unique<Timer>();
			m_threadTimer->Expired = MakeDelegate(this, &Thread::ThreadCheck, *this);
			m_threadTimer->Start(m_watchdogTimeout.load() / 4);

			// Timer to check that this Thread instance runs. WatchdogCheck invoked 
			// on Timer::ProcessTimers() thread.
			m_watchdogTimer = std::make_unique<Timer>();
			m_watchdogTimer->Expired = MakeDelegate(this, &Thread::WatchdogCheck);
			m_watchdogTimer->Start(m_watchdogTimeout.load() / 2);
		}

		LOG_INFO("Thread::CreateThread {}", THREAD_NAME);
	}
	return true;
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
std::thread::id Thread::GetThreadId()
{
	if (m_thread == nullptr)
		throw std::invalid_argument("Thread pointer is null");

	return m_thread->get_id();
}

//----------------------------------------------------------------------------
// GetCurrentThreadId
//----------------------------------------------------------------------------
std::thread::id Thread::GetCurrentThreadId()
{
	return this_thread::get_id();
}

//----------------------------------------------------------------------------
// GetQueueSize
//----------------------------------------------------------------------------
size_t Thread::GetQueueSize()
{
	lock_guard<mutex> lock(m_mutex);
	return m_queue.size();
}

//----------------------------------------------------------------------------
// SetThreadName
//----------------------------------------------------------------------------
void Thread::SetThreadName(std::thread::native_handle_type handle, const std::string& name)
{
#ifdef _WIN32
	// Set the thread name so it shows in the Visual Studio Debug Location toolbar
	std::wstring wstr(name.begin(), name.end());
	HRESULT hr = SetThreadDescription(handle, wstr.c_str());
	if (FAILED(hr))
	{
		// Handle error if needed
	}
#endif
}

//----------------------------------------------------------------------------
// ExitThread
//----------------------------------------------------------------------------
void Thread::ExitThread()
{
	if (!m_thread)
		return;

	if (m_watchdogTimer)
		m_watchdogTimer->Stop();
	if (m_threadTimer)
		m_threadTimer->Stop();

	// Create a new ThreadMsg
	std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_EXIT_THREAD, 0));

	// Put exit thread message into the queue
	{
		lock_guard<mutex> lock(m_mutex);
		m_queue.push(threadMsg);
		m_cv.notify_one();
	}

	m_exit.store(true);

	// Prevent deadlock if ExitThread is called from within the thread itself
	if (m_thread->joinable())
	{
		if (std::this_thread::get_id() != m_thread->get_id())
		{
			m_thread->join();
		}
		else
		{
			// We are killing ourselves. Detach so the thread object cleans up naturally.
			m_thread->detach();
		}
	}

	{
		lock_guard<mutex> lock(m_mutex);
		m_thread = nullptr;
		while (!m_queue.empty())
			m_queue.pop();
	}

	LOG_INFO("Thread::ExitThread {}", THREAD_NAME);
}

//----------------------------------------------------------------------------
// DispatchDelegate
//----------------------------------------------------------------------------
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
	if (m_exit.load())
		return;
	if (m_thread == nullptr)
		throw std::invalid_argument("Thread pointer is null");

	// If using XALLOCATOR explicit operator new required. See xallocator.h.
	std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_DISPATCH_DELEGATE, msg));

	// Add dispatch delegate msg to queue and notify worker thread
	std::unique_lock<std::mutex> lk(m_mutex);
	m_queue.push(threadMsg);
	m_cv.notify_one();

	LOG_INFO("Thread::DispatchDelegate\n   thread={}\n   target={}", 
		THREAD_NAME, 
		typeid(*threadMsg->GetData()->GetInvoker()).name());
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
	// Invoked by m_threadTimer on this thread context. Execution proves the 
	// thread is responsive. Actual m_lastAliveTime update occurs in the 
	// main Process() loop.
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void Thread::Process()
{
	// Signal that the thread has started processing to notify CreateThread
	m_threadStartPromise.set_value();

	LOG_INFO("Thread::Process Start {}", THREAD_NAME);

	while (1)
	{
		m_lastAliveTime.store(Timer::GetNow());

		std::shared_ptr<ThreadMsg> msg;
		{
			// Wait for a message to be added to the queue
			std::unique_lock<std::mutex> lk(m_mutex);
			while (m_queue.empty())
				m_cv.wait(lk);

			if (m_queue.empty())
				continue;

			// Get highest priority message within queue
			msg = m_queue.top();
			m_queue.pop();
		}

		switch (msg->GetId())
		{
			case MSG_DISPATCH_DELEGATE:
			{
				// @TODO: Update error handling below if necessary.
				
				// Get pointer to DelegateMsg data from queue msg data
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
				throw std::invalid_argument("Invalid message ID");
			}
		}
	}
}
