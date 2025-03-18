#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include "RemoteIds.h"
#include "AlarmMsg.h"
#include "DataMsg.h"
#include "CommandMsg.h"
#include "ActuatorMsg.h"
#include "RemoteEndpoint.h"
#include <map>

/// @brief NetworkMgr sends and receives data using a DelegateMQ transport implemented
/// with ZeroMQ and MessagePack libraries. Class is thread safe. All public APIs are 
/// asynchronous.
/// 
/// @details NetworkMgr has its own internal thread of control. All public APIs are 
/// asynchronous (blocking and non-blocking). Register with ErrorCb or SendStatusCb to 
/// handle success or errors.
/// 
/// Each socket instance in the ZeroMQ transport layer must only be accessed by a single
/// thread of control. Therefore, when invoking a remote delegate it must be performed on 
/// the internal NetworkMgr thread.
class NetworkMgr
{
public:
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

    // Process message timeouts 
    void Timeout();

    // Handle errors from DelegateMQ library
    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux);

    // Handle send message status callbacks
    void SendStatusHandler(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status);

    // NetworkApp thread of control
    Thread m_thread;

    // Message timeout processing thread
    Thread m_timeoutThread;

    // Timer for polling
    Timer m_recvTimer;

    // Timer for processing message timeouts
    Timer m_timeoutTimer;

    // Delegate dispatcher 
    Dispatcher m_dispatcher;

    // Transport using ZeroMQ library. Only call transport from NetworkMsg thread.
    ZeroMqTransport m_transport;

    // Monitor message timeouts
    TransportMonitor m_transportMonitor;

    // Map each remote delegate ID to an invoker instance
    std::map<dmq::DelegateRemoteId, dmq::IRemoteInvoker*> m_receiveIdMap;

    // Remote delegate endpoints used to send/receive messages
    RemoteEndpoint<AlarmDel, AlarmFunc> m_alarmMsgDel;
    RemoteEndpoint<CommandDel, CommandFunc> m_commandMsgDel;
    RemoteEndpoint<DataDel, DataFunc> m_dataMsgDel;
    RemoteEndpoint<ActuatorDel, ActuatorFunc> m_actuatorMsgDel;
};

#endif
