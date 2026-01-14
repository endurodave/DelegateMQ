#ifndef _TIMER_H
#define _TIMER_H

#include "../../delegate/SignalSafe.h"
#include <list>

/// @brief A thread-safe timer class that provides periodic or one-shot callbacks.
/// 
/// @details
/// The Timer class allows clients to register for callbacks (`OnExpired`) that are invoked 
/// when a specified timeout interval elapses. 
/// 
/// **Key Features:**
/// * **Thread Safe:** All public API methods (`Start`, `Stop`, etc.) are thread-safe and can be called 
///   from any thread.
/// * **Flexible Modes:** Supports both one-shot (`once = true`) and periodic (`once = false`) operation.
/// * **Deterministic Execution:** Callbacks are invoked on the thread that calls `ProcessTimers()`. 
///   This allows the user to control exactly which thread executes the timer logic (e.g., Main Thread, 
///   GUI Thread, or a dedicated Worker Thread).
/// 
/// **Usage Note:**
/// This class uses a cooperative polling model. The static method `Timer::ProcessTimers()` MUST be 
/// called periodically (e.g., inside a main loop or a dedicated thread loop) to service active timers 
/// and dispatch callbacks.
/// 
/// @see SafeTimer.cpp for examples on how to handle callbacks safely with object lifetimes.
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
    static dmq::RecursiveMutex& GetLock()
    {
        // Allocate on heap and NEVER delete. Prevents lock from being destroyed 
        // before the last Timer destructor runs at app shutdown.
        static dmq::RecursiveMutex* lock = new dmq::RecursiveMutex();
        return *lock;
    }

    dmq::Duration m_timeout = dmq::Duration(0);		
    dmq::TimePoint m_expireTime;
    bool m_enabled = false;
    bool m_once = false;
    static bool m_timerStopped;
};

#endif
