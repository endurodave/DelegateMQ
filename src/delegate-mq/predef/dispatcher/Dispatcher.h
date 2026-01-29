#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file Dispatcher.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief The bridge between the serialization layer and the physical transport layer.
/// 
/// @details
/// The `Dispatcher` class is responsible for packaging serialized function arguments 
/// into a valid DelegateMQ message and handing it off to the transport.
/// 
/// **Key Responsibilities:**
/// 1. **Message Construction:** Creates the protocol header (`DmqHeader`) containing 
///    the Remote ID and a monotonic Sequence Number.
/// 2. **Stream Management:** Validates that the output stream is compatible 
///    (expects `xostringstream`).
/// 3. **Dispatch:** Forwards the header and the serialized payload (stream) to the 
///    registered `ITransport::Send()` method.
/// 
/// **Usage:**
/// This class is typically used internally by `DelegateRemote` to finalize a remote 
/// procedure call before transmission.

#include "delegate/IDispatcher.h"
#include "predef/transport/DmqHeader.h"
#include "predef/transport/ITransport.h"
#include <sstream>

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