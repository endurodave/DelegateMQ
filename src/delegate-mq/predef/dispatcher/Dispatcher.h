#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Dispatch callable argument data to a remote endpoint.

#include "delegate/IDispatcher.h"
#include "predef/transport/DmqHeader.h"
#include "predef/transport/ITransport.h"
#include <sstream>
#include <mutex>

/// @brief Dispatcher sends data to the transport for transmission to the endpoint.
class Dispatcher : public dmq::IDispatcher
{
public:
    Dispatcher() = default;
    ~Dispatcher()
    {
        m_transport = nullptr;
    }

    void SetTransport(ITransport* transport)
    {
        m_transport = transport;
    }

    // Send argument data to the transport
    virtual int Dispatch(std::ostream& os, dmq::DelegateRemoteId id) 
    {
        xostringstream* ss = dynamic_cast<xostringstream*>(&os);
        if (!ss)
        {
            LOG_ERROR("Dispatcher::Dispatch - Null ss, id={}", id);
            return -1;
        }

        if (m_transport)
        {
            DmqHeader header(id, DmqHeader::GetNextSeqNum());
            int err = m_transport->Send(*ss, header);
            LOG_INFO("Dispatcher::Dispatch id={} seqNum={} err={}", header.GetId(), header.GetSeqNum(), err);
            return err;
        }
        return -1;
    }

private:
    ITransport* m_transport = nullptr;
};

#endif