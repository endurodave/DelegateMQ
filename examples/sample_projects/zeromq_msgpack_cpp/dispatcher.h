#ifndef DISPATCHER_H
#define DISPATCHER_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "transport.h"
#include "IDispatcher.h"
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
    virtual int Dispatch(std::ostream& os) 
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        ss << os.rdbuf();
        if (m_transport)
            m_transport->Send(ss);
        return 0;
    }

private:
    Transport* m_transport = nullptr;
};

#endif