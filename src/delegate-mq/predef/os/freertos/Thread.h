#ifndef _THREAD_FREERTOS_H
#define _THREAD_FREERTOS_H

#include "delegate/IThread.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

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

	/// Get the ID of this thread instance
	TaskHandle_t GetThreadId();

	/// Get the ID of the currently executing thread
	static TaskHandle_t GetCurrentThreadId();

	/// Get thread name
	std::string GetThreadName() { return THREAD_NAME; }

	virtual void DispatchDelegate(std::shared_ptr<DelegateMQ::DelegateMsg> msg);

private:
	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;

	/// Entry point for the thread
	static void Process(void*);

	TaskHandle_t m_thread = nullptr;
	QueueHandle_t m_queue = nullptr;

	const std::string THREAD_NAME;
};

#endif 

