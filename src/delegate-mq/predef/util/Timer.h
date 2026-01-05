#ifndef _TIMER_H
#define _TIMER_H

#include "../../delegate/SignalSafe.h"
#include <mutex>
#include <list>

/// @brief A timer class provides periodic timer callbacks on the client's 
/// thread of control. Timer is thread safe.
/// See example SafeTimer.cpp to prevent a latent callback on a dead object.
class Timer 
{
public:
    /// Client's register with OnExpired to get timer callbacks
    dmq::SignalPtr<void(void)> OnExpired;

    /// Constructor
    Timer(void);

    /// Destructor
    ~Timer(void);

    /// Starts a timer for callbacks on the specified timeout interval.
    /// @param[in] timeout - the timeout.
    /// @param[in] once - true if only one timer expiration
    void Start(dmq::Duration timeout, bool once = false);

    /// Stops a timer.
    void Stop();

    /// Gets the enabled state of a timer.
    /// @return TRUE if the timer is enabled, FALSE otherwise.
    bool Enabled() { return m_enabled; }

    /// Get the time. 
    /// @return The time now. 
    static dmq::TimePoint GetNow();

    /// Called on a periodic basic to service all timer instances. 
    /// @TODO: Call periodically for timer expiration handling.
    static void ProcessTimers();

private:
    // Prevent inadvertent copying of this object
    Timer(const Timer&);
    Timer& operator=(const Timer&);

    /// Called to check for expired timers and callback registered clients.
    void CheckExpired();

    typedef xlist<Timer*>::iterator TimersIterator;

    /// Get list using the "Immortal" Pattern
    static xlist<Timer*>& GetTimers()
    {
        // Allocate on heap and NEVER delete. Prevents lock from being destroyed 
        // before the last Timer destructor runs at app shutdown.
        static xlist<Timer*>* instance = new xlist<Timer*>();
        return *instance;
    }

    /// Get lock using the "Immortal" Pattern
    static std::recursive_mutex& GetLock()
    {
        // Allocate on heap and NEVER delete. Prevents lock from being destroyed 
        // before the last Timer destructor runs at app shutdown.
        static std::recursive_mutex* lock = new std::recursive_mutex();
        return *lock;
    }

    dmq::Duration m_timeout = dmq::Duration(0);		
    dmq::TimePoint m_expireTime;
    bool m_enabled = false;
    bool m_once = false;
    static bool m_timerStopped;
};

#endif
