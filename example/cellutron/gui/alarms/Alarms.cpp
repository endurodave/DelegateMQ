#include "Alarms.h"
#include "Constants.h"
#include "messages/RunStatusMsg.h"
#include <iostream>

using namespace dmq;

namespace cellutron {
namespace util {

void Alarms::Initialize() {
    m_thread.CreateThread(WATCHDOG_TIMEOUT);
    m_ticksWaited = 0;

    // 1-second timer to track startup grace period
    if (!m_alarmGraceTimer) {
        m_alarmGraceTimer = std::make_unique<Timer>();
    }
    m_alarmGraceConn = m_alarmGraceTimer->OnExpired.Connect(dmq::MakeDelegate([this]() {
        m_ticksWaited++;
    }, m_thread));
    m_alarmGraceTimer->Start(std::chrono::milliseconds(1000));

    // 1. Subscribe to system faults
    m_faultConn = DataBus::Subscribe<FaultMsg>(topics::FAULT, [this](FaultMsg msg) {
        // If we are already showing an alarm, don't overwrite it with a generic one
        if (m_alarmActive && msg.faultCode == FAULT_OVERSPEED) {
             // Let overspeed through as it is high priority
        } else if (m_alarmActive) {
            return;
        }

        std::string message;
        switch (msg.faultCode) {
            case FAULT_OVERSPEED:   message = "ALARM: Centrifuge Overspeed"; break;
            case FAULT_SAFETY_LOST: message = "ALARM: Safety Node Lost"; break;
            case FAULT_AIR_INLET:   message = "ALARM: Air Detected in Inlet"; break;
            case FAULT_BLOCKAGE:    message = "ALARM: Outlet Blockage Detected"; break;
            case FAULT_CONTROLLER_LOST: message = "ALARM: Controller Node Lost"; break;
            default:                message = "ALARM: Unknown System Fault (" + std::to_string(msg.faultCode) + ")"; break;
        }
        SetAlarm(message, true);
    }, &m_thread);

    // 2. Filtered subscription to run status. Only receive when the system is in FAULT.
    // This demonstrates the DataBus::SubscribeFilter feature.
    m_runStatusConn = DataBus::SubscribeFilter<RunStatusMsg>(
        topics::STATUS_RUN, 
        [this](RunStatusMsg) {
            SetAlarm("ALARM: System-wide Fault Detected", true);
        }, 
        [](const RunStatusMsg& msg) {
            return msg.status == RunStatus::FAULT;
        },
        &m_thread);

    // 3. Setup Watchdog for safety heartbeat
    m_safetyWatchdog = std::make_unique<dmq::DeadlineSubscription<HeartbeatMsg>>(
        topics::SAFETY_HEARTBEAT,
        HEARTBEAT_TIMEOUT,
        [](const HeartbeatMsg&) {},
        [this]() {
            if (!m_alarmActive && m_ticksWaited >= HEARTBEAT_WARMUP.count()) {
                SetAlarm("ALARM: Safety Node Heartbeat Lost", true);
            }
        },
        &m_thread
    );

    // 3. Setup Watchdog for controller heartbeat
    m_controllerWatchdog = std::make_unique<dmq::DeadlineSubscription<HeartbeatMsg>>(
        topics::CONTROLLER_HEARTBEAT,
        HEARTBEAT_TIMEOUT,
        [](const HeartbeatMsg&) {},
        [this]() {
            if (!m_alarmActive && m_ticksWaited >= HEARTBEAT_WARMUP.count()) {
                SetAlarm("ALARM: Controller Node Heartbeat Lost", true);
            }
        },
        &m_thread
    );
}

void Alarms::Shutdown() {
    if (m_alarmGraceTimer) m_alarmGraceTimer->Stop();
    m_safetyWatchdog.reset();
    m_controllerWatchdog.reset();
    m_faultConn.Disconnect();
    m_runStatusConn.Disconnect();
    m_thread.ExitThread();
}

void Alarms::Reset() {
    dmq::MakeDelegate(this, &Alarms::SetAlarm, m_thread).AsyncInvoke("No Alarm", false);
}

void Alarms::SetAlarm(const std::string& message, bool active) {
    m_currentMessage = message;
    m_alarmActive = active;
    
    // Emit signal to Notify UI (and anyone else)
    OnAlarmChanged(m_currentMessage, m_alarmActive);
}

} // namespace util
} // namespace cellutron
