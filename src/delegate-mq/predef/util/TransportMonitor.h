#ifndef _TRANSPORT_MONITOR_HH
#define _TRANSPORT_MONITOR_HH

#include "DelegateMQ.h"
#include "../transport/ITransportMonitor.h"
#include <map>
#include <cstdint>
#include <chrono>

/// @brief Monitors remote delegate send message timeouts. Class is thread safe.
/// Call TransportMonitor::Process() periodically for timeout handling.
/// Depending on the transport implementation, the message might still be delivered
/// event if the monitor SendStatusCb callback is invoked. A timeout expiring just means 
/// that an ack was not received within the time specified.
class TransportMonitor : public ITransportMonitor
{
public:
    enum class Status
    {
        SUCCESS,  // Message received by remote
        TIMEOUT   // Message timeout
    };

    // Delegate callback to monitor message send status. Callback invoked by 
    // either the Remove() caller's thread or the Process() caller's thread.
    dmq::MulticastDelegateSafe<void(dmq::DelegateRemoteId id, uint16_t seqNum, Status status)> SendStatusCb;

    TransportMonitor(const dmq::Duration timeout) : TRANSPORT_TIMEOUT(timeout) {}
    ~TransportMonitor() 
    { 
        const std::lock_guard<std::mutex> lock(m_lock);
        m_pending.clear(); 
    }

	/// Add a sequence number
	/// param[in] seqNum - the delegate message sequence number
    /// param[in] remoteId - the remote ID
    virtual void Add(uint16_t seqNum, dmq::DelegateRemoteId remoteId) override
    {
        const std::lock_guard<std::mutex> lock(m_lock);
        TimeoutData d;
        d.timeStamp = std::chrono::system_clock::now();
        d.remoteId = remoteId;
        m_pending[seqNum] = d;
    }

	/// Remove a sequence number. Invokes SendStatusCb callback to notify 
    /// registered client of removal.
	/// param[in] seqNum - the delegate message sequence number
    virtual void Remove(uint16_t seqNum) override
    {
        const std::lock_guard<std::mutex> lock(m_lock);
        auto it = m_pending.find(seqNum);
        if (it != m_pending.end())
        {
            TimeoutData d = it->second;
            m_pending.erase(it);
            SendStatusCb(d.remoteId, seqNum, Status::SUCCESS);
        }
    }

	/// Call periodically to process message timeouts
    void Process()
    {
        const std::lock_guard<std::mutex> lock(m_lock);
        auto now = std::chrono::system_clock::now();
        auto it = m_pending.begin();
        while (it != m_pending.end()) 
        {
            // Calculate the elapsed time as a duration
            auto elapsed = std::chrono::duration_cast<dmq::Duration>(now - (*it).second.timeStamp);

            // Has message timeout expired?
            if (elapsed > TRANSPORT_TIMEOUT)
            {
                SendStatusCb((*it).second.remoteId, (*it).first, Status::TIMEOUT);
                LOG_ERROR("TransportMonitor::Process TIMEOUT {} {}", (*it).second.remoteId, (*it).first);
                it = m_pending.erase(it);                
            }
            else 
            {
                ++it;
            }
        }
    }

private:
    struct TimeoutData
    {
        dmq::DelegateRemoteId remoteId;
        std::chrono::system_clock::time_point timeStamp;
    };

	std::map<uint16_t, TimeoutData> m_pending;
	const dmq::Duration TRANSPORT_TIMEOUT;
    std::mutex m_lock;
};

#endif
