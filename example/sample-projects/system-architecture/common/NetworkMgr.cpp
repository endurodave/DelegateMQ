#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

NetworkMgr::NetworkMgr()
    : m_alarmMsgDel(ids::ALARM_MSG_ID, &m_dispatcher),
    m_commandMsgDel(ids::COMMAND_MSG_ID, &m_dispatcher),
    m_dataMsgDel(ids::DATA_MSG_ID, &m_dispatcher),
    m_actuatorMsgDel(ids::ACTUATOR_MSG_ID, &m_dispatcher)
{
}

int NetworkMgr::Create()
{
    // Bind signals
    m_alarmMsgDel.Bind(AlarmMsgCb.get(), &SignalSafe<void(AlarmMsg&, AlarmNote&)>::operator(), ids::ALARM_MSG_ID);
    m_dataMsgDel.Bind(DataMsgCb.get(), &SignalSafe<void(DataMsg&)>::operator(), ids::DATA_MSG_ID);
    m_commandMsgDel.Bind(CommandMsgCb.get(), &SignalSafe<void(CommandMsg&)>::operator(), ids::COMMAND_MSG_ID);
    m_actuatorMsgDel.Bind(ActuatorMsgCb.get(), &SignalSafe<void(ActuatorMsg&)>::operator(), ids::ACTUATOR_MSG_ID);

    // Register for error callbacks
    m_alarmMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::OnError);
    m_dataMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::OnError);
    m_commandMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::OnError);
    m_actuatorMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::OnError);

    // Register endpoints with the Base Engine (So Incoming() can find them)
    RegisterEndpoint(ids::ALARM_MSG_ID, &m_alarmMsgDel);
    RegisterEndpoint(ids::DATA_MSG_ID, &m_dataMsgDel);
    RegisterEndpoint(ids::COMMAND_MSG_ID, &m_commandMsgDel);
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
    if (ErrorCb) (*ErrorCb)(id, error, aux);
}

void NetworkMgr::OnStatus(DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
    if (SendStatusCb) (*SendStatusCb)(id, seq, status);
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