#ifndef _THREAD_STD_H
#define _THREAD_STD_H

// @see https://github.com/endurodave/StdWorkerThread
// David Lafreniere, Feb 2017.

#include "DelegateOpt.h"
#include "IThread.h"
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

class ThreadMsg;

class WorkerThread : public DelegateLib::IThread
{
public:
	/// Constructor
	WorkerThread(const std::string& threadName);

	/// Destructor
	~WorkerThread();

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

	virtual void DispatchDelegate(std::shared_ptr<DelegateLib::DelegateMsg> msg);

private:
	WorkerThread(const WorkerThread&) = delete;
	WorkerThread& operator=(const WorkerThread&) = delete;

	/// Entry point for the thread
	void Process();

    /// Entry point for timer thread
    void TimerThread();

	std::unique_ptr<std::thread> m_thread;
	std::queue<std::shared_ptr<ThreadMsg>> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cv;
    std::atomic<bool> m_timerExit;
	const std::string THREAD_NAME;
};

#endif 

