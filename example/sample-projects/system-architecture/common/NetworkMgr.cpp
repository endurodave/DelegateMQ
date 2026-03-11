#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

NetworkMgr::NetworkMgr()
{
}

int NetworkMgr::Create()
{
    // Initialize one RemoteChannel per message signature.
    // Each channel owns its Dispatcher (wired to the shared send transport)
    // and its serialization stream, and borrows the per-type serializer.
    m_alarmChannel.emplace(GetSendTransport(), m_alarmSer);
    m_commandChannel.emplace(GetSendTransport(), m_commandSer);
    m_dataChannel.emplace(GetSendTransport(), m_dataSer);
    m_actuatorChannel.emplace(GetSendTransport(), m_actuatorSer);

    // Bind and wire each delegate through its channel.
    // Bind() sets the target function; the Set* calls wire in the channel's
    // dispatcher, serializer, and stream — mirroring how MakeDelegate() works
    // for async delegates.
    m_alarmMsgDel.Bind(this, &NetworkMgr::ForwardAlarm, ids::ALARM_MSG_ID);
    m_alarmMsgDel.SetDispatcher(m_alarmChannel->GetDispatcher());
    m_alarmMsgDel.SetSerializer(m_alarmChannel->GetSerializer());
    m_alarmMsgDel.SetStream(&m_alarmChannel->GetStream());

    m_commandMsgDel.Bind(this, &NetworkMgr::ForwardCommand, ids::COMMAND_MSG_ID);
    m_commandMsgDel.SetDispatcher(m_commandChannel->GetDispatcher());
    m_commandMsgDel.SetSerializer(m_commandChannel->GetSerializer());
    m_commandMsgDel.SetStream(&m_commandChannel->GetStream());

    m_dataMsgDel.Bind(this, &NetworkMgr::ForwardData, ids::DATA_MSG_ID);
    m_dataMsgDel.SetDispatcher(m_dataChannel->GetDispatcher());
    m_dataMsgDel.SetSerializer(m_dataChannel->GetSerializer());
    m_dataMsgDel.SetStream(&m_dataChannel->GetStream());

    m_actuatorMsgDel.Bind(this, &NetworkMgr::ForwardActuator, ids::ACTUATOR_MSG_ID);
    m_actuatorMsgDel.SetDispatcher(m_actuatorChannel->GetDispatcher());
    m_actuatorMsgDel.SetSerializer(m_actuatorChannel->GetSerializer());
    m_actuatorMsgDel.SetStream(&m_actuatorChannel->GetStream());

    // Register error handlers
    m_alarmMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::OnError));
    m_commandMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::OnError));
    m_dataMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::OnError));
    m_actuatorMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::OnError));

    // Register endpoints with the Base Engine (so Incoming() can route by ID)
    RegisterEndpoint(ids::ALARM_MSG_ID, &m_alarmMsgDel);
    RegisterEndpoint(ids::COMMAND_MSG_ID, &m_commandMsgDel);
    RegisterEndpoint(ids::DATA_MSG_ID, &m_dataMsgDel);
    RegisterEndpoint(ids::ACTUATOR_MSG_ID, &m_actuatorMsgDel);

    // Initialize Base Engine
#ifdef SERVER_APP
    return Initialize("tcp://*:5555", "tcp://*:5556", true);
#else
    return Initialize("tcp://localhost:5556", "tcp://localhost:5555", false);
#endif
}

// Override hooks to fire signals
void NetworkMgr::OnError(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux) {
    (*OnNetworkError)(id, error, aux);
}

void NetworkMgr::OnStatus(DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
    (*OnSendStatus)(id, seq, status);
}

void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note) {
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);
    m_alarmMsgDel(msg, note);
}

bool NetworkMgr::SendAlarmMsgWait(AlarmMsg& msg, AlarmNote& note) {
    return RemoteInvokeWait(m_alarmMsgDel, msg, note);
}

void NetworkMgr::SendCommandMsg(CommandMsg& command) {
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendCommandMsg, m_thread)(command);
    m_commandMsgDel(command);
}

bool NetworkMgr::SendCommandMsgWait(CommandMsg& command) {
    return RemoteInvokeWait(m_commandMsgDel, command);
}

void NetworkMgr::SendDataMsg(DataMsg& data) {
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendDataMsg, m_thread)(data);
    m_dataMsgDel(data);
}

bool NetworkMgr::SendDataMsgWait(DataMsg& data) {
    return RemoteInvokeWait(m_dataMsgDel, data);
}

bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg) {
    return RemoteInvokeWait(m_actuatorMsgDel, msg);
}

std::future<bool> NetworkMgr::SendActuatorMsgFuture(ActuatorMsg& msg) {
    return std::async(&NetworkMgr::SendActuatorMsgWait, this, std::ref(msg));
}