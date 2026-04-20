#include "CentrifugeProcess.h"
#include "messages/CentrifugeSpeedMsg.h"
#include <algorithm>

using namespace dmq;

CentrifugeProcess::CentrifugeProcess() : StateMachine(ST_MAX_STATES) {}

CentrifugeProcess::~CentrifugeProcess()
{
    m_pollTimer.Stop();
}

void CentrifugeProcess::SetThread(dmq::IThread& thread)
{
    StateMachine::SetThread(thread);
    m_pollTimerConn = m_pollTimer.OnExpired.Connect(
        dmq::MakeDelegate(this, &CentrifugeProcess::Poll, thread));
}

void CentrifugeProcess::StartRamp(uint16_t targetRpm, dmq::Duration duration)
{
    BEGIN_TRANSITION_MAP(CentrifugeProcess, StartRamp, targetRpm, duration)
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_RAMPING
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_START_STOP
        TRANSITION_MAP_ENTRY(ST_START_RAMP)    // ST_STOPPING
    END_TRANSITION_MAP(([&](){ 
        auto d = std::make_shared<RampData>(); 
        d->targetRpm = targetRpm; 
        d->duration = duration; 
        return d; 
    }()))
}

void CentrifugeProcess::StopRamp()
{
    BEGIN_TRANSITION_MAP(CentrifugeProcess, StopRamp)
        TRANSITION_MAP_ENTRY(ST_IDLE)          // ST_IDLE -> ST_IDLE (Fires confirmation signal)
        TRANSITION_MAP_ENTRY(ST_START_STOP)    // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_START_STOP)    // ST_RAMPING
        TRANSITION_MAP_ENTRY(ST_START_STOP)    // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(ST_START_STOP)    // ST_START_STOP
        TRANSITION_MAP_ENTRY(ST_START_STOP)    // ST_STOPPING
    END_TRANSITION_MAP(([&](){ 
        auto d = std::make_shared<RampData>(); 
        d->targetRpm = 0; 
        d->duration = std::chrono::milliseconds(2000); 
        return d; 
    }()))
}

void CentrifugeProcess::EventAtSpeed()
{
    BEGIN_TRANSITION_MAP(CentrifugeProcess, EventAtSpeed)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_AT_SPEED)   // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_AT_SPEED)   // ST_RAMPING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_START_STOP
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_STOPPING
    END_TRANSITION_MAP(nullptr)
}

void CentrifugeProcess::EventIdle()
{
    BEGIN_TRANSITION_MAP(CentrifugeProcess, EventIdle)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_RAMPING
        TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_START_STOP
        TRANSITION_MAP_ENTRY(ST_IDLE)       // ST_STOPPING
    END_TRANSITION_MAP(nullptr)
}

void CentrifugeProcess::Poll()
{
    BEGIN_TRANSITION_MAP(CentrifugeProcess, Poll)
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_RAMPING)    // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_RAMPING)    // ST_RAMPING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED) // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(ST_STOPPING)   // ST_START_STOP
        TRANSITION_MAP_ENTRY(ST_STOPPING)   // ST_STOPPING
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(CentrifugeProcess, Idle, NoEventData)
{
    printf("CentrifugeProcess: ST_IDLE\n");
    m_currentRpm = 0;
    StopPoll();
    DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { 0 });
    OnTargetReached(0);
}

STATE_DEFINE(CentrifugeProcess, StartRampState, RampData)
{
    m_startRpm      = m_currentRpm;
    m_targetRpm     = data->targetRpm;
    m_rampDuration  = data->duration;
    m_rampStartTime = Clock::now();
    printf("CentrifugeProcess: ST_START_RAMP target=%u dur=%lldms\n",
           m_targetRpm, (long long)m_rampDuration.count());

    if (m_rampDuration.count() > 0)
        StartPoll();
    else {
        m_currentRpm = m_targetRpm;
        EventAtSpeed();
    }
}

STATE_DEFINE(CentrifugeProcess, Ramping, NoEventData)
{
    auto elapsed = Clock::now() - m_rampStartTime;
    if (elapsed >= m_rampDuration) {
        m_currentRpm = m_targetRpm;
        EventAtSpeed();
    } else {
        float t = static_cast<float>(elapsed.count()) / m_rampDuration.count();
        m_currentRpm = static_cast<uint16_t>(
            m_startRpm + static_cast<int32_t>(m_targetRpm - m_startRpm) * t);
        DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    }
}

STATE_DEFINE(CentrifugeProcess, AtSpeed, NoEventData)
{
    printf("CentrifugeProcess: ST_AT_SPEED rpm=%u -- firing OnTargetReached\n", m_currentRpm);
    StopPoll();
    DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    OnTargetReached(m_currentRpm);
}

STATE_DEFINE(CentrifugeProcess, StartStopState, RampData)
{
    m_startRpm      = m_currentRpm;
    m_targetRpm     = 0;
    m_rampDuration  = data->duration;
    m_rampStartTime = Clock::now();
    StartPoll();
}

STATE_DEFINE(CentrifugeProcess, Stopping, NoEventData)
{
    auto elapsed = Clock::now() - m_rampStartTime;
    if (elapsed >= m_rampDuration) {
        EventIdle();
    } else {
        float t = static_cast<float>(elapsed.count()) / m_rampDuration.count();
        m_currentRpm = static_cast<uint16_t>(
            m_startRpm + static_cast<int32_t>(m_targetRpm - m_startRpm) * t);
        DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    }
}
