#include "System.h"
#include "Network.h"
#include "RemoteConfig.h"
#include "Constants.h"
#include <cstdio>

using namespace dmq;

namespace cellutron {

void System::Initialize() {
    printf("Safety: System initializing...\n");

    RegisterSerializers();
    RegisterStringifiers();
    m_thread.CreateThread(std::chrono::seconds(2));

    SetupLocalSubscriptions();
    SetupNetwork();
    SetupWatchdog();

    m_heartbeat.Start();

    printf("Safety: System ready.\n");
}

void System::Tick() {
    m_heartbeat.Tick();
}

void System::SetupLocalSubscriptions() {
    m_speedConn = DataBus::Subscribe<CentrifugeSpeedMsg>(topics::CMD_CENTRIFUGE_SPEED, [this](CentrifugeSpeedMsg msg) {
        if (!m_faulted && msg.rpm > MAX_CENTRIFUGE_RPM) {
            m_faulted = true;
            printf("Safety: CRITICAL - Centrifuge speed exceeded limit! (%u RPM). TRIGGERING FAULT.\n", msg.rpm);
            DataBus::Publish<FaultMsg>(topics::FAULT, { FAULT_OVERSPEED });
        }
    }, &m_thread);

    m_faultConn = DataBus::Subscribe<FaultMsg>(topics::FAULT, [this](FaultMsg) {
        m_faulted = true;
    }, &m_thread);
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5013, "Safety"); 
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>(topics::CMD_CENTRIFUGE_SPEED, RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::CONTROLLER_HEARTBEAT, RID_CONTROLLER_HB, serHeartbeat);
    Network::GetInstance().RegisterIncomingTopic<HeartbeatMsg>(topics::GUI_HEARTBEAT, RID_GUI_HB, serHeartbeat);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);

    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);

    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>(topics::FAULT, RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterOutgoingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);
}

void System::SetupWatchdog() {
    m_heartbeat.MonitorNode(topics::CONTROLLER_HEARTBEAT, FAULT_CONTROLLER_LOST, "Controller");
    m_heartbeat.MonitorNode(topics::GUI_HEARTBEAT, FAULT_GUI_LOST, "GUI");
}

} // namespace cellutron
