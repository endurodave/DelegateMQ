#include "Timer.h"
#include "Fault.h"
#include <chrono>
#include <algorithm>

using namespace std;
using namespace dmq;

bool Timer::m_timerStopped = false;

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
    const std::lock_guard<std::recursive_mutex> lock(GetLock());
    m_enabled = false;
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Timer::~Timer()
{
    try {
        const std::lock_guard<std::recursive_mutex> lock(GetLock());
        auto& timers = GetTimers();

        if (timers.size() != 0) {
            // Safely check before removing
            auto it = std::find(timers.begin(), timers.end(), this);
            if (it != timers.end()) {
                timers.erase(it);
            }
        }
    }
    catch (...) {
        // Failsafe during static destruction
    }
}

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------
void Timer::Start(dmq::Duration timeout, bool once)
{
    if (timeout <= dmq::Duration(0))
        throw std::invalid_argument("Timeout cannot be 0");

    const std::lock_guard<std::recursive_mutex> lock(GetLock());

    m_timeout = timeout;
    m_once = once;
    m_expireTime = GetTime();
    m_enabled = true;

    // If 'this' is the 'next' item in the ProcessTimers loop, removing it 
    // invalidates the iterator and causes a crash.
    auto& timers = GetTimers();
    bool found = (std::find(timers.begin(), timers.end(), this) != timers.end());

    // Only add if not already in the list. 
    // If it IS in the list (even if disabled/stopped), we just updated 
    // its state above, so it is now active again.
    if (!found) 
        timers.push_back(this);    

    LOG_INFO("Timer::Start timeout={}", m_timeout.count());
}

//------------------------------------------------------------------------------
// Stop
//------------------------------------------------------------------------------
void Timer::Stop()
{
    const std::lock_guard<std::recursive_mutex> lock(GetLock());

    m_enabled = false;

    // Don't remove immediately! Just set a flag.
    // Let ProcessTimers() handle the actual removal safely using remove_if
    m_timerStopped = true;

    LOG_INFO("Timer::Stop timeout={}", m_timeout.count());
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

    if (m_once)
    {
        m_enabled = false;
        m_timerStopped = true;
    }
    else
    {
        // Increment the timer to the next expiration
        m_expireTime += m_timeout;

        // Is the timer already expired after we incremented above?
        if (Difference(m_expireTime, GetTime()) > m_timeout)
        {
            // The timer has fallen behind so set time expiration further forward.
            m_expireTime = GetTime();

            // Timer processing is falling behind. Maybe user timer expiration is too 
            // short, time processing takings too long, or CheckExpired not called 
            // frequently enough. 
            LOG_INFO("Timer::CheckExpired Timer Processing Falling Behind");
        }
    }

    // Call the client's expired callback function
    Expired();
}

//------------------------------------------------------------------------------
// Difference
//------------------------------------------------------------------------------
dmq::Duration Timer::Difference(dmq::Duration time1, dmq::Duration time2)
{
    return (time2 - time1);
}

//------------------------------------------------------------------------------
// ProcessTimers
//------------------------------------------------------------------------------
void Timer::ProcessTimers()
{
    const std::lock_guard<std::recursive_mutex> lock(GetLock());

    // Remove disabled timer from the list if stopped
    if (m_timerStopped)
    {
        GetTimers().remove_if(TimerDisabled);
        m_timerStopped = false;
    }

    // Iterate safely handling potential deletion during callback
    auto it = GetTimers().begin();
    while (it != GetTimers().end())
    {
        Timer* t = *it;

        // INCREMENT NOW: Move 'it' to the next element BEFORE calling the function
        // that might delete the current element 't'.
        it++;

        // Now call the function. If 't' destroys itself, it is removed from the list,
        // but our local 'it' variable is already safe at the next node.
        if (t != nullptr)
            t->CheckExpired();
    }
}

//------------------------------------------------------------------------------
// GetTime
//------------------------------------------------------------------------------
dmq::Duration Timer::GetTime()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto duration = std::chrono::duration_cast<dmq::Duration>(now);
    return duration;
}

