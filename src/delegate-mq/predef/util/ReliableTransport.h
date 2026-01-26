#ifndef RELIABLE_TRANSPORT_H
#define RELIABLE_TRANSPORT_H

#include "../transport/ITransport.h"
#include "RetryMonitor.h"

/// @brief Adapter to enable automatic retries on any ITransport.
/// @details Routes Send() calls through the RetryMonitor before passing them
/// to the physical transport.
class ReliableTransport : public ITransport
{
public:
    ReliableTransport(ITransport& transport, RetryMonitor& retry) 
        : m_transport(transport), m_retry(retry) {}

    /// @brief Sends data via the RetryMonitor to ensure reliability.
    virtual int Send(xostringstream& os, const DmqHeader& header) override {
        return m_retry.SendWithRetry(os, header);
    }

    /// @brief Pass-through for receiving data.
    virtual int Receive(xstringstream& is, DmqHeader& header) override {
        return m_transport.Receive(is, header);
    }

private:
    ITransport& m_transport;
    RetryMonitor& m_retry;
};

#endif