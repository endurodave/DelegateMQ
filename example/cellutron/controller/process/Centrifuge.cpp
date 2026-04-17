#include "Centrifuge.h"
#include "messages/CentrifugeSpeedMsg.h"
#include <algorithm>
#include <iostream>

using namespace dmq;

Centrifuge::Centrifuge() :
    StateMachine(ST_MAX_STATES)
{
}

Centrifuge::~Centrifuge()
{
    m_pollTimer.Stop();
}

void Centrifuge::SetThread(dmq::IThread& thread)
{
    StateMachine::SetThread(thread);
    m_pollTimerConn = m_pollTimer.OnExpired.Connect(dmq::MakeDelegate(this, &Centrifuge::Poll, thread));
}

void Centrifuge::StartRamp(uint16_t targetRpm, dmq::Duration duration)
{
    auto data = std::make_shared<RampData>();
    data->targetRpm = targetRpm;
    data->duration = duration;

    // Use shared_ptr<const EventData> as required by the SM framework
    ExternalEvent(ST_START_RAMP, data);
}

void Centrifuge::StopRamp()
{
    auto data = std::make_shared<RampData>();
    data->targetRpm = 0;
    data->duration = std::chrono::milliseconds(2000); 

    ExternalEvent(ST_START_STOP, data);
}

void Centrifuge::Poll()
{
    BEGIN_TRANSITION_MAP(Centrifuge, Poll)     // - Current State -
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)    // ST_IDLE
        TRANSITION_MAP_ENTRY(ST_RAMPING)       // ST_START_RAMP
        TRANSITION_MAP_ENTRY(ST_RAMPING)       // ST_RAMPING
        TRANSITION_MAP_ENTRY(EVENT_IGNORED)    // ST_AT_SPEED
        TRANSITION_MAP_ENTRY(ST_STOPPING)      // ST_START_STOP
        TRANSITION_MAP_ENTRY(ST_STOPPING)      // ST_STOPPING
    END_TRANSITION_MAP(nullptr)
}

STATE_DEFINE(Centrifuge, Idle, NoEventData)
{
    bool wasRamping = (m_currentRpm != 0);
    m_currentRpm = 0;
    StopPoll();
    DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { 0 });
    
    if (wasRamping) {
        OnTargetReached(0);
    }
}

STATE_DEFINE(Centrifuge, StartRampState, RampData)
{
    m_startRpm = m_currentRpm;
    m_targetRpm = data->targetRpm;
    m_rampDuration = data->duration;
    m_rampStartTime = Clock::now();

    if (m_rampDuration.count() > 0)
    {
        StartPoll();
    }
    else
    {
        m_currentRpm = m_targetRpm;
        InternalEvent(ST_AT_SPEED);
    }
}

STATE_DEFINE(Centrifuge, Ramping, NoEventData)
{
    auto now = Clock::now();
    auto elapsed = now - m_rampStartTime;

    if (elapsed >= m_rampDuration)
    {
        m_currentRpm = m_targetRpm;
        InternalEvent(ST_AT_SPEED);
    }
    else
    {
        float progress = static_cast<float>(elapsed.count()) / m_rampDuration.count();
        m_currentRpm = static_cast<uint16_t>(m_startRpm + (static_cast<int32_t>(m_targetRpm) - m_startRpm) * progress);
        DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    }
}

STATE_DEFINE(Centrifuge, AtSpeed, NoEventData)
{
    StopPoll();
    DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    OnTargetReached(m_currentRpm);
}

STATE_DEFINE(Centrifuge, StartStopState, RampData)
{
    m_startRpm = m_currentRpm;
    m_targetRpm = 0;
    m_rampDuration = data->duration;
    m_rampStartTime = Clock::now();
    StartPoll();
}

STATE_DEFINE(Centrifuge, Stopping, NoEventData)
{
    auto now = Clock::now();
    auto elapsed = now - m_rampStartTime;

    if (elapsed >= m_rampDuration)
    {
        InternalEvent(ST_IDLE);
    }
    else
    {
        float progress = static_cast<float>(elapsed.count()) / m_rampDuration.count();
        m_currentRpm = static_cast<uint16_t>(m_startRpm + (static_cast<int32_t>(m_targetRpm) - m_startRpm) * progress);
        DataBus::Publish<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", { m_currentRpm });
    }
}
