#ifndef RECEIVER_H
#define RECEIVER_H

/// @file main.cpp
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief MQTT with remote delegates examples. 

#include "DelegateMQ.h"
#include "../common/data.h"

using namespace dmq;
using namespace std;

/// @brief Receiver receives data from the Sender.
class Receiver : public IMqttReceiveHandler
{
public:
    dmq::MulticastDelegateSafe<void(Data&)> DataCb;

    Receiver(DelegateRemoteId id) :
        m_id(id),
        m_thread("Receiver"),
        m_argStream(ios::in | ios::out | ios::binary)
    {
        // Set the delegate interfaces
        m_recvDelegate.SetStream(&m_argStream);
        m_recvDelegate.SetSerializer(&m_serializer);
        m_recvDelegate = MakeDelegate(this, &Receiver::DataUpdate, id);

        // Set the transport
        m_transport.Create(MqttTransport::Type::SUB);

        m_transport.SetReceiveHandler(this);

        // Create the receiver thread
        m_thread.CreateThread();
    }

    ~Receiver()
    {
        m_thread.ExitThread();
        m_recvDelegate = nullptr;
    }

    void Start()
    {
    }

    void Stop()
    {
        m_thread.ExitThread();
    }

private:
    // Handle incoming data message
    void Receive() override
    {
        // Get incoming data
        DmqHeader header;
        auto arg_data = m_transport.Receive(header);

        // Incoming remote delegate data arrived?
        if (!arg_data.str().empty())
        {
            // Invoke the receiver target function with the sender's argument data
            m_recvDelegate.Invoke(arg_data);
        }
    }

    // Receiver target function called when sender remote delegate is invoked
    void DataUpdate(Data& data)
    {
        // Notify registered callback subscribers
        DataCb(data);
    }

    DelegateRemoteId m_id;
    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_argStream;
    MqttTransport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Receiver remote delegate
    DelegateMemberRemote<Receiver, void(Data&)> m_recvDelegate;
};

#endif
