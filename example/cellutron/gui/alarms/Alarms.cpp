#include "Alarms.h"
#include "Constants.h"
#include <iostream>

using namespace dmq;
using namespace cellutron;

static Timer* g_alarmGraceTimer = nullptr;
static dmq::ScopedConnection g_alarmGraceConn;

void Alarms::Initialize() {
    m_thread.CreateThread(std::chrono::seconds(2));
    m_ticksWaited = 0;

    // 1-second timer to track startup grace period
    g_alarmGraceTimer = new Timer();
    g_alarmGraceConn = g_alarmGraceTimer->OnExpired.Connect(dmq::MakeDelegate([this]() {
        m_ticksWaited++;
    }, m_thread));
    g_alarmGraceTimer->Start(std::chrono::milliseconds(1000));

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

    // 2. Setup Watchdog for safety heartbeat
    m_safetyWatchdog = std::make_unique<dmq::DeadlineSubscription<HeartbeatMsg>>(
        topics::SAFETY_HEARTBEAT,
        std::chrono::seconds(2),
        [](const HeartbeatMsg&) {},
        [this]() {
            if (!m_alarmActive && m_ticksWaited >= 15) {
                SetAlarm("ALARM: Safety Node Heartbeat Lost", true);
            }
        },
        &m_thread
    );

    // 3. Setup Watchdog for controller heartbeat
    m_controllerWatchdog = std::make_unique<dmq::DeadlineSubscription<HeartbeatMsg>>(
        topics::CONTROLLER_HEARTBEAT,
        std::chrono::seconds(2),
        [](const HeartbeatMsg&) {},
        [this]() {
            if (!m_alarmActive && m_ticksWaited >= 15) {
                SetAlarm("ALARM: Controller Node Heartbeat Lost", true);
            }
        },
        &m_thread
    );
}

void Alarms::Shutdown() {
    if (g_alarmGraceTimer) g_alarmGraceTimer->Stop();
    m_safetyWatchdog.reset();
    m_controllerWatchdog.reset();
    m_faultConn.Disconnect();
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
