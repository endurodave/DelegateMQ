#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "DataPackage.h"
#include <msgpack.hpp>
#include <map>
#include <mutex>

static const DelegateRemoteId DATA_PACKAGE_ID = 1;

class MsgHeader
{
public:
    DelegateMQ::DelegateRemoteId id = 0;
    MSGPACK_DEFINE(id);
};

// NetworkMgr sends and receives data using a delegate transport implemented
// using ZeroMQ library.
class NetworkMgr
{
public:
    static MulticastDelegateSafe<void(DataPackage&)> DataPackageRecv;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    void Start();

    void Stop();

    void SendDataPackage(DataPackage& data);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    void ErrorHandler(DelegateError, DelegateErrorAux);

    void RecvSensorData(DataPackage& data);

    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_argStream;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    Transport m_transportSend;
    Transport m_transportRecv;

    Dispatcher m_dispatcher;

    std::map<DelegateMQ::DelegateRemoteId, DelegateMQ::IRemoteInvoker*> m_receiveIdMap;

    Serializer<void(DataPackage&)> m_dataPackageSer;
    DelegateMemberRemote<NetworkMgr, void(DataPackage&)> m_dataPackageDel;
};

#endif
