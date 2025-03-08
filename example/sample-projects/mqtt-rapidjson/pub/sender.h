#ifndef SENDER_H
#define SENDER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "../common/data.h"

using namespace dmq;
using namespace std;

typedef std::function<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCallback;

/// @brief Sender is an active object with a thread. The thread sends data to the 
/// Receiver every time the timer expires. 
class Sender
{
public:
    Sender() :
        m_thread("Sender"),
        m_sendDelegate(Data::DATA_ID),
        m_argStream(ios::in | ios::out | ios::binary)
    {
        // Set the delegate interfaces
        m_sendDelegate.SetStream(&m_argStream);
        m_sendDelegate.SetSerializer(&m_serializer);
        m_sendDelegate.SetDispatcher(&m_dispatcher);
        m_sendDelegate.SetErrorHandler(MakeDelegate(this, &Sender::ErrorHandler));

        // Set the transport
        m_transport.Create(MqttTransport::Type::PUB);
        m_dispatcher.SetTransport(&m_transport);
        
        // Create the sender thread
        m_thread.CreateThread();
    }

    ~Sender() { m_thread.ExitThread(); }

    void Start()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Sender::Start, m_thread)();

        // Start a timer to send data
        m_sendTimer.Expired = MakeDelegate(this, &Sender::Send, m_thread);
        //m_sendTimer.Expired = MakeDelegate(this, &Sender::SendV2, m_thread);
        m_sendTimer.Start(std::chrono::milliseconds(500));
    }

    void Stop()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Sender::Stop, m_thread)();

        m_sendTimer.Stop();
        m_transport.Close();
    }

    // Send data to the remote
    void Send()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Sender::Send, m_thread)();

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

    // Send data to the remote and capture send success or error with lambda
    void SendV2()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Sender::SendV2, m_thread)();

        bool success = false;

        // Callback to capture invoke m_sendDelegate() success or error
        ErrorCallback errorCb = [&success](dmq::DelegateRemoteId id, dmq::DelegateError err, dmq::DelegateErrorAux aux) {
            if (id == Data::DATA_ID) {
                // Send success?
                if (err == dmq::DelegateError::SUCCESS)
                    success = true;
            }
        };

        // Register for callback
        m_sendDelegate.SetErrorHandler(MakeDelegate(errorCb));

        // Create data
        Data data;
        for (int i = 0; i < 5; i++)
        {
            DataPoint dataPoint;
            dataPoint.x = x++;
            dataPoint.y = y++;
            data.dataPoints.push_back(dataPoint);
        }
        data.msg = "Data Message ";

        // Send data to remote receiver
        m_sendDelegate(data);

        // Send complete; unregister from callback
        m_sendDelegate.ClearErrorHandler();
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
    MqttTransport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Sender remote delegate
    DelegateMemberRemote<Sender, void(Data&)> m_sendDelegate;

    int x = 0;
    int y = 0;
};

#endif