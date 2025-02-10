#include "Thread.h"
#include "ThreadMsg.h"
#include "predef/util/Timer.h"
#include "predef/util/Fault.h"

using namespace std;
using namespace DelegateMQ;

#define MSG_DISPATCH_DELEGATE	1
#define MSG_EXIT_THREAD			2
#define MSG_TIMER				3

//----------------------------------------------------------------------------
// Thread
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName) : m_timerExit(false), THREAD_NAME(threadName)
{
}

//----------------------------------------------------------------------------
// ~Thread
//----------------------------------------------------------------------------
Thread::~Thread()
{
}

//----------------------------------------------------------------------------
// CreateThread
//----------------------------------------------------------------------------
bool Thread::CreateThread()
{
	if (!m_thread)
	{
		// Create a default worker thread
		BaseType_t xReturn = xTaskCreate(
			(TaskFunction_t)&Thread::Process,
			THREAD_NAME.c_str(),
			4096,
			this,
			configMAX_PRIORITIES - 1,// | portPRIVILEGE_BIT,
			&m_thread);
		//ASSERT_TRUE(xReturn == pdPASS); TODO

		// Wait for the thread to enter the Process method
		//m_threadStartFuture.get(); TODO

		m_queue = xQueueCreate(30, sizeof(std::shared_ptr<ThreadMsg>));
	}
	return true;
}

//----------------------------------------------------------------------------
// GetThreadId
//----------------------------------------------------------------------------
TaskHandle_t Thread::GetThreadId()
{
	if (m_thread == nullptr)
		throw std::invalid_argument("Thread pointer is null");

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
void Thread::DispatchDelegate(std::shared_ptr<DelegateMQ::DelegateMsg> msg)
{
	if (m_queue == nullptr)
		throw std::invalid_argument("Queue pointer is null");

	// Create a new ThreadMsg
    std::shared_ptr<ThreadMsg> threadMsg(new ThreadMsg(MSG_DISPATCH_DELEGATE, msg));
	xQueueSend(m_queue, &threadMsg, portMAX_DELAY);
}

//----------------------------------------------------------------------------
// TimerThread
//----------------------------------------------------------------------------
void Thread::TimerThread()
{
	// TODO: use freertos timer
    while (!m_timerExit)
    {
		vTaskDelay(pdMS_TO_TICKS(100));

        std::shared_ptr<ThreadMsg> threadMsg (new ThreadMsg(MSG_TIMER, 0));
		xQueueSend(m_queue, &threadMsg, portMAX_DELAY);
    }
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void Thread::Process(void* instance)
{
	Thread* thread = (Thread*)(instance);
	// todo assert not null

	thread->m_timerExit = false;
    //std::thread timerThread(&Thread::TimerThread, this);

	std::shared_ptr<ThreadMsg> msg;

	while (1)
	{
		if (xQueueReceive(thread->m_queue, &msg, portMAX_DELAY) == pdPASS)
		{
			switch (msg->GetId())
			{
			case MSG_DISPATCH_DELEGATE:
			{
				// Get pointer to DelegateMsg data from queue msg data
				auto delegateMsg = msg->GetData();
				//ASSERT_TRUE(delegateMsg);

				auto invoker = delegateMsg->GetInvoker();
				//ASSERT_TRUE(invoker);

				// Invoke the delegate destination target function
				bool success = invoker->Invoke(delegateMsg);
				//ASSERT_TRUE(success);
				break;
			}

			case MSG_TIMER:
				//Timer::ProcessTimers(); TODO
				break;

			case MSG_EXIT_THREAD:
			{
				thread->m_timerExit = true;
				return;
			}

			default:
				throw std::invalid_argument("Invalid message ID");
			}
		}
	}
}

