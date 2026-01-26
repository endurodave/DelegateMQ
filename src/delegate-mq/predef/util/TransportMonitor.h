#ifndef _TRANSPORT_MONITOR_HH
#define _TRANSPORT_MONITOR_HH

#include "DelegateMQ.h"
#include "../transport/ITransportMonitor.h"
#include <map>
#include <cstdint>
#include <chrono>
#include <vector>

/// @brief A thread-safe monitor for tracking outgoing remote messages and detecting timeouts.
/// 
/// @details 
/// The TransportMonitor implements the reliability layer for remote delegate invocations. 
/// It tracks "in-flight" messages by their sequence number and timestamps them upon sending.
///
/// **Key Responsibilities:**
/// * **Timeout Detection:** Identifies messages that have not been acknowledged within the 
///   configured `TRANSPORT_TIMEOUT` duration.
/// * **Status Reporting:** Invokes the `SendStatusCb` delegate with `Status::SUCCESS` (upon ACK) 
///   or `Status::TIMEOUT` (upon expiration) to notify the application.
/// * **Thread Safety:** Internal state is protected by a recursive mutex, allowing safe access 
///   from multiple threads (e.g., sending thread vs. ACK receiving thread).
///
/// **Usage Note:**
/// This class relies on a cooperative polling model. The `Process()` method must be called 
/// periodically (typically by a background timer or the network thread loop) to scan for 
/// and handle expired messages.
class TransportMonitor : public ITransportMonitor
{
public:
    enum class Status
    {
        SUCCESS,  // Message received by remote
        TIMEOUT   // Message timeout
    };

    /// Signal emitted when a message status is determined.
    /// Subscribers receive: (remoteId, seqNum, status)
    /// Use a shared_ptr for the signal as required by SignalSafe
    std::shared_ptr<dmq::SignalSafe<void(dmq::DelegateRemoteId, uint16_t, Status)>> OnSendStatus;

    TransportMonitor(const dmq::Duration timeout) : TRANSPORT_TIMEOUT(timeout) 
    {
        OnSendStatus = dmq::MakeSignal<void(dmq::DelegateRemoteId, uint16_t, Status)>();
    }

    ~TransportMonitor() 
    { 
        const std::lock_guard<dmq::RecursiveMutex> lock(m_lock);
        m_pending.clear(); 
    }

	/// Add a sequence number
	/// param[in] seqNum - the delegate message sequence number
    /// param[in] remoteId - the remote ID
    virtual void Add(uint16_t seqNum, dmq::DelegateRemoteId remoteId) override
    {
        const std::lock_guard<dmq::RecursiveMutex> lock(m_lock);
        TimeoutData d;
        d.timeStamp = std::chrono::steady_clock::now();
        d.remoteId = remoteId;
        m_pending[seqNum] = d;
    }

	/// Remove a sequence number. Invokes SendStatusCb callback to notify 
    /// registered client of removal.
	/// param[in] seqNum - the delegate message sequence number
    virtual void Remove(uint16_t seqNum) override
    {
        const std::lock_guard<dmq::RecursiveMutex> lock(m_lock);
        auto it = m_pending.find(seqNum);
        if (it != m_pending.end())
        {
            TimeoutData d = it->second;
            m_pending.erase(it);
            if (OnSendStatus)
                (*OnSendStatus)(d.remoteId, seqNum, Status::SUCCESS);
        }
    }

	/// Call periodically to process message timeouts
    void Process()
    {
        // 1. Collect expired items into a local list
        struct ExpiredItem { uint16_t seq; TimeoutData data; };
        std::vector<ExpiredItem> expiredItems;

        {
            // Lock ONLY while reading/modifying the map
            const std::lock_guard<dmq::RecursiveMutex> lock(m_lock);
            auto now = std::chrono::steady_clock::now();
            auto it = m_pending.begin();

            while (it != m_pending.end())
            {
                auto elapsed = std::chrono::duration_cast<dmq::Duration>(now - (*it).second.timeStamp);

                if (elapsed > TRANSPORT_TIMEOUT)
                {
                    expiredItems.push_back({ (*it).first, (*it).second });
                    it = m_pending.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        } // Lock is RELEASED here

        // 2. Fire callbacks without holding the lock
        // This prevents the deadlock: Process(Lock A) -> Callback -> Send -> Transport(Wait for Thread B)
        // Meanwhile Thread B -> Send -> Add(Wait for Lock A)
        if (OnSendStatus && !expiredItems.empty())
        {
            for (const auto& item : expiredItems)
            {
                // Simple logging to console
                std::cerr << "TransportMonitor::Process TIMEOUT RemoteID: " << item.data.remoteId << " Seq: " << item.seq << std::endl;
                (*OnSendStatus)(item.data.remoteId, item.seq, Status::TIMEOUT);
            }
        }
    }

private:
    struct TimeoutData
    {
        dmq::DelegateRemoteId remoteId = 0;
        std::chrono::steady_clock::time_point timeStamp;
    };

	std::map<uint16_t, TimeoutData> m_pending;
	const dmq::Duration TRANSPORT_TIMEOUT;
    dmq::RecursiveMutex m_lock;
};

#endif
