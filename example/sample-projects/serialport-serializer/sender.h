#ifndef SENDER_H
#define SENDER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "predef/util/RetryMonitor.h"
#include "predef/util/ReliableTransport.h"
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
        m_argStream(ios::in | ios::out | ios::binary),
        m_transportMonitor(chrono::milliseconds(1000)),      // 1s timeout
        m_retryMonitor(m_transport, m_transportMonitor, 3),  // 3 retries
        m_reliableTransport(m_transport, m_retryMonitor)
    {
        // Link monitors
        m_transport.SetTransportMonitor(&m_transportMonitor);
        m_dispatcher.SetTransport(&m_reliableTransport);

        // Subscribe to the Status Signal
        // We connect our callback to the signal shared_ptr provided by TransportMonitor
        if (m_transportMonitor.OnSendStatus) {
            m_statusHandle = (*m_transportMonitor.OnSendStatus).Connect(MakeDelegate(this, &Sender::OnSendStatus));
        }

        // Set the delegate interfaces
        m_sendDelegate.SetStream(&m_argStream);
        m_sendDelegate.SetSerializer(&m_serializer);
        m_sendDelegate.SetDispatcher(&m_dispatcher);
        m_sendDelegate.SetErrorHandler(MakeDelegate(this, &Sender::ErrorHandler));

        // Create the transport
        if (m_transport.Create("COM1", 115200) != 0) {
            std::cout << "Error: Failed to open COM1" << std::endl;
        }

        // Create the sender thread
        m_thread.CreateThread();
    }

    ~Sender() { Stop(); }

    void Start()
    {
        // Start a timer to send data
        (*m_sendTimer.OnExpired) += MakeDelegate(this, &Sender::Send, m_thread);
        m_sendTimer.Start(std::chrono::milliseconds(50));

        // Start a timer to poll the transport monitor for timeouts
        (*m_monitorTimer.OnExpired) += MakeDelegate(&m_transportMonitor, &TransportMonitor::Process, m_thread);
        m_monitorTimer.Start(std::chrono::milliseconds(100));
    }

    void Stop()
    {
        m_sendTimer.Stop();
        m_monitorTimer.Stop();
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
        data.msg = "Data Message:";

        DataAux dataAux;
        dataAux.auxMsg = "Aux Message";

        // 1. Send (Dispatcher -> Adapter -> RetryMonitor -> SerialTransport)
        m_sendDelegate(data, dataAux);

        // 2. Poll for ACKs
        // If you don't do this, the ACK sits in the buffer and you Timeout.
        xstringstream ss;
        DmqHeader h;
        m_transport.Receive(ss, h);

        // 3. Process timeouts (Can also be done via m_monitorTimer, but safe to do here too)
        m_transportMonitor.Process();
    }

private:
    void ErrorHandler(DelegateRemoteId, DelegateError err, DelegateErrorAux)
    {
        if (err != dmq::DelegateError::SUCCESS)
            std::cout << "ErrorHandler " << (int)err << std::endl;
    }

    void OnSendStatus(DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)
    {
        if (status == TransportMonitor::Status::TIMEOUT)
        {
            std::cout << ">> TIMEOUT! Remote: " << id << " Seq: " << seqNum << " expired." << std::endl;
        }
        else if (status == TransportMonitor::Status::SUCCESS)
        {
            // Optional: Log success if you want to see ACKs coming in
            // std::cout << ">> SUCCESS. Seq: " << seqNum << " delivered." << std::endl;
        }
    }

    Thread m_thread;
    Timer m_sendTimer;
    Timer m_monitorTimer; // Polls the reliability layer

    xostringstream m_argStream;
    Dispatcher m_dispatcher;

    SerialTransport m_transport;
    TransportMonitor m_transportMonitor;
    RetryMonitor m_retryMonitor;
    ReliableTransport m_reliableTransport;

    Serializer<void(Data&, DataAux&)> m_serializer;

    // Sender remote delegate
    DelegateMemberRemote<Sender, void(Data&, DataAux&)> m_sendDelegate;

    dmq::Connection m_statusHandle;

    int x = 0;
    int y = 0;
};

#endif
