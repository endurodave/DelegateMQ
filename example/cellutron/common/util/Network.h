#ifndef NETWORK_H
#define NETWORK_H

#include "DelegateMQ.h"
#include "port/transport/win32-udp/Win32UdpTransport.h"
#include <string>
#include <memory>
#include <unordered_map>

class Network {
public:
    static Network& GetInstance() {
        static Network instance;
        return instance;
    }

    /// Initialize network and start receiving.
    void Initialize(uint16_t subPort, const std::string& nodeName = "Unknown");

    /// Stop network.
    void Shutdown();

    /// Add a remote node to the network.
    void AddRemoteNode(const std::string& nodeName, const std::string& addr, uint16_t port);

    /// Map a topic to a specific remote node.
    void MapTopicToRemote(const std::string& topic, dmq::DelegateRemoteId remoteId, const std::string& nodeName);

    /// Register a serializer for an outgoing topic.
    template <typename T>
    void RegisterOutgoingTopic(const std::string& topic, dmq::DelegateRemoteId remoteId, dmq::ISerializer<void(T)>& serializer) {
        dmq::DataBus::RegisterSerializer<T>(topic, serializer);
        m_outgoingTopics.push_back({topic, remoteId});
        for (auto& [name, node] : m_remoteNodes) {
            node.participant->AddRemoteTopic(topic, remoteId);
        }
    }

    /// Register an incoming topic from the network.
    template <typename T>
    void RegisterIncomingTopic(const std::string& topic, dmq::DelegateRemoteId remoteId, dmq::ISerializer<void(T)>& serializer) {
        if (m_subParticipant) {
            dmq::DataBus::AddIncomingTopic<T>(topic, remoteId, *m_subParticipant, serializer);
        }
    }

private:
    Network() = default;
    ~Network();

    void ReceiverThread();

    bool m_running = false;
    UdpTransport m_subTransport;
    std::shared_ptr<dmq::Participant> m_subParticipant;
    
    // Standardized thread name for Active Object subsystem
    Thread m_thread{"NetworkThread"};

    struct RemoteNode {
        std::unique_ptr<UdpTransport> transport;
        std::shared_ptr<dmq::Participant> participant;
    };
    std::unordered_map<std::string, RemoteNode> m_remoteNodes;
    std::vector<std::pair<std::string, dmq::DelegateRemoteId>> m_outgoingTopics;
    std::string m_nodeName;
};

#endif
