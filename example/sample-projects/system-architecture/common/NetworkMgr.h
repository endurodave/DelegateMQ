#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "AlarmMsg.h"
#include "DataMsg.h"
#include "CommandMsg.h"
#include "ActuatorMsg.h"
#include <msgpack.hpp>
#include <map>
#include <mutex>

static const dmq::DelegateRemoteId ALARM_MSG_ID = 1;
static const dmq::DelegateRemoteId DATA_MSG_ID = 2;
static const dmq::DelegateRemoteId COMMAND_MSG_ID = 3;
static const dmq::DelegateRemoteId ACTUATOR_MSG_ID = 4;

/// @brief NetworkMgr sends and receives data using a delegate transport implemented
/// using ZeroMQ library. Class is thread safe. All public APIs are asynchronous.
/// 
/// @details NetworkMgr has its own internal thread of control. All DelegateMemberRemote<>
/// must only be invoked on the internal thread. All public APIs are asynchronous (blocking 
/// and non-blocking). Register with ErrorCb or TimeoutCb to handle errors.
class NetworkMgr
{
public:
    // Resister with delegate to receive callbacks
    static dmq::MulticastDelegateSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCb;
    static dmq::MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId, TransportMonitor::Status)> SendStatusCb;
    static dmq::MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> AlarmMsgCb;
    static dmq::MulticastDelegateSafe<void(CommandMsg&)> CommandMsgCb;
    static dmq::MulticastDelegateSafe<void(DataMsg&)> DataMsgCb;
    static dmq::MulticastDelegateSafe<void(ActuatorMsg&)> ActuatorMsgCb;

    static NetworkMgr& Instance()
    {
        static NetworkMgr instance;
        return instance;
    }

    // Create the instance. Called once at startup.
    int Create();

    // Start lisiting for messages 
    void Start();

    // Stop lisining for messages
    void Stop();

    // Send alarm message to the remote
    void SendAlarmMsg(AlarmMsg& msg, AlarmNote& note);

    // Send command message to the remote
    void SendCommandMsg(CommandMsg& command);

    // Send data message to the remote
    void SendDataMsg(DataMsg& data);

    // Send actuator position command. Blocking call returns after remote receives 
    // the actuator message.
    bool SendActuatorMsgWait(ActuatorMsg& msg);

private:
    NetworkMgr();
    ~NetworkMgr();

    // Poll called periodically on m_thread context
    void Poll();

    // Process message timeout loop
    void Timeout();

    // Handle errors from DelegateMQ library
    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux);

    // Handle send message status callbacks
    void SendStatusHandler(uint16_t seqNum, dmq::DelegateRemoteId id, TransportMonitor::Status status);

    // Incoming message handlers
    void RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note);
    void RecvCommandMsg(CommandMsg& command);
    void RecvDataMsg(DataMsg& data);
    void RecvActuatorMsg(ActuatorMsg& msg);

    // NetworkApp thread of control
    Thread m_thread;

    // Message timeout processing thread
    Thread m_timeoutThread;

    // Timer for polling
    Timer m_recvTimer;

    // Timer for processing message timeouts
    Timer m_timeoutTimer;

    // Serialize function argument data into a stream
    xostringstream m_argStream;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    ZeroMqTransport m_transport;

    // Monitor messages for timeout
    TransportMonitor m_transportMonitor;

    // Dispatcher using DelegateMQ library
    Dispatcher m_dispatcher;

    // Map each remote delegate ID with an invoker instance
    std::map<dmq::DelegateRemoteId, dmq::IRemoteInvoker*> m_receiveIdMap;

    // Receive alarm via remote delegate
    Serializer<void(AlarmMsg&, AlarmNote&)> m_alarmMsgSer;
    dmq::DelegateMemberRemote<NetworkMgr, void(AlarmMsg&, AlarmNote&)> m_alarmMsgDel;

    // Receive commands via remote delegate
    Serializer<void(CommandMsg&)> m_commandMsgSer;
    dmq::DelegateMemberRemote<NetworkMgr, void(CommandMsg&)> m_commandMsgDel;

    // Receive data package via remote delegate
    Serializer<void(DataMsg&)> m_dataMsgSer;
    dmq::DelegateMemberRemote<NetworkMgr, void(DataMsg&)> m_dataMsgDel;

    // Receive actuator message via remote delegate
    Serializer<void(ActuatorMsg&)> m_actuatorMsgSer;
    dmq::DelegateMemberRemote<NetworkMgr, void(ActuatorMsg&)> m_actuatorMsgDel;
};

#endif
