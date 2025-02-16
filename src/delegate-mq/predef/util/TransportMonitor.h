#ifndef _TRANSPORT_MONITOR_HH
#define _TRANSPORT_MONITOR_HH

#include "DelegateMQ.h"
#include "../transport/ITransportMonitor.h"
#include <map>
#include <cstdint>
#include <chrono>

/// @brief Monitors remote delegate message timeouts. Class is not thread safe.
/// Call TransportMonitor::Process() periodically to for timeout handling.
/// Depending on the transport implementation, the message might still be delivered.
/// A timeout expiring just means that an ack was not receied within the time 
/// specified.
class TransportMonitor : public ITransportMonitor
{
public:
    dmq::MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId)> Timeout;

    TransportMonitor(const std::chrono::milliseconds& timeout) : TIMEOUT(timeout) {}
    ~TransportMonitor() { m_pending.clear(); }

	/// Add a sequence number
	/// param[in] seqNum - the message sequence number
    /// param[in] remoteId - the remote ID
    void Add(uint16_t seqNum, dmq::DelegateRemoteId remoteId)
    {
        TimeoutData d;
        d.timeStamp = std::chrono::system_clock::now();
        d.remoteId = remoteId;
        m_pending[seqNum] = d;
    }

	/// Remove a sequence number
	/// param[in] seqNum - the message sequence number
    void Remove(uint16_t seqNum)
    {
        if (m_pending.count(seqNum) != 0)
        {
            TimeoutData d = m_pending[seqNum];
            m_pending.erase(seqNum);
        }
    }

	/// Called periodically to process message timeouts
    void Process()
    {
        auto now = std::chrono::system_clock::now();
        auto it = m_pending.begin();
        while (it != m_pending.end()) 
        {
            // Calculate the elapsed time as a duration
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - (*it).second.timeStamp);

            // Has timeout expired?
            if (elapsed > TIMEOUT) 
            {
                Timeout((*it).first, (*it).second.remoteId);
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
	const std::chrono::milliseconds TIMEOUT;
};

#endif
