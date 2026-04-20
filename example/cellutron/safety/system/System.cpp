#include "System.h"
#include "Network.h"
#include "RemoteConfig.h"
#include "Constants.h"
#include <cstdio>

using namespace dmq;

namespace cellutron {

System::~System() {
    delete m_heartbeatTimer;
}

void System::Initialize() {
    printf("Safety: System initializing...\n");

    RegisterStringifiers();
    m_thread.CreateThread(std::chrono::seconds(2));

    SetupLocalSubscriptions();
    SetupNetwork();
    SetupHeartbeat();

    printf("Safety: System ready.\n");
}

void System::SetupLocalSubscriptions() {
    m_speedConn = DataBus::Subscribe<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", [this](CentrifugeSpeedMsg msg) {
        if (!m_faulted && msg.rpm > MAX_CENTRIFUGE_RPM) {
            m_faulted = true;
            printf("Safety: CRITICAL - Centrifuge speed exceeded limit! (%u RPM). TRIGGERING FAULT.\n", msg.rpm);
            DataBus::Publish<FaultMsg>("cell/fault", { FAULT_OVERSPEED });
        }
    }, &m_thread);
}

void System::SetupNetwork() {
    Network::GetInstance().Initialize(5013, "Safety"); 
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);

    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().AddRemoteNode("GUI", "127.0.0.1", 5010);

    Network::GetInstance().RegisterOutgoingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterOutgoingTopic<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, RID_SAFETY_HB, serHeartbeat);
}

void System::SetupHeartbeat() {
    m_heartbeatTimer = new Timer();
    m_heartbeatConn = m_heartbeatTimer->OnExpired.Connect(dmq::MakeDelegate([this]() {
        DataBus::Publish<HeartbeatMsg>(topics::SAFETY_HEARTBEAT, { ++m_heartbeatCount });
    }, m_thread));
    m_heartbeatTimer->Start(std::chrono::milliseconds(500));
}

} // namespace cellutron
