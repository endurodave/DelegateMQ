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
#include <future>

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
/// 
/// Three types of remote APIs are implemented:
/// 
/// 1. Non-blocking APIs send the message without waiting for success or failure.
/// 
/// 2. Blocking APIs wait for the remote to ack the message or time.
/// 
/// 3. Future APIs return immediately and a std::future is used to capture the return 
/// value later.
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

    // Error delegate is invoked on success or failure of message send
    static dmq::MulticastDelegateSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCb;

    // Send status delegate invoked when the receiver acknowleges the sent message
    static dmq::MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId, TransportMonitor::Status)> SendStatusCb;

    // Incoming message callback delegates
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

    // Start listening for messages 
    void Start();

    // Stop listening for messages
    void Stop();

    // Send alarm message to the remote
    void SendAlarmMsg(AlarmMsg& msg, AlarmNote& note);

    // Send alarm message to the remote and wait for response
    bool SendAlarmMsgWait(AlarmMsg& msg, AlarmNote& note);

    // Send command message to the remote
    void SendCommandMsg(CommandMsg& command);

    // Send command message to the remote and wait for response
    bool SendCommandMsgWait(CommandMsg& command);

    // Send data message to the remote
    void SendDataMsg(DataMsg& data);

    // Send actuator position command. Blocking call returns after remote receives 
    // the actuator message or timeout occurs.
    bool SendActuatorMsgWait(ActuatorMsg& msg);

    // Alternative function that relies upon RemoteInvoke template function. Blocking 
    // call returns after remote receives the actuator message or timeout occurs.
    bool SendActuatorMsgWaitAlt(ActuatorMsg& msg);

    // Send actuator message to the remote and block using a future return
    std::future<bool> SendActuatorMsgFuture(ActuatorMsg& msg);

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

    // Timer for polling
    Timer m_recvTimer;

    // Timer for processing message timeouts
    Timer m_timeoutTimer;

    // Delegate dispatcher 
    Dispatcher m_dispatcher;

    // Transport using ZeroMQ library. Only call transport from NetworkMgr thread.
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

    // Generic helper function for invoking any remote delegate. The execution order sequence 
    // numbers shown below.
    template <class TClass, class RetType, class... Args>
    bool RemoteInvoke(RemoteEndpoint<TClass, RetType(Args...)>& endpointDel, Args&&... args) 
    {
        // If caller is not executing on m_thread
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        {
            // *** Caller's thread executes this control branch ***

            std::atomic<bool> success(false);
            bool complete = false;
            std::mutex mtx;
            std::condition_variable cv;
            dmq::DelegateRemoteId remoteId = endpointDel.GetRemoteId();

            // 7. Callback lambda handler for transport monitor when remote receives message (success or failure).
            //    Callback context is m_thread.
            SendStatusCallback statusCb = [&success, &complete, &remoteId, &cv, &mtx](dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status) {
                if (id == remoteId) {
                    {
                        std::lock_guard<std::mutex> lock(mtx);
                        complete = true;
                        if (status == TransportMonitor::Status::SUCCESS)
                            success.store(true);   // Client received ActuatorMsg
                    }
                    cv.notify_one();
                }
            };

            // 1. Register for send status callback (success or failure)
            m_transportMonitor.SendStatusCb += dmq::MakeDelegate(statusCb);

            // Lambda that binds and forwards arguments to RemoteInvoke with specified endpoint
            std::function<RetType(Args...)> func = [this, &endpointDel](Args&&... args) {
                return this->RemoteInvoke<TClass, RetType, Args...>(
                    endpointDel, std::forward<Args>(args)...);
                };

            // 2. Async reinvoke func on m_thread context and wait for send to complete
            auto del = dmq::MakeDelegate(func, m_thread, SEND_TIMEOUT);
            auto retVal = del.AsyncInvoke(std::forward<Args>(args)...);

            // 5. Check that the remote delegate send succeeded
            if (retVal.has_value() &&      // If async function call succeeded AND
                retVal.value() == true)    // async function call returned true
            {
                // 6. Wait for statusCb callback to be invoked. Callback invoked when the 
                //    receiver ack's the message or timeout.
                std::unique_lock<std::mutex> lock(mtx);
                while (!complete)
                {
                    if (cv.wait_for(lock, RECV_TIMEOUT) == std::cv_status::timeout)
                    {
                        // Timeout waiting for remote delegate message ack
                        std::cout << "Timeout RemoteInvoke()" << std::endl;
                    }
                }
            }

            // 8. Unregister from status callback
            m_transportMonitor.SendStatusCb -= dmq::MakeDelegate(statusCb);

            // 9. Return the blocking async function invoke status to caller
            return success.load();
        }
        else
        {
            // *** NetworkMgr::m_thread executes this control branch ***

            // 3. Send actuator command to remote on m_thread
            endpointDel(std::forward<Args>(args)...);

            // 4. Check if send succeeded
            if (endpointDel.GetError() == DelegateError::SUCCESS)
            {
                return true;
            }

            std::cout << "Send failed!" << std::endl;
            return false;
        }
    }
};

#endif
