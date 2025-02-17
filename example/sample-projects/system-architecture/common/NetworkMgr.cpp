/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId)> NetworkMgr::TimeoutCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_argStream(ios::in | ios::out | ios::binary),
    m_transportMonitor(std::chrono::milliseconds(2000))
{
    // Create the receiver thread
    m_thread.CreateThread();
}

NetworkMgr::~NetworkMgr()
{
    m_thread.ExitThread();
}

int NetworkMgr::Create()
{
    // Reinvoke call onto NetworkMgr thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Create, m_thread, WAIT_INFINITE)();

    // Setup the remote delegate interfaces
    m_alarmMsgDel.SetStream(&m_argStream);
    m_alarmMsgDel.SetSerializer(&m_alarmMsgSer);
    m_alarmMsgDel.SetDispatcher(&m_dispatcher);
    m_alarmMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_alarmMsgDel = MakeDelegate(this, &NetworkMgr::RecvAlarmMsg, ALARM_MSG_ID);

    m_dataMsgDel.SetStream(&m_argStream);
    m_dataMsgDel.SetSerializer(&m_dataMsgSer);
    m_dataMsgDel.SetDispatcher(&m_dispatcher);
    m_dataMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_dataMsgDel = MakeDelegate(this, &NetworkMgr::RecvDataMsg, DATA_MSG_ID);

    m_commandMsgDel.SetStream(&m_argStream);
    m_commandMsgDel.SetSerializer(&m_commandMsgSer);
    m_commandMsgDel.SetDispatcher(&m_dispatcher);
    m_commandMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_commandMsgDel = MakeDelegate(this, &NetworkMgr::RecvCommandMsg, COMMAND_MSG_ID);

    int err = 0;
#ifdef SERVER_APP
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_SERVER, "tcp://*:5555");
#else
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://localhost:5555");   // Connect to local server
    //err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://192.168.0.123:5555"); // Connect to remote server
#endif

    m_transportMonitor.Timeout += dmq::MakeDelegate(this, &NetworkMgr::TimeoutHandler);
    m_transport.SetTransportMonitor(&m_transportMonitor);

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_transport);

    // Set receive async delegates into map
    m_receiveIdMap[ALARM_MSG_ID] = &m_alarmMsgDel;
    m_receiveIdMap[COMMAND_MSG_ID] = &m_commandMsgDel;
    m_receiveIdMap[DATA_MSG_ID] = &m_dataMsgDel;

    return err;
}

void NetworkMgr::Start()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Start, m_thread)();

    // Start a timer to poll data
    m_recvTimer.Expired = MakeDelegate(this, &NetworkMgr::Poll, m_thread);
    m_recvTimer.Start(std::chrono::milliseconds(50));
}

void NetworkMgr::Stop()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Stop, m_thread)();

    m_recvTimer.Stop();
    m_dispatcher.SetTransport(nullptr);
    m_transport.Close();
    //m_transport.Destroy();
}

void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);

    // Send alarm to remote. 
    m_alarmMsgDel(msg, note);
}

void NetworkMgr::SendCommandMsg(CommandMsg& command)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendCommandMsg, m_thread)(command);

    // Send data to remote. 
    m_commandMsgDel(command);
}

void NetworkMgr::SendDataMsg(DataMsg& data)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendDataMsg, m_thread)(data);

    // Send data to remote. 
    m_dataMsgDel(data);
}

// Poll called periodically on m_thread context
void NetworkMgr::Poll()
{
    // Get incoming data
    DmqHeader header;
    auto arg_data = m_transport.Receive(header);

    // Incoming remote delegate data arrived?
    if (!arg_data.str().empty())
    {
        if (header.GetId() != ACK_REMOTE_ID)
        {
            // Process remote delegate message
            auto receiveDelegate = m_receiveIdMap[header.GetId()];
            if (receiveDelegate)
            {
                // Invoke the receiver target function with the sender's argument data
                receiveDelegate->Invoke(arg_data);
            }
            else
            {
                cout << "Receiver delegate not found!" << endl;
            }
        }
    }

    // Preiodically process message timeout handling
    m_transportMonitor.Process();
}

void NetworkMgr::ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
{
    ErrorCb(id, error, aux);
}

void NetworkMgr::TimeoutHandler(uint16_t seqNum, dmq::DelegateRemoteId id)
{
    TimeoutCb(seqNum, id);
}

void NetworkMgr::RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    AlarmMsgCb(msg, note);
}

void NetworkMgr::RecvCommandMsg(CommandMsg& command)
{
    CommandMsgCb(command);
}

void NetworkMgr::RecvDataMsg(DataMsg& data)
{
    DataMsgCb(data);
}

