#include "Timer.h"
#include "Fault.h"
#include <chrono>

using namespace std;
using namespace DelegateMQ;

std::mutex Timer::m_lock;
bool Timer::m_timerStopped = false;
xlist<Timer*> Timer::m_timers;

//------------------------------------------------------------------------------
// TimerDisabled
//------------------------------------------------------------------------------
static bool TimerDisabled (Timer* value)
{
	return !(value->Enabled());
}

//------------------------------------------------------------------------------
// Constructor
//------------------------------------------------------------------------------
Timer::Timer() 
{
	const std::lock_guard<std::mutex> lock(m_lock);
	m_enabled = false;
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Timer::~Timer()
{
	const std::lock_guard<std::mutex> lock(m_lock);
	m_timers.remove(this);
}

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------
void Timer::Start(std::chrono::milliseconds timeout)
{
	if (timeout <= std::chrono::milliseconds(0))
		throw std::invalid_argument("Timeout cannot be 0");

	const std::lock_guard<std::mutex> lock(m_lock);

	m_timeout = timeout;
	m_expireTime = GetTime();
	m_enabled = true;

	// Remove the existing entry, if any, to prevent duplicates in the list
	m_timers.remove(this);

	// Add this timer to the list for servicing
	m_timers.push_back(this);
}

//------------------------------------------------------------------------------
// Stop
//------------------------------------------------------------------------------
void Timer::Stop()
{
	const std::lock_guard<std::mutex> lock(m_lock);

	m_enabled = false;
	m_timerStopped = true;
}

//------------------------------------------------------------------------------
// CheckExpired
//------------------------------------------------------------------------------
void Timer::CheckExpired()
{
	if (!m_enabled)
		return;

	// Has the timer expired?
    if (Difference(m_expireTime, GetTime()) < m_timeout)
        return;

    // Increment the timer to the next expiration
	m_expireTime += m_timeout;

	// Is the timer already expired after we incremented above?
    if (Difference(m_expireTime, GetTime()) > m_timeout)
	{
		// The timer has fallen behind so set time expiration further forward.
		m_expireTime = GetTime();
	}

	// Call the client's expired callback function
	if (Expired)
		Expired();
}

//------------------------------------------------------------------------------
// Difference
//------------------------------------------------------------------------------
std::chrono::milliseconds Timer::Difference(std::chrono::milliseconds time1, std::chrono::milliseconds time2)
{
	return (time2 - time1);
}

//------------------------------------------------------------------------------
// ProcessTimers
//------------------------------------------------------------------------------
void Timer::ProcessTimers()
{
	const std::lock_guard<std::mutex> lock(m_lock);

	// Remove disabled timer from the list if stopped
	if (m_timerStopped)
	{
		m_timers.remove_if(TimerDisabled);
		m_timerStopped = false;
	}

	// Iterate through each timer and check for expirations
	TimersIterator it;
	for (it = m_timers.begin() ; it != m_timers.end(); it++ )
	{
		if ((*it) != NULL)
			(*it)->CheckExpired();
	}
}

std::chrono::milliseconds Timer::GetTime()
{
	auto duration = std::chrono::system_clock::now().time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	return millis;
}

