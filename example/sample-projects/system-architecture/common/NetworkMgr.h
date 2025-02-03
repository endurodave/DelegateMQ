#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "DataPackage.h"
#include "Command.h"
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
    static MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> Error;
    static MulticastDelegateSafe<void(Command&)> CommandRecv;
    static MulticastDelegateSafe<void(DataPackage&)> DataPackageRecv;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    void Create();
    void Start();
    void Stop();

    // Send command to the remote
    void SendCommand(Command& command);

    // Send data package to the remote
    void SendDataPackage(DataPackage& data);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    // Handle errors from DelegateMQ library
    void ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux);

    // Incoming message handlers
    void RecvCommand(Command& command);
    void RecvSensorData(DataPackage& data);

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
    Serializer<void(Command&)> m_commandSer;
    DelegateMemberRemote<NetworkMgr, void(Command&)> m_commandDel;

    // Receive data package via remote delegate
    Serializer<void(DataPackage&)> m_dataPackageSer;
    DelegateMemberRemote<NetworkMgr, void(DataPackage&)> m_dataPackageDel;
};

#endif
