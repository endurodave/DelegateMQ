#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "delegate/IDispatcher.h"
#include "predef/transport/msgpack/Transport.h"
#include <sstream>

/// @brief Dispatcher sends data to the transport for transmission to the endpoint.
class Dispatcher : public DelegateLib::IDispatcher
{
public:
    Dispatcher() = default;
    ~Dispatcher()
    {
        if (m_transport)
            m_transport->Close();
        m_transport = nullptr;
    }

    void SetTransport(Transport* transport)
    {
        m_transport = transport;
    }

    // Send argument data to the transport
    virtual int Dispatch(std::ostream& os, DelegateLib::DelegateRemoteId id) 
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Prepend marker (0x55AA as a short)
        const short MARKER = 0x55AA;
        ss.write(reinterpret_cast<const char*>(&MARKER), sizeof(MARKER));

        // Prepend DelegateRemoteId
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        // insert delegate arguments
        ss << os.rdbuf();
        if (m_transport)
            m_transport->Send(ss);
        return 0;
    }

private:
    Transport* m_transport = nullptr;
};

#endif