/// @file NetworkMgr.h
/// @brief Application-specific network manager for Alarms, Commands, and Data.
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#ifndef NETWORK_MGR_H
#define NETWORK_MGR_H

#include "DelegateMQ.h" 
#include "RemoteIds.h"
#include "AlarmMsg.h"
#include "DataMsg.h"
#include "CommandMsg.h"
#include "ActuatorMsg.h"

/// @brief NetworkMgr sends and receives data using a DelegateMQ transport implemented
/// with Windows UDP sockets and msg_serialize. Class is thread safe. All public APIs are 
/// asynchronous.
/// 
/// @details NetworkMgr inherits from NetworkEngine, which manages the internal thread 
/// of control. All public APIs are asynchronous (blocking and non-blocking). Register 
/// with OnError or OnSendStatus to handle success or errors.
/// 
/// The underlying UDP transport layer managed by NetworkEngine is accessed only by a 
/// single internal thread. Therefore, when invoking a remote delegate, the call is 
/// automatically dispatched to the internal NetworkEngine thread.
/// 
/// **Key Responsibilities:**
/// * **Asynchronous Communication:** Exposes a fully thread-safe, asynchronous public API for network operations, 
///   utilizing an internal thread managed by `NetworkEngine` to handle all I/O.
/// * **Transport Abstraction:** Implements specific UDP transport logic (using Windows or Linux sockets) while abstracting 
///   these details from the application logic. Two sockets are created: one for sending and one for receiving.
/// * **Message Dispatching:** Automatically marshals all outgoing remote delegate invocations to the internal 
///   network thread, ensuring safe single-threaded access to the underlying UDP resources.
/// * **Invocation Modes:** Support for three distinct remote invocation patterns:
///     1. *Fire-and-Forget (Non-blocking):* Sends messages immediately without waiting for confirmation.
///     2. *Synchronous Wait (Blocking):* Blocks the calling thread until an acknowledgment (ACK) is received or a timeout occurs.
///     3. *Future-based:* Returns a `std::future` immediately, allowing retrieval of the result at a later time.
/// * **Error & Status Reporting:** Provides registration points (`OnNetworkError`, `OnSendStatus`) for clients to subscribe 
///   to transmission results and error notifications.
class NetworkMgr : public NetworkEngine
{
public:
    // Public Signal Types
    // Clients Connect() to these safely using RAII.
    using AlarmSignal = dmq::SignalSafe<void(AlarmMsg&, AlarmNote&)>;
    using CommandSignal = dmq::SignalSafe<void(CommandMsg&)>;
    using DataSignal = dmq::SignalSafe<void(DataMsg&)>;
    using ActuatorSignal = dmq::SignalSafe<void(ActuatorMsg&)>;

    using ErrorSignal = dmq::SignalSafe<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)>;
    using SendStatusSignal = dmq::SignalSafe<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)>;

    static inline std::shared_ptr<ErrorSignal>      OnNetworkError = std::make_shared<ErrorSignal>();
    static inline std::shared_ptr<SendStatusSignal> OnSendStatus = std::make_shared<SendStatusSignal>();

    static inline std::shared_ptr<AlarmSignal>      OnAlarm = std::make_shared<AlarmSignal>();
    static inline std::shared_ptr<CommandSignal>    OnCommand = std::make_shared<CommandSignal>();
    static inline std::shared_ptr<DataSignal>       OnData = std::make_shared<DataSignal>();
    static inline std::shared_ptr<ActuatorSignal>   OnActuator = std::make_shared<ActuatorSignal>();

    static NetworkMgr& Instance() { static NetworkMgr instance; return instance; }

    int Create();

    // Send Functions (non-blocking, blocking, and future)
    void SendAlarmMsg(AlarmMsg& msg, AlarmNote& note);
    bool SendAlarmMsgWait(AlarmMsg& msg, AlarmNote& note);
    void SendCommandMsg(CommandMsg& command);
    bool SendCommandMsgWait(CommandMsg& command);
    void SendDataMsg(DataMsg& data);
    bool SendDataMsgWait(DataMsg& data);
    bool SendActuatorMsgWait(ActuatorMsg& msg);
    std::future<bool> SendActuatorMsgFuture(ActuatorMsg& msg);

protected:
    // Override base class hooks to fire our static Signals
    void OnError(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux) override;
    void OnStatus(dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) override;

private:
    NetworkMgr();
    ~NetworkMgr() = default;

    // Specific endpoints
    RemoteEndpoint<dmq::MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)>, void(AlarmMsg&, AlarmNote&)> m_alarmMsgDel;
    RemoteEndpoint<dmq::MulticastDelegateSafe<void(CommandMsg&)>, void(CommandMsg&)> m_commandMsgDel;
    RemoteEndpoint<dmq::MulticastDelegateSafe<void(DataMsg&)>, void(DataMsg&)> m_dataMsgDel;
    RemoteEndpoint<dmq::MulticastDelegateSafe<void(ActuatorMsg&)>, void(ActuatorMsg&)> m_actuatorMsgDel;
};

#endif