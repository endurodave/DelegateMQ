#include "Timer.h"
#include "Fault.h"
#include <chrono>
#include <algorithm>

using namespace std;
using namespace dmq;

std::mutex Timer::m_lock;
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
    const std::lock_guard<std::mutex> lock(m_lock);
    m_enabled = false;
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
Timer::~Timer()
{
    try {
        const std::lock_guard<std::mutex> lock(m_lock);
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

    const std::lock_guard<std::mutex> lock(m_lock);

    m_timeout = timeout;
    m_once = once;
    m_expireTime = GetTime();
    m_enabled = true;

    // Remove the existing entry, if any, to prevent duplicates in the list
    GetTimers().remove(this);

    // Add this timer to the list for servicing
    GetTimers().push_back(this);

    LOG_INFO("Timer::Start timeout={}", m_timeout.count());
}

//------------------------------------------------------------------------------
// Stop
//------------------------------------------------------------------------------
void Timer::Stop()
{
    const std::lock_guard<std::mutex> lock(m_lock);

    m_enabled = false;
    m_timerStopped = true;

    GetTimers().remove(this);

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
    const std::lock_guard<std::mutex> lock(m_lock);

    // Remove disabled timer from the list if stopped
    if (m_timerStopped)
    {
        GetTimers().remove_if(TimerDisabled);
        m_timerStopped = false;
    }

    // Iterate through each timer and check for expirations
    TimersIterator it;
    for (it = GetTimers().begin() ; it != GetTimers().end(); it++ )
    {
        if ((*it) != NULL)
            (*it)->CheckExpired();
    }
}

dmq::Duration Timer::GetTime()
{
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto duration = std::chrono::duration_cast<dmq::Duration>(now);
    return duration;
}

