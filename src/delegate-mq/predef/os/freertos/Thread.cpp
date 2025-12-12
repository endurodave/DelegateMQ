#include "Thread.h"
#include "ThreadMsg.h"
#include "predef/util/Fault.h"

using namespace std;
using namespace dmq;

#define MSG_DISPATCH_DELEGATE	1
#define MSG_EXIT_THREAD			2

//----------------------------------------------------------------------------
// Thread
//----------------------------------------------------------------------------
Thread::Thread(const std::string& threadName) : THREAD_NAME(threadName)
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
		// Create a worker thread
		BaseType_t xReturn = xTaskCreate(
			(TaskFunction_t)&Thread::Process,
			THREAD_NAME.c_str(),
			2046,
			this,
			configMAX_PRIORITIES - 1,// | portPRIVILEGE_BIT,
			&m_thread);
		ASSERT_TRUE(xReturn == pdPASS);

		m_queue = xQueueCreate(30, sizeof(ThreadMsg*));
		ASSERT_TRUE(m_queue != nullptr);
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
void Thread::DispatchDelegate(std::shared_ptr<dmq::DelegateMsg> msg)
{
	if (m_queue == nullptr)
		throw std::invalid_argument("Queue pointer is null");

	// Create a new ThreadMsg
	ThreadMsg* threadMsg = new ThreadMsg(MSG_DISPATCH_DELEGATE, msg);
	if (xQueueSend(m_queue, &threadMsg, portMAX_DELAY) != pdPASS)
	{
		// Handle the case when the message was not successfully added to the queue
		delete threadMsg;  // Ensure we don't leak memory if the send fails
		throw std::runtime_error("Failed to send message to queue");
	}
}

//----------------------------------------------------------------------------
// Process
//----------------------------------------------------------------------------
void Thread::Process(void* instance)
{
	Thread* thread = (Thread*)(instance);
	ASSERT_TRUE(thread != nullptr);

	ThreadMsg* msg = nullptr;
	while (1)
	{
		if (xQueueReceive(thread->m_queue, &msg, portMAX_DELAY) == pdPASS)
		{
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

					delete msg;
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
}

