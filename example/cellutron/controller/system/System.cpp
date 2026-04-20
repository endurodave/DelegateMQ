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

System::~System() {
    delete m_heartbeatTimer;
    delete m_commThread;
}

void System::Initialize() {
    printf("Controller: System initializing...\n");
    m_ticksWaited = 0;

    // 1. Create Communication/Sequencing Thread
    m_commThread = new Thread("CommThread", 200, FullPolicy::DROP);
    if (!m_commThread->CreateThread(std::chrono::seconds(2))) {
        printf("Controller: ERROR - Failed to create comm thread!\n");
        return;
    }

    // 2. Instantiate state machines
    Process::GetInstance().Initialize();

    // 3. Initialize Subsystems
    Actuators::GetInstance().Initialize();
    Sensors::GetInstance().Initialize();

    // 4. Setup Wiring
    SetupLocalSubscriptions();
    SetupNetwork();
    SetupHeartbeat();
    SetupWatchdog();

    printf("Controller: System ready.\n");
}

void System::Tick() {
    m_ticksWaited++;
}

void System::SetupLocalSubscriptions() {
    m_startConn = DataBus::Subscribe<StartProcessMsg>("cell/cmd/run", [](StartProcessMsg msg) {
        printf("Controller: >>>> RECEIVED START COMMAND <<<<\n");
        Process::GetInstance().Start();
    }, m_commThread);

    m_stopConn = DataBus::Subscribe<StopProcessMsg>("cell/cmd/abort", [](StopProcessMsg msg) {
        printf("Controller: >>>> RECEIVED ABORT COMMAND <<<<\n");
        Process::GetInstance().Abort();
    }, m_commThread);

    m_faultConn = DataBus::Subscribe<FaultMsg>("cell/fault", [](FaultMsg msg) {
        if (Process::GetInstance().GetCellProcess().GetCurrentState() != 25) { // 25 = ST_FAULT
            printf("Controller: >>>> CRITICAL FAULT RECEIVED (Code: %d) <<<<\n", msg.faultCode);
            Process::GetInstance().Fault();
        }
    }, m_commThread);
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5011, "Controller"); 
    
    // Incoming from Network
    Network::GetInstance().RegisterIncomingTopic<StartProcessMsg>("cell/cmd/run", RID_START_PROCESS, serStart);
    Network::GetInstance().RegisterIncomingTopic<StopProcessMsg>("cell/cmd/abort", RID_STOP_PROCESS, serStop);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);

    // Setup Outgoing Topics
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);
    Network::GetInstance().AddRemoteNode("Safety", "127.0.0.1", 5013);

    Network::GetInstance().RegisterOutgoingTopic<RunStatusMsg>("cell/status/run", RID_RUN_STATUS, serRun);
    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterOutgoingTopic<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, RID_CONTROLLER_HB, serHeartbeat);
    Network::GetInstance().RegisterOutgoingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterOutgoingTopic<ActuatorStatusMsg>("hw/status/actuator", RID_ACTUATOR_STATUS, serActuator);
    Network::GetInstance().RegisterOutgoingTopic<SensorStatusMsg>("hw/status/sensor", RID_SENSOR_STATUS, serSensor);
}

void System::SetupHeartbeat() {
    m_heartbeatTimer = new Timer();
    m_heartbeatConn = m_heartbeatTimer->OnExpired.Connect(dmq::MakeDelegate([this]() {
        DataBus::Publish<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, { ++m_heartbeatCount });
    }, *m_commThread));
    m_heartbeatTimer->Start(std::chrono::milliseconds(500));
}

void System::SetupWatchdog() {
    m_safetyWatchdog = std::make_unique<dmq::DeadlineSubscription<HeartbeatMsg>>(
        topics::SAFETY_HEARTBEAT,
        std::chrono::seconds(2),
        [](const HeartbeatMsg&) { /* No-op on success */ },
        [this]() {
            if (m_ticksWaited >= 15 && Process::GetInstance().GetCellProcess().GetCurrentState() != 25) { 
                printf("Controller: CRITICAL - Safety node heartbeat lost! TRIGGERING FAULT.\n");
                DataBus::Publish<FaultMsg>("cell/fault", { FAULT_SAFETY_LOST });
                Process::GetInstance().Fault();
            }
        },
        m_commThread
    );
}

} // namespace cellutron
