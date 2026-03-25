#ifndef DMQ_PARTICIPANT_H
#define DMQ_PARTICIPANT_H

#include "DelegateMQ.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace dmq {

// A Participant represents a remote node in the DataBus network.
// It manages the transport and the remote channels for different signatures.
//
// LIFETIME NOTE: This class assumes that the ITransport object passed to the constructor 
// will outlive the Participant instance.
class Participant {
public:
    Participant(ITransport& transport) : m_transport(&transport) {}

    // Add a remote topic mapping.
    // When local DataBus publishes to 'topic', it will be sent to this participant using 'remoteId'.
    void AddRemoteTopic(const std::string& topic, dmq::DelegateRemoteId remoteId) {
        std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
        m_topicToRemoteId[topic] = remoteId;
    }

    // Process incoming data from the transport.
    // @return The result code from ITransport::Receive.
    int ProcessIncoming() {
        xstringstream is(std::ios::in | std::ios::out | std::ios::binary);
        DmqHeader header;
        dmq::IRemoteInvoker* invoker = nullptr;

        int result = m_transport->Receive(is, header);
        if (result == 0) {
            // Validate header marker
            if (header.GetMarker() != DmqHeader::MARKER) {
                return -1; // Protocol error
            }

            {
                std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
                auto it = m_channels.find(header.GetId());
                if (it != m_channels.end()) {
                    invoker = it->second.invoker;
                }
            }

            // Invoke outside the lock to prevent deadlocks and allow re-entry
            if (invoker) {
                invoker->Invoke(is);
            }
        }
        return result;
    }

    // Send data for a topic.
    template <typename T>
    void Send(const std::string& topic, T data, dmq::ISerializer<void(T)>& serializer) {
        dmq::DelegateRemoteId remoteId;
        if (GetRemoteId(topic, remoteId)) {
            auto channel = GetOrCreateChannel<T>(remoteId, serializer);
            (*channel)(data);
        }
    }

    // Register a local handler for a remote topic using a `std::function`.
    template <typename T>
    void RegisterHandler(dmq::DelegateRemoteId remoteId, dmq::ISerializer<void(T)>& serializer, std::function<void(T)> func) {
        std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
        auto channel = std::make_shared<dmq::RemoteChannel<void(T)>>(*m_transport, serializer);
        
        // Use Bind() to register the callback for incoming calls.
        channel->Bind(func, remoteId);
        
        m_channels[remoteId] = { channel, channel->GetEndpoint() };
    }

    // Register a local handler for a remote topic using a raw lambda or functor.
    template <typename T, typename F, typename = std::enable_if_t<trait::is_callable<F>::value>>
    void RegisterHandler(dmq::DelegateRemoteId remoteId, dmq::ISerializer<void(T)>& serializer, F&& func) {
        std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
        auto channel = std::make_shared<dmq::RemoteChannel<void(T)>>(*m_transport, serializer);

        // Use Bind() to register the callback for incoming calls.
        channel->Bind(std::forward<F>(func), remoteId);

        m_channels[remoteId] = { channel, channel->GetEndpoint() };
    }

private:
    struct ChannelInvoker {
        std::shared_ptr<void> channel;
        dmq::IRemoteInvoker* invoker = nullptr;
    };

    bool GetRemoteId(const std::string& topic, dmq::DelegateRemoteId& remoteId) {
        std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
        auto it = m_topicToRemoteId.find(topic);
        if (it != m_topicToRemoteId.end()) {
            remoteId = it->second;
            return true;
        }
        return false;
    }

    template <typename T>
    std::shared_ptr<dmq::RemoteChannel<void(T)>> GetOrCreateChannel(dmq::DelegateRemoteId remoteId, dmq::ISerializer<void(T)>& serializer) {
        std::lock_guard<dmq::RecursiveMutex> lock(m_mutex);
        auto it = m_channels.find(remoteId);
        if (it != m_channels.end()) {
            return std::static_pointer_cast<dmq::RemoteChannel<void(T)>>(it->second.channel);
        }

        auto channel = std::make_shared<dmq::RemoteChannel<void(T)>>(*m_transport, serializer);
        
        // Establish the remote ID for sending via operator(). 
        channel->SetRemoteId(remoteId); 
        
        m_channels[remoteId] = { channel, channel->GetEndpoint() };
        return channel;
    }

    ITransport* m_transport;
    dmq::RecursiveMutex m_mutex;
    std::unordered_map<std::string, dmq::DelegateRemoteId> m_topicToRemoteId;
    std::unordered_map<dmq::DelegateRemoteId, ChannelInvoker> m_channels;
};

} // namespace dmq

#endif // DMQ_PARTICIPANT_H
