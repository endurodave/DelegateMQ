#ifndef RECEIVER_H
#define RECEIVER_H

/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ with remote delegates examples. 

#include "DelegateMQ.h"
#include "data.h"

using namespace DelegateMQ;
using namespace std;

/// @brief Receiver receives data from the Sender.
class Receiver
{
public:
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
        m_transport.Create(Transport::Type::SUB, "tcp://localhost:5555");

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
        // Start a timer to poll data
        m_recvTimer.Expired = MakeDelegate(this, &Receiver::Poll, m_thread);
        m_recvTimer.Start(std::chrono::milliseconds(50));
    }

    void Stop()
    {
        m_recvTimer.Stop();
        m_thread.ExitThread();
        m_recvTimer.Stop();
    }

private:
    // Poll called periodically on m_thread context
    void Poll()
    {
        // Get incoming data
        MsgHeader header;
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
        if (data.dataPoints.size() > 0)
            cout << data.msg << " " << data.dataPoints[0].y << " " << data.dataPoints[0].y << endl;
        else
            cout << "DataUpdate incoming data error!" << endl;
    }

    DelegateRemoteId m_id;
    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_argStream;
    Transport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Receiver remote delegate
    DelegateMemberRemote<Receiver, void(Data&)> m_recvDelegate;
};

#endif
