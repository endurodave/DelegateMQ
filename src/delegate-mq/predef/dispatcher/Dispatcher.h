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
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        DmqHeader header(id, DmqHeader::GetNextSeqNum());

        // Write each header value using the getters from DmqHeader
        auto marker = header.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id_value = header.GetId();
        ss.write(reinterpret_cast<const char*>(&id_value), sizeof(id_value));

        auto seqNum = header.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        // Insert delegate arguments from the stream (os)
        ss << os.rdbuf();

        if (m_transport)
        {
            int err = m_transport->Send(ss);
            return err;
        }
        return -1;
    }

private:
    ITransport* m_transport = nullptr;
};

#endif