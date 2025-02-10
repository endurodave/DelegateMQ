#ifndef _THREAD_STD_H
#define _THREAD_STD_H

// @see https://github.com/endurodave/StdWorkerThread
// David Lafreniere, Feb 2017.

#include "delegate/IThread.h"
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <future>

class ThreadMsg;

class Thread : public DelegateMQ::IThread
{
public:
	/// Constructor
	Thread(const std::string& threadName);

	/// Destructor
	~Thread();

	/// Called once to create the worker thread
	/// @return TRUE if thread is created. FALSE otherise. 
	bool CreateThread();

	/// Called once a program exit to exit the worker thread
	void ExitThread();

	/// Get the ID of this thread instance
	std::thread::id GetThreadId();

	/// Get the ID of the currently executing thread
	static std::thread::id GetCurrentThreadId();

	/// Get thread name
	std::string GetThreadName() { return THREAD_NAME; }

	/// Get size of thread message queue.
	size_t GetQueueSize();

	virtual void DispatchDelegate(std::shared_ptr<DelegateMQ::DelegateMsg> msg);

private:
	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	/// Entry point for the thread
	void Process();

	void SetThreadName(std::thread::native_handle_type handle, const std::string& name);

	std::unique_ptr<std::thread> m_thread;
	std::queue<std::shared_ptr<ThreadMsg>> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	const std::string THREAD_NAME;

	// Promise and future to synchronize thread start
	std::promise<void> m_threadStartPromise;
	std::future<void> m_threadStartFuture;
};

#endif 

