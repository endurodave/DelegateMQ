#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateLib.h"
#include "IInvoker.h"
#include "Thread.h"
#include "Transport.h"
#include "Dispatcher.h"
#include "Serializer.h"
#include "Timer.h"
#include <msgpack.hpp>
#include <map>

static const DelegateRemoteId SENSOR_DATA = 1;

class MsgHeader
{
public:
    DelegateLib::DelegateRemoteId id = 0;
    MSGPACK_DEFINE(id);
};

class SensorData
{
public:
    float value = 0;
    MSGPACK_DEFINE(value);
};

class NetworkMgr
{
public:
    static MulticastDelegateSafe<void(SensorData&)> SensorDataRecv;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    void Start();

    void Stop();

    void SendSensorData(SensorData& data);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    void ErrorHandler(DelegateError, DelegateErrorAux);

    void RecvSensorData(SensorData& data);

    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_argStream;
    Transport m_transport;
    Dispatcher m_dispatcher;

    std::map<DelegateLib::DelegateRemoteId, DelegateLib::IRemoteInvoker*> m_receiveIdMap;

    Serializer<void(SensorData&)> m_sensorDataSer;
    DelegateMemberRemote<NetworkMgr, void(SensorData&)> m_sensorDataDel;


};

#endif
