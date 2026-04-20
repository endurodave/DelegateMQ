#include "Network.h"
#include <iostream>
#include <vector>

using namespace dmq;

Network::~Network() {
    Shutdown();
}

void Network::Initialize(uint16_t subPort, const std::string& nodeName) {
    if (m_running) return;

    m_nodeName = nodeName;
    m_subParticipant = std::make_shared<Participant>(m_subTransport);

    std::cout << "Network: Initializing node '" << m_nodeName << "' on port " << subPort << "..." << std::endl;

    if (m_subTransport.Create(UdpTransport::Type::SUB, "127.0.0.1", subPort) != 0) {
        std::cerr << "Network: ERROR - Failed to create subscriber transport on port " << subPort << std::endl;
        return;
    }

    m_running = true;
    
    // Enable DelegateMQ Watchdog (2 second timeout)
    m_thread.CreateThread(std::chrono::seconds(2));

    // Initialize Bridges for dmq-spy and dmq-monitor
    SpyBridge::Start("127.0.0.1", 9999);
    NodeBridge::StartMulticast(m_nodeName, "239.1.1.1", 9998);

    // Post the receiver loop to the standardized worker thread
    dmq::MakeDelegate(this, &Network::ReceiverThread, m_thread).AsyncInvoke();
    
    std::cout << "Network: Receiver thread started on port " << subPort << std::endl;
}

void Network::Shutdown() {
    if (!m_running) return;

    std::cout << "Network: Shutting down..." << std::endl;
    m_running = false;
    m_subTransport.Close();
    m_thread.ExitThread();

    m_remoteNodes.clear();
}

void Network::AddRemoteNode(const std::string& nodeName, const std::string& addr, uint16_t port) {
    std::cout << "Network: Adding remote node '" << nodeName << "' at " << addr << ":" << port << "..." << std::endl;
    auto transport = std::make_unique<UdpTransport>();
    if (transport->Create(UdpTransport::Type::PUB, addr.c_str(), port) == 0) {
        auto participant = std::make_shared<Participant>(*transport);

        // Apply all outgoing topic-RID mappings registered so far
        for (const auto& [topic, remoteId] : m_outgoingTopics) {
            participant->AddRemoteTopic(topic, remoteId);
        }

        // Register with DataBus for global distribution
        DataBus::AddParticipant(participant);

        RemoteNode node;
        node.transport = std::move(transport);
        node.participant = participant;
        m_remoteNodes[nodeName] = std::move(node);
        
        std::cout << "Network: SUCCESS - Remote node '" << nodeName << "' added to DataBus." << std::endl;
    } else {
        std::cerr << "Network: ERROR - Failed to connect to node " << nodeName << " at " << addr << ":" << port << std::endl;
    }
}

void Network::MapTopicToRemote(const std::string& topic, dmq::DelegateRemoteId remoteId, const std::string& nodeName) {
    auto it = m_remoteNodes.find(nodeName);
    if (it != m_remoteNodes.end()) {
        std::cout << "Network: Mapping topic '" << topic << "' (ID: " << remoteId << ") to node '" << nodeName << "'" << std::endl;
        it->second.participant->AddRemoteTopic(topic, remoteId);
    } else {
        std::cerr << "Network: ERROR - Node '" << nodeName << "' not found for topic '" << topic << "'" << std::endl;
    }
}

void Network::ReceiverThread() {
    std::cout << "Network: ReceiverThread loop entered." << std::endl;
    while (m_running) {
        if (m_subParticipant) {
            int result = m_subParticipant->ProcessIncoming();
            if (result != 0 && result != -1) { 
                std::cerr << "Network: ProcessIncoming returned error " << result << std::endl;
            }
        }
    }
    std::cout << "Network: ReceiverThread loop exited." << std::endl;
}
