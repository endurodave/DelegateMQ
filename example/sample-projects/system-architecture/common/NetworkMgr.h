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

/// @brief NetworkMgr sends and receives data using a DelegateMQ transport implemented
/// with ZeroMQ and MessagePack libraries. Class is thread safe. All public APIs are 
/// asynchronous.
/// 
/// @details NetworkMgr has its own internal thread of control. All public APIs are 
/// asynchronous (blocking and non-blocking). Register with ErrorCb or TimeoutCb to 
/// handle errors.
/// 
/// Each socket instance in the ZeroMQ transport layer must only be accessed by a single
/// thread of control. Therefore, when invoking a remote delegate it must be performed on 
/// the internal NetworkMgr thread.
class NetworkMgr
{
public:
    // Remote delegate IDs
    static const dmq::DelegateRemoteId ALARM_MSG_ID;
    static const dmq::DelegateRemoteId DATA_MSG_ID;
    static const dmq::DelegateRemoteId COMMAND_MSG_ID;
    static const dmq::DelegateRemoteId ACTUATOR_MSG_ID;

    // Remote delegate type definitions
    using AlarmFunc = void(AlarmMsg&, AlarmNote&);
    using AlarmDel = dmq::MulticastDelegateSafe<AlarmFunc>;
    using CommandFunc = void(CommandMsg&);
    using CommandDel = dmq::MulticastDelegateSafe<CommandFunc>;
    using DataFunc = void(DataMsg&);
    using DataDel = dmq::MulticastDelegateSafe<DataFunc>;
    using ActuatorFunc = void(ActuatorMsg&);
    using ActuatorDel = dmq::MulticastDelegateSafe<ActuatorFunc>;

    // Receive callbacks by registering with a container
    static dmq::MulticastDelegateSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCb;
    static dmq::MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId, TransportMonitor::Status)> SendStatusCb;
    static AlarmDel AlarmMsgCb;
    static CommandDel CommandMsgCb;
    static DataDel DataMsgCb;
    static ActuatorDel ActuatorMsgCb;

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
    // the actuator message or timeout occurs.
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

    // Map each remote delegate ID to an invoker instance
    std::map<dmq::DelegateRemoteId, dmq::IRemoteInvoker*> m_receiveIdMap;

    // Alarm message remote delegate
    Serializer<AlarmFunc> m_alarmMsgSer;
    dmq::DelegateMemberRemote<AlarmDel, AlarmFunc> m_alarmMsgDel;

    // Command message remote delegate
    Serializer<CommandFunc> m_commandMsgSer;
    dmq::DelegateMemberRemote<CommandDel, CommandFunc> m_commandMsgDel;

    // Data message remote delegate
    Serializer<DataFunc> m_dataMsgSer;
    dmq::DelegateMemberRemote<DataDel, DataFunc> m_dataMsgDel;

    // Actuator message remote delegate
    Serializer<ActuatorFunc> m_actuatorMsgSer;
    dmq::DelegateMemberRemote<ActuatorDel, ActuatorFunc> m_actuatorMsgDel;
};

#endif
