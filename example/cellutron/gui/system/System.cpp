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

    RegisterStringifiers();
    SetupNetwork();
    
    Logs::GetInstance().Initialize();
    Alarms::GetInstance().Initialize();

    StartTimerThread();

    printf("GUI: System ready.\n");
}

void System::Shutdown() {
    if (m_timerRunning) {
        m_timerRunning = false;
        if (m_backgroundTimer.joinable()) {
            m_backgroundTimer.join();
        }
        m_timerThread.ExitThread();

        Alarms::GetInstance().Shutdown();
        Logs::GetInstance().Shutdown();
        Network::GetInstance().Shutdown();
    }
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5010, "GUI");

    // Incoming Topics
    Network::GetInstance().RegisterIncomingTopic<RunStatusMsg>("cell/status/run", RID_RUN_STATUS, serRun);
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterIncomingTopic<ActuatorStatusMsg>("hw/status/actuator", RID_ACTUATOR_STATUS, serActuator);
    Network::GetInstance().RegisterIncomingTopic<SensorStatusMsg>("hw/status/sensor", RID_SENSOR_STATUS, serSensor);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, RID_CONTROLLER_HB, serHeartbeat);

    // Outgoing Topics
    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().RegisterOutgoingTopic<StartProcessMsg>("cell/cmd/run", RID_START_PROCESS, serStart);
    Network::GetInstance().RegisterOutgoingTopic<StopProcessMsg>("cell/cmd/abort", RID_STOP_PROCESS, serStop);
}

void System::StartTimerThread() {
    m_timerThread.CreateThread();
    m_timerRunning = true;
    m_backgroundTimer = std::thread([this]() {
        while (m_timerRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            dmq::MakeDelegate([]() { Timer::ProcessTimers(); }, m_timerThread).AsyncInvoke();
        }
    });
}

} // namespace cellutron
