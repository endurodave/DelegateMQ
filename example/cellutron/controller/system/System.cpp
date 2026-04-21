#include "System.h"
#include "process/Process.h"
#include "actuators/Actuators.h"
#include "sensors/Sensors.h"
#include "Network.h"
#include "RemoteConfig.h"
#include "Constants.h"
#include <cstdio>

using namespace dmq;

namespace cellutron {

void System::Initialize() {
    printf("Controller: System initializing...\n");

    RegisterSerializers();
    RegisterStringifiers();

    // 1. Create System Thread
    if (!m_thread.CreateThread(std::chrono::seconds(2))) {
        printf("Controller: ERROR - Failed to create system thread!\n");
        return;
    }

    // 2. Instantiate state machines
    process::Process::GetInstance().Initialize();

    // 3. Initialize Subsystems
    actuators::Actuators::GetInstance().Initialize();
    sensors::Sensors::GetInstance().Initialize();

    // 4. Setup Wiring
    SetupLocalSubscriptions();
    SetupNetwork();
    SetupWatchdog();

    // 5. Start Heartbeat
    m_heartbeat.Start();

    printf("Controller: System ready.\n");
}

void System::Tick() {
    m_heartbeat.Tick();
}

void System::SetupLocalSubscriptions() {
    m_startConn = DataBus::Subscribe<StartProcessMsg>("cell/cmd/run", [](StartProcessMsg msg) {
        printf("Controller: >>>> RECEIVED START COMMAND <<<<\n");
        process::Process::GetInstance().Start();
    }, &m_thread);

    m_stopConn = DataBus::Subscribe<StopProcessMsg>("cell/cmd/abort", [](StopProcessMsg msg) {
        printf("Controller: >>>> RECEIVED ABORT COMMAND <<<<\n");
        process::Process::GetInstance().Abort();
    }, &m_thread);

    m_faultConn = DataBus::Subscribe<FaultMsg>(topics::FAULT, [](FaultMsg msg) {
        if (process::Process::GetInstance().GetCellProcess().GetCurrentState() != 25) { // 25 = ST_FAULT
            printf("Controller: >>>> CRITICAL FAULT RECEIVED (Code: %d) <<<<\n", msg.faultCode);
            process::Process::GetInstance().Fault();
        }
    }, &m_thread);
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5011, "Controller"); 
    
    // Incoming from Network
    Network::GetInstance().RegisterIncomingTopic<StartProcessMsg>("cell/cmd/run", RID_START_PROCESS, serStart);
    Network::GetInstance().RegisterIncomingTopic<StopProcessMsg>("cell/cmd/abort", RID_STOP_PROCESS, serStop);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::GUI_HEARTBEAT, RID_GUI_HB, serHeartbeat);

    // Setup Outgoing Topics
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);
    Network::GetInstance().AddRemoteNode("Safety", "127.0.0.1", 5013);

    Network::GetInstance().RegisterOutgoingTopic<RunStatusMsg>("cell/status/run", RID_RUN_STATUS, serRun);
    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterOutgoingTopic<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, RID_CONTROLLER_HB, serHeartbeat);
    Network::GetInstance().RegisterOutgoingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterOutgoingTopic<ActuatorStatusMsg>("hw/status/actuator", RID_ACTUATOR_STATUS, serActuator);
    Network::GetInstance().RegisterOutgoingTopic<SensorStatusMsg>("hw/status/sensor", RID_SENSOR_STATUS, serSensor);
}

void System::SetupWatchdog() {
    m_heartbeat.MonitorNode(topics::SAFETY_HEARTBEAT, FAULT_SAFETY_LOST, "Safety");
    m_heartbeat.MonitorNode(topics::GUI_HEARTBEAT, FAULT_GUI_LOST, "GUI");
}

} // namespace cellutron
