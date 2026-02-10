#include "NetworkMgr.h"

#if defined(STM32F407xx)
    extern UART_HandleTypeDef huart6;
#endif

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
    m_alarmMsgDel.Bind(this, &NetworkMgr::ForwardAlarm, ids::ALARM_MSG_ID);
    m_dataMsgDel.Bind(this, &NetworkMgr::ForwardData, ids::DATA_MSG_ID);
    m_commandMsgDel.Bind(this, &NetworkMgr::ForwardCommand, ids::COMMAND_MSG_ID);
    m_actuatorMsgDel.Bind(this, &NetworkMgr::ForwardActuator, ids::ACTUATOR_MSG_ID);

    // Register for error callbacks using single  signment (=)
    m_alarmMsgDel.OnError = MakeDelegate(this, &NetworkMgr::OnError);
    m_dataMsgDel.OnError = MakeDelegate(this, &NetworkMgr::OnError);
    m_commandMsgDel.OnError = MakeDelegate(this, &NetworkMgr::OnError);
    m_actuatorMsgDel.OnError = MakeDelegate(this, &NetworkMgr::OnError);

    // Register endpoints with the Base Engine (So Incoming() can find them)
    RegisterEndpoint(ids::ALARM_MSG_ID, &m_alarmMsgDel);
    RegisterEndpoint(ids::DATA_MSG_ID, &m_dataMsgDel);
    RegisterEndpoint(ids::COMMAND_MSG_ID, &m_commandMsgDel);
    RegisterEndpoint(ids::ACTUATOR_MSG_ID, &m_actuatorMsgDel);

    // Initialize Base Engine
#ifdef SERVER_APP
    #if defined(STM32F407xx)
        return Initialize(&huart6);
    #endif
#else
    // Connects to STM32 UART @ 115200 baud
    // @TODO Change PC COM port if necessary.
    return Initialize("COM3", 115200);
#endif
}

// Override hooks to fire signals
void NetworkMgr::OnError(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux) {
    if (OnNetworkError) (*OnNetworkError)(id, error, aux);
}

void NetworkMgr::OnStatus(DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
    if (OnSendStatus) (*OnSendStatus)(id, seq, status);
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

void NetworkMgr::SendActuatorMsg(ActuatorMsg& msg) {
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendActuatorMsg, m_thread)(msg);
    m_actuatorMsgDel(msg);
}

bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg) {
    return RemoteInvokeWait(m_actuatorMsgDel, msg);
}

