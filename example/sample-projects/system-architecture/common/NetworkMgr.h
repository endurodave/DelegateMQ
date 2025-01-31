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
// using ZeroMQ library.
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

    void Start();
    void Stop();
    void SendCommand(Command& command);
    void SendDataPackage(DataPackage& data);

    // Generic send function
    template <typename T>
    void SendMessage(T& message);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    void ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux);

    void RecvCommand(Command& command);
    void RecvSensorData(DataPackage& data);

    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_argStream;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    Transport m_transport;

    Dispatcher m_dispatcher;

    std::map<DelegateMQ::DelegateRemoteId, DelegateMQ::IRemoteInvoker*> m_receiveIdMap;

    Serializer<void(Command&)> m_commandSer;
    DelegateMemberRemote<NetworkMgr, void(Command&)> m_commandDel;

    Serializer<void(DataPackage&)> m_dataPackageSer;
    DelegateMemberRemote<NetworkMgr, void(DataPackage&)> m_dataPackageDel;
};

#endif
