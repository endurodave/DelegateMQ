#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Dispatch callable argument data to a remote endpoint.

#include "delegate/IDispatcher.h"
#if defined(DMQ_TRANSPORT_ZEROMQ)
    #include "predef/transport/zeromq/ZeroMqTransport.h"
#elif defined (DMQ_TRANSPORT_WIN32_PIPE)
    #include "predef/transport/win32-pipe/Win32PipeTransport.h"
#elif defined (DMQ_TRANSPORT_WIN32_UDP)
    #include "predef/transport/win32-udp/Win32UdpTransport.h"
#else
    #error "Include a transport header."
#endif
#include "predef/transport/DmqHeader.h"
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
            return -1;

        if (m_transport)
        {
            DmqHeader header(id, DmqHeader::GetNextSeqNum());
            int err = m_transport->Send(*ss, header);
            return err;
        }
        return -1;
    }

private:
    ITransport* m_transport = nullptr;
};

#endif