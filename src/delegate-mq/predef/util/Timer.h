#ifndef _TIMER_H
#define _TIMER_H

#include "../../delegate/UnicastDelegateSafe.h"
#include <mutex>
#include <list>

/// @brief A timer class provides periodic timer callbacks on the client's 
/// thread of control. Timer is thread safe.
class Timer 
{
public:
	/// Client's register with Expired to get timer callbacks
	dmq::UnicastDelegateSafe<void(void)> Expired;

	/// Constructor
	Timer(void);

	/// Destructor
	~Timer(void);

	/// Starts a timer for callbacks on the specified timeout interval.
	/// @param[in]	timeout - the timeout in milliseconds.
	void Start(std::chrono::milliseconds timeout);

	/// Stops a timer.
	void Stop();

	/// Gets the enabled state of a timer.
	/// @return		TRUE if the timer is enabled, FALSE otherwise.
	bool Enabled() { return m_enabled; }

	/// Get the current time in ticks. 
	/// @return The current time in ticks. 
    static std::chrono::milliseconds GetTime();

	/// Computes the time difference in ticks between two tick values taking into
	/// account rollover.
	/// @param[in] 	time1 - time stamp 1 in ticks.
	/// @param[in] 	time2 - time stamp 2 in ticks.
	/// @return		The time difference in ticks.
	static std::chrono::milliseconds Difference(std::chrono::milliseconds time1, std::chrono::milliseconds time2);

	/// Called on a periodic basic to service all timer instances. 
	static void ProcessTimers();

private:
	// Prevent inadvertent copying of this object
	Timer(const Timer&);
	Timer& operator=(const Timer&);

	/// Called to check for expired timers and callback registered clients.
	void CheckExpired();

	/// List of all system timers to be serviced.
	static xlist<Timer*> m_timers;
	typedef xlist<Timer*>::iterator TimersIterator;

	/// A lock to make this class thread safe.
	static std::mutex m_lock;

	std::chrono::milliseconds m_timeout = std::chrono::milliseconds(0);		
	std::chrono::milliseconds m_expireTime = std::chrono::milliseconds(0);
	bool m_enabled = false;
	static bool m_timerStopped;
};

#endif
