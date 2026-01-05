#ifndef SENDER_H
#define SENDER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "data.h"

using namespace dmq;
using namespace std;

/// @brief Sender is an active object with a thread. The thread sends data to the 
/// Receiver every time the timer expires. 
class Sender
{
public:
    Sender(DelegateRemoteId id) :
        m_thread("Sender"),
        m_sendDelegate(id),
        m_argStream(ios::in | ios::out | ios::binary)
    {
        // Set the delegate interfaces
        m_sendDelegate.SetStream(&m_argStream);
        m_sendDelegate.SetSerializer(&m_serializer);
        m_sendDelegate.SetDispatcher(&m_dispatcher);
        m_sendDelegate.SetErrorHandler(MakeDelegate(this, &Sender::ErrorHandler));

        // Set the transport
        m_transport.Create(ZeroMqTransport::Type::PUB, "tcp://*:5555");
        m_dispatcher.SetTransport(&m_transport);
        
        // Create the sender thread
        m_thread.CreateThread();
    }

    ~Sender() { Stop(); }

    void Start()
    {
        // Start a timer to send data
        (*m_sendTimer.OnExpired) += MakeDelegate(this, &Sender::Send, m_thread);
        m_sendTimer.Start(std::chrono::milliseconds(50));
    }

    void Stop()
    {
        (*m_sendTimer.OnExpired) -= MakeDelegate(this, &Sender::Send, m_thread);
        m_sendTimer.Stop();
        m_thread.ExitThread();
        m_transport.Close();
    }

    // Send data to the remote
    void Send()
    {
        Data data;
        for (int i = 0; i < 5; i++)
        {
            DataPoint dataPoint;
            dataPoint.x = x++;
            dataPoint.y = y++;
            data.dataPoints.push_back(dataPoint);
        }
        data.msg = "Data Message ";

        m_sendDelegate(data);
    }

private:
    void ErrorHandler(DelegateRemoteId, DelegateError err, DelegateErrorAux)
    {
        if (err != dmq::DelegateError::SUCCESS)
            std::cout << "ErrorHandler " << (int)err << std::endl;
    }

    Thread m_thread;
    Timer m_sendTimer;

    xostringstream m_argStream;
    Dispatcher m_dispatcher;
    ZeroMqTransport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Sender remote delegate
    DelegateMemberRemote<Sender, void(Data&)> m_sendDelegate;

    int x = 0;
    int y = 0;
};

#endif