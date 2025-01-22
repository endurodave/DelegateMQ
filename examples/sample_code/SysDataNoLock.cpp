#include "SysDataNoLock.h"
#include "WorkerThreadStd.h"

WorkerThread workerThread2("WorkerThread2");

//----------------------------------------------------------------------------
// GetInstance
//----------------------------------------------------------------------------
SysDataNoLock& SysDataNoLock::GetInstance()
{
	static SysDataNoLock instance;
	return instance;
}

//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
SysDataNoLock::SysDataNoLock() :
	m_systemMode(SystemMode::STARTING)
{
	SetSystemModeDelegate += MakeDelegate(this, &SysDataNoLock::SetSystemModePrivate, workerThread2);
	workerThread2.CreateThread();
}

//----------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------
SysDataNoLock::~SysDataNoLock()
{
	SetSystemModeDelegate -= MakeDelegate(this, &SysDataNoLock::SetSystemModePrivate, workerThread2);
	workerThread2.ExitThread();
}

//----------------------------------------------------------------------------
// SetSystemMode
//----------------------------------------------------------------------------
void SysDataNoLock::SetSystemMode(SystemMode::Type systemMode)
{
	// Invoke the private callback. SetSystemModePrivate() will be called on workerThread2.
	SetSystemModeDelegate(systemMode);
}

//----------------------------------------------------------------------------
// SetSystemModeAsyncAPI
//----------------------------------------------------------------------------
void SysDataNoLock::SetSystemModeAsyncAPI(SystemMode::Type systemMode)
{
	// Is the caller executing on workerThread2?
	if (workerThread2.GetThreadId() != WorkerThread::GetCurrentThreadId())
	{
		// Create an asynchronous delegate and reinvoke the function call on workerThread2
		MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncAPI, workerThread2).AsyncInvoke(systemMode);
		return;
	}

	// Create the callback data
	SystemModeChanged callbackData;
	callbackData.PreviousSystemMode = m_systemMode;
	callbackData.CurrentSystemMode = systemMode;

	// Update the system mode
	m_systemMode = systemMode;

	// Callback all registered subscribers
	SystemModeChangedDelegate(callbackData);
}

//----------------------------------------------------------------------------
// SetSystemModeAsyncWaitAPI
//----------------------------------------------------------------------------
SystemMode::Type SysDataNoLock::SetSystemModeAsyncWaitAPI(SystemMode::Type systemMode)
{
	// Is the caller executing on workerThread2?
	if (workerThread2.GetThreadId() != WorkerThread::GetCurrentThreadId())
	{
		// Create an asynchronous delegate and reinvoke the function call on workerThread2
		auto delegate =
			MakeDelegate(this, &SysDataNoLock::SetSystemModeAsyncWaitAPI, workerThread2, WAIT_INFINITE);
		return delegate(systemMode);
	}

	// Create the callback data
	SystemModeChanged callbackData;
	callbackData.PreviousSystemMode = m_systemMode;
	callbackData.CurrentSystemMode = systemMode;

	// Update the system mode
	m_systemMode = systemMode;

	// Callback all registered subscribers
	SystemModeChangedDelegate(callbackData);

	return callbackData.PreviousSystemMode;
}

//----------------------------------------------------------------------------
// SetSystemModePrivate
//----------------------------------------------------------------------------
void SysDataNoLock::SetSystemModePrivate(SystemMode::Type systemMode)
{
	// Create the callback data
	SystemModeChanged callbackData;
	callbackData.PreviousSystemMode = m_systemMode;
	callbackData.CurrentSystemMode = systemMode;

	// Update the system mode
	m_systemMode = systemMode;

	// Callback all registered subscribers
	SystemModeChangedDelegate(callbackData);
}

