#include "Thread.h"
#include "predef/util/Fault.h"

#ifdef WIN32
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
bool Thread::CreateThread()
{
	if (!m_thread)
	{
		m_threadStartFuture = m_threadStartPromise.get_future();

		m_thread = std::unique_ptr<std::thread>(new thread(&Thread::Process, this));

		auto handle = m_thread->native_handle();
		SetThreadName(handle, THREAD_NAME);

		// Wait for the thread to enter the Process method
		m_threadStartFuture.get();
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
#ifdef WIN32
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

	// Create a new ThreadMsg
	std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_EXIT_THREAD, 0));

	// Put exit thread message into the queue
	{
		lock_guard<mutex> lock(m_mutex);
		m_queue.push(threadMsg);
		m_cv.notify_one();
	}

	m_exit.store(true);
    m_thread->join();

	// Clear the queue if anything added while waiting for join
	{
		lock_guard<mutex> lock(m_mutex);
		m_thread = nullptr;
		while (!m_queue.empty()) 
			m_queue.pop();
	}
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

	// Create a new ThreadMsg
    std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_DISPATCH_DELEGATE, msg));

	// Add dispatch delegate msg to queue and notify worker thread
	std::unique_lock<std::mutex> lk(m_mutex);
	m_queue.push(threadMsg);
	m_cv.notify_one();
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void Thread::Process()
{
	// Signal that the thread has started processing to notify CreateThread
	m_threadStartPromise.set_value();

	while (1)
	{
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
                return;
			}

			default:
				throw std::invalid_argument("Invalid message ID");
		}
	}
}

