#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// Dispatch callable argument data to a remote endpoint.

#include "delegate/IDispatcher.h"
#if defined(DMQ_TRANSPORT_ZEROMQ)
    #include "predef/transport/zeromq/Transport.h"
#elif defined (DMQ_TRANSPORT_WIN32_PIPE)
    #include "predef/transport/win32-pipe/Transport.h"
#elif defined (DMQ_TRANSPORT_WIN32_UDP)
    #include "predef/transport/win32-udp/Transport.h"
#else
    #error "Include a transport header."
#endif
#include "MsgHeader.h"
#include <sstream>
#include <mutex>

/// @brief Dispatcher sends data to the transport for transmission to the endpoint.
class Dispatcher : public DelegateMQ::IDispatcher
{
public:
    Dispatcher() = default;
    ~Dispatcher()
    {
        m_transport = nullptr;
    }

    void SetTransport(Transport* transport)
    {
        m_transport = transport;
    }

    // Send argument data to the transport
    virtual int Dispatch(std::ostream& os, DelegateMQ::DelegateRemoteId id) 
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        MsgHeader header(id, GetSeqNum());

        // Write each header value using the getters from MsgHeader
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
    uint16_t GetSeqNum()
    {
        static std::atomic<uint16_t> seqNum(0);

        // Atomically increment and return the previous value
        return seqNum.fetch_add(1, std::memory_order_relaxed);
    }

    Transport* m_transport = nullptr;
};

#endif