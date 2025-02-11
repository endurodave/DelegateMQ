/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_argStream(ios::in | ios::out | ios::binary)
{
    // Create the receiver thread
    m_thread.CreateThread();
}

NetworkMgr::~NetworkMgr()
{
    m_thread.ExitThread();
}

void NetworkMgr::Create()
{
    // Reinvoke call onto NetworkMgr thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Create, m_thread)();

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

#ifdef SERVER_APP
    m_transport.Create(Transport::Type::PAIR_SERVER, "tcp://*:5555");
#else
    m_transport.Create(Transport::Type::PAIR_CLIENT, "tcp://localhost:5555");
#endif

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_transport);

    // Set receive async delegates into map
    m_receiveIdMap[ALARM_MSG_ID] = &m_alarmMsgDel;
    m_receiveIdMap[COMMAND_MSG_ID] = &m_commandMsgDel;
    m_receiveIdMap[DATA_MSG_ID] = &m_dataMsgDel;
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
    MsgHeader header;
    auto arg_data = m_transport.Receive(header);
     
    // Incoming remote delegate data arrived?
    if (!arg_data.str().empty())
    {
        auto receiveDelegate = m_receiveIdMap[header.GetId()];
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

void NetworkMgr::ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
{
    ErrorCb(id, error, aux);
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

