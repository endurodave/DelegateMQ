#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "AlarmMsg.h"
#include "DataMsg.h"
#include "CommandMsg.h"
#include <msgpack.hpp>
#include <map>
#include <mutex>

static const DelegateMQ::DelegateRemoteId ALARM_MSG_ID = 1;
static const DelegateMQ::DelegateRemoteId DATA_MSG_ID = 2;
static const DelegateMQ::DelegateRemoteId COMMAND_MSG_ID = 3;

// NetworkMgr sends and receives data using a delegate transport implemented
// using ZeroMQ library. Class is thread safe.
class NetworkMgr
{
public:
    // Resister with delegate to receive callbacks
    static DelegateMQ::MulticastDelegateSafe<void(DelegateMQ::DelegateRemoteId, DelegateMQ::DelegateError, DelegateMQ::DelegateErrorAux)> ErrorCb;
    static DelegateMQ::MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> AlarmMsgCb;
    static DelegateMQ::MulticastDelegateSafe<void(CommandMsg&)> CommandMsgCb;
    static DelegateMQ::MulticastDelegateSafe<void(DataMsg&)> DataMsgCb;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    void Create();
    void Start();
    void Stop();

    // Send alarm message to the remote
    void SendAlarmMsg(AlarmMsg& msg, AlarmNote& note);

    // Send command message to the remote
    void SendCommandMsg(CommandMsg& command);

    // Send data message to the remote
    void SendDataMsg(DataMsg& data);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    // Handle errors from DelegateMQ library
    void ErrorHandler(DelegateMQ::DelegateRemoteId id, DelegateMQ::DelegateError error, DelegateMQ::DelegateErrorAux aux);

    // Incoming message handlers
    void RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note);
    void RecvCommandMsg(CommandMsg& command);
    void RecvDataMsg(DataMsg& data);

    // NetworkApp thread of control
    Thread m_thread;

    // Timer for polling
    Timer m_recvTimer;

    // Serialize function argument data into a stream
    xostringstream m_argStream;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    Transport m_transport;

    // Dispatcher using DelegateMQ library
    Dispatcher m_dispatcher;

    // Map each remote delegate ID with an invoker instance
    std::map<DelegateMQ::DelegateRemoteId, DelegateMQ::IRemoteInvoker*> m_receiveIdMap;

    // Receive alarm via remote delegate
    Serializer<void(AlarmMsg&, AlarmNote&)> m_alarmMsgSer;
    DelegateMQ::DelegateMemberRemote<NetworkMgr, void(AlarmMsg&, AlarmNote&)> m_alarmMsgDel;

    // Receive commands via remote delegate
    Serializer<void(CommandMsg&)> m_commandMsgSer;
    DelegateMQ::DelegateMemberRemote<NetworkMgr, void(CommandMsg&)> m_commandMsgDel;

    // Receive data package via remote delegate
    Serializer<void(DataMsg&)> m_dataMsgSer;
    DelegateMQ::DelegateMemberRemote<NetworkMgr, void(DataMsg&)> m_dataMsgDel;
};

#endif
