#ifndef _CENTRIFUGE_PROCESS_H
#define _CENTRIFUGE_PROCESS_H

#include "DelegateMQ.h"
#include "state-machine/StateMachine.h"
#include "extras/util/Timer.h"

/// @brief Process-layer centrifuge state machine. Simulates centrifuge physics
/// (ramp up, hold at speed, ramp down) and fires OnTargetReached when done.
/// Distinct from hw::Centrifuge in actuators/, which is the hardware abstraction.
class CentrifugeProcess : public StateMachine
{
public:
    CentrifugeProcess();
    ~CentrifugeProcess();

    virtual void SetThread(dmq::IThread& thread) override;

    /// Command a ramp to target speed.
    void StartRamp(uint16_t targetRpm, dmq::Duration duration);

    /// Command an immediate stop (ramp to 0).
    void StopRamp();

    /// Signal fired when the target RPM is reached.
    dmq::Signal<void(uint16_t rpm)> OnTargetReached;

    struct RampData : public EventData {
        uint16_t targetRpm;
        dmq::Duration duration;
    };

protected:
    enum States {
        ST_IDLE, ST_START_RAMP, ST_RAMPING, ST_AT_SPEED,
        ST_START_STOP, ST_STOPPING, ST_MAX_STATES
    };

    STATE_DECLARE(CentrifugeProcess, Idle,           NoEventData)
    STATE_DECLARE(CentrifugeProcess, StartRampState, RampData)
    STATE_DECLARE(CentrifugeProcess, Ramping,        NoEventData)
    STATE_DECLARE(CentrifugeProcess, AtSpeed,        NoEventData)
    STATE_DECLARE(CentrifugeProcess, StartStopState, RampData)
    STATE_DECLARE(CentrifugeProcess, Stopping,       NoEventData)

    BEGIN_STATE_MAP
        STATE_MAP_ENTRY(&Idle)
        STATE_MAP_ENTRY(&StartRampState)
        STATE_MAP_ENTRY(&Ramping)
        STATE_MAP_ENTRY(&AtSpeed)
        STATE_MAP_ENTRY(&StartStopState)
        STATE_MAP_ENTRY(&Stopping)
    END_STATE_MAP

private:
    void Poll();
    void StartPoll() {
        printf("CentrifugeProcess: StartPoll() -- before Start() OnExpired.Empty=%d\n", (int)m_pollTimer.OnExpired.Empty());
        m_pollTimer.Start(std::chrono::milliseconds(100));
        printf("CentrifugeProcess: StartPoll() -- after Start()\n");
    }
    void StopPoll() {
        printf("CentrifugeProcess: StopPoll() -- before Stop()\n");
        m_pollTimer.Stop();
        printf("CentrifugeProcess: StopPoll() -- after Stop()\n");
    }

    void EventAtSpeed();
    void EventIdle();

    uint16_t      m_currentRpm   = 0;
    uint16_t      m_startRpm     = 0;
    uint16_t      m_targetRpm    = 0;
    dmq::Duration m_rampDuration = std::chrono::milliseconds(0);
    dmq::TimePoint m_rampStartTime;

    Timer                 m_pollTimer;
    dmq::ScopedConnection m_pollTimerConn;
};

#endif // _CENTRIFUGE_PROCESS_H
