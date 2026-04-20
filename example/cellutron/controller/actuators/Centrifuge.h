#ifndef _HW_CENTRIFUGE_H
#define _HW_CENTRIFUGE_H

#include "DelegateMQ.h"
#include "messages/CentrifugeSpeedMsg.h"
#include "extras/util/Timer.h"
#include <cstdio>

namespace hw {

/// @brief Hardware centrifuge actuator with internal speed ramping.
///
/// SetSpeed() accepts a target RPM and ramp duration. An internal 50ms
/// poll timer (running on the ActuatorThread) interpolates the current
/// RPM along the ramp curve and publishes it to the DataBus each tick.
/// When the target is reached the OnTargetReached signal fires.
///
/// This class owns the physics — the process state machine simply
/// commands a speed and listens for OnTargetReached.
class Centrifuge
{
public:
    /// Signal fired when the commanded RPM is reached (or zero on stop).
    /// Connect from the process layer to drive state transitions.
    dmq::Signal<void(uint16_t rpm)> OnTargetReached;

    /// Connect the poll timer to the ActuatorsThread.
    /// Must be called after the thread is created.
    void SetThread(dmq::IThread& thread)
    {
        m_pollTimerConn = m_pollTimer.OnExpired.Connect(
            dmq::MakeDelegate(this, &Centrifuge::Poll, thread));
    }

    /// Command the centrifuge to ramp to targetRpm over rampDuration.
    /// Runs on the ActuatorThread (called via blocking delegate in Actuators).
    void SetSpeed(uint16_t targetRpm, dmq::Duration rampDuration)
    {
        m_startRpm     = m_currentRpm;
        m_targetRpm    = targetRpm;
        m_rampDuration = rampDuration;
        m_rampStart    = dmq::Clock::now();

        printf("[Centrifuge] Ramp: %u -> %u RPM over %lldms\n",
               m_startRpm, m_targetRpm,
               std::chrono::duration_cast<std::chrono::milliseconds>(rampDuration).count());

        m_pollTimer.Start(std::chrono::milliseconds(50));
    }

    /// Ramp down to zero over 2 seconds.
    /// Runs on the ActuatorThread.
    void Stop()
    {
        SetSpeed(0, std::chrono::milliseconds(2000));
    }

    uint16_t GetCurrentRpm() const { return m_currentRpm; }

private:
    /// Called every 50ms on the ActuatorThread.
    void Poll()
    {
        auto elapsed = dmq::Clock::now() - m_rampStart;

        if (elapsed >= m_rampDuration)
        {
            // Ramp complete
            m_currentRpm = m_targetRpm;
            m_pollTimer.Stop();
            PublishRpm();
            printf("[Centrifuge] Target reached: %u RPM\n", m_currentRpm);
            OnTargetReached(m_currentRpm);
        }
        else
        {
            // Interpolate along ramp curve
            float t = static_cast<float>(elapsed.count()) /
                      static_cast<float>(m_rampDuration.count());
            m_currentRpm = static_cast<uint16_t>(
                m_startRpm + static_cast<int32_t>(m_targetRpm - m_startRpm) * t);
            PublishRpm();
        }
    }

    void PublishRpm()
    {
        dmq::DataBus::Publish<CentrifugeSpeedMsg>(
            "cell/cmd/centrifuge_speed", { m_currentRpm });
    }

    uint16_t      m_currentRpm   = 0;
    uint16_t      m_startRpm     = 0;
    uint16_t      m_targetRpm    = 0;
    dmq::Duration m_rampDuration = std::chrono::milliseconds(0);
    dmq::TimePoint m_rampStart   = {};

    Timer                 m_pollTimer;
    dmq::ScopedConnection m_pollTimerConn;
};

} // namespace hw

#endif // _HW_CENTRIFUGE_H
