/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace DelegateLib;
using namespace std;

MulticastDelegateSafe<void(SensorData&)> NetworkMgr::SensorDataRecv;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_argStream(ios::in | ios::out | ios::binary)
{
    // Set the delegate interfaces
    m_sensorDataDel.SetStream(&m_argStream);
    m_sensorDataDel.SetSerializer(&m_sensorDataSer);
    m_sensorDataDel.SetDispatcher(&m_dispatcher);
    m_sensorDataDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_sensorDataDel = MakeDelegate(this, &NetworkMgr::RecvSensorData, SENSOR_DATA);

    // Set the transport
#ifdef SERVER_APP
    m_transport.Create(Transport::Type::PUB, "tcp://*:5555");
    m_dispatcher.SetTransport(&m_transport);
#else
    m_transport.Create(Transport::Type::SUB, "tcp://localhost:5555");

    // Client receive async delegates
    m_receiveIdMap[SENSOR_DATA] = &m_sensorDataDel;
#endif

    // Create the receiver thread
    m_thread.CreateThread();
}

NetworkMgr::~NetworkMgr()
{
    m_thread.ExitThread();
}

void NetworkMgr::Start()
{
    // Start a timer to poll data
    m_recvTimer.Expired = MakeDelegate(this, &NetworkMgr::Poll, m_thread);
    m_recvTimer.Start(std::chrono::milliseconds(50));
}

void NetworkMgr::Stop()
{
    m_recvTimer.Stop();
    m_thread.ExitThread();
    m_recvTimer.Stop();
}

void NetworkMgr::SendSensorData(SensorData& data)
{
    m_sensorDataDel(data);
}

// Poll called periodically on m_thread context
void NetworkMgr::Poll()
{
    // Get incoming data
    DelegateRemoteId id = INVALID_REMOTE_ID;
    auto arg_data = m_transport.Receive(id);

    // Incoming remote delegate data arrived?
    if (!arg_data.str().empty())
    {
        auto receiveDelegate = m_receiveIdMap[id];
        if (receiveDelegate)
        {
            // Invoke the receiver target function with the sender's argument data
            receiveDelegate->Invoke(arg_data);
        }
        else
        {
            cout << "Received delegate not found!" << endl;
        }
    }
}

void NetworkMgr::ErrorHandler(DelegateError, DelegateErrorAux)
{
    ASSERT_TRUE(0);
}

void NetworkMgr::RecvSensorData(SensorData& data)
{
    SensorDataRecv(data);
}

