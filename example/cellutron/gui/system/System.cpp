#include "System.h"
#include "Network.h"
#include "ui/UI.h"
#include "logs/Logs.h"
#include "alarms/Alarms.h"
#include "RemoteConfig.h"
#include "Constants.h"
#include <cstdio>

using namespace dmq;

namespace cellutron {

System::~System() {
    Shutdown();
}

void System::Initialize() {
    printf("GUI: System initializing...\n");

    RegisterSerializers();
    RegisterStringifiers();
    
    m_thread.CreateThread(std::chrono::seconds(2));

    SetupNetwork();
    SetupWatchdog();
    
    Logs::GetInstance().Initialize();
    Alarms::GetInstance().Initialize();

    StartTimerThread();

    m_heartbeat.Start();

    printf("GUI: System ready.\n");
}

void System::Shutdown() {
    if (m_timerRunning) {
        m_timerRunning = false;
        if (m_backgroundTimer.joinable()) {
            m_backgroundTimer.join();
        }
        m_thread.ExitThread();

        Alarms::GetInstance().Shutdown();
        Logs::GetInstance().Shutdown();
        Network::GetInstance().Shutdown();
    }
}

void System::Tick() {
    m_heartbeat.Tick();
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5010, "GUI");

    // Incoming Topics
    Network::GetInstance().RegisterIncomingTopic<RunStatusMsg>(topics::STATUS_RUN, RID_RUN_STATUS, serRun);
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>(topics::CMD_CENTRIFUGE_SPEED, RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterIncomingTopic<ActuatorStatusMsg>(topics::STATUS_ACTUATOR, RID_ACTUATOR_STATUS, serActuator);
    Network::GetInstance().RegisterIncomingTopic<SensorStatusMsg>(topics::STATUS_SENSOR, RID_SENSOR_STATUS, serSensor);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, RID_CONTROLLER_HB, serHeartbeat);

    // Outgoing Topics
    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().AddRemoteNode("Safety", "127.0.0.1", 5013);

    Network::GetInstance().RegisterOutgoingTopic<StartProcessMsg>(topics::CMD_RUN, RID_START_PROCESS, serStart);
    Network::GetInstance().RegisterOutgoingTopic<StopProcessMsg>(topics::CMD_ABORT, RID_STOP_PROCESS, serStop);
    Network::GetInstance().RegisterOutgoingTopic<HeartbeatMsg>(topics::GUI_HEARTBEAT, RID_GUI_HB, serHeartbeat);
    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);
}

void System::SetupWatchdog() {
    m_heartbeat.MonitorNode(topics::CONTROLLER_HEARTBEAT, FAULT_CONTROLLER_LOST, "Controller");
    m_heartbeat.MonitorNode(topics::SAFETY_HEARTBEAT, FAULT_SAFETY_LOST, "Safety");
}

void System::StartTimerThread() {
    m_timerRunning = true;
    m_backgroundTimer = std::thread([this]() {
        while (m_timerRunning) {
            Thread::Sleep(std::chrono::milliseconds(10));
            dmq::MakeDelegate([]() { Timer::ProcessTimers(); }, m_thread).AsyncInvoke();
        }
    });
}

} // namespace cellutron
