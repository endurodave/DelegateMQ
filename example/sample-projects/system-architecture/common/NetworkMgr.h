#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "DataMsg.h"
#include "CommandMsg.h"
#include <msgpack.hpp>
#include <map>
#include <mutex>

static const DelegateRemoteId DATA_PACKAGE_ID = 1;
static const DelegateRemoteId COMMAND_ID = 2;

// NetworkMgr sends and receives data using a delegate transport implemented
// using ZeroMQ library. Class is thread safe.
class NetworkMgr
{
public:
    // Resister with delegate to receive callbacks
    static MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> ErrorCb;
    static MulticastDelegateSafe<void(CommandMsg&)> CommandMsgCb;
    static MulticastDelegateSafe<void(DataMsg&)> DataMsgCb;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    void Create();
    void Start();
    void Stop();

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
    void ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux);

    // Incoming message handlers
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

    // Receive commands via remote delegate
    Serializer<void(CommandMsg&)> m_commandMsgSer;
    DelegateMemberRemote<NetworkMgr, void(CommandMsg&)> m_commandMsgDel;

    // Receive data package via remote delegate
    Serializer<void(DataMsg&)> m_dataMsgSer;
    DelegateMemberRemote<NetworkMgr, void(DataMsg&)> m_dataMsgDel;
};

#endif
