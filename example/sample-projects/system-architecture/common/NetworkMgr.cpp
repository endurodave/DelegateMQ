/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "NetworkMgr.h"
#include <iostream>

using namespace dmq;
using namespace std;

static const std::chrono::milliseconds TIMEOUT(2000);

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(uint16_t, dmq::DelegateRemoteId, TransportMonitor::Status)> NetworkMgr::SendStatusCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;
MulticastDelegateSafe<void(ActuatorMsg&)> NetworkMgr::ActuatorMsgCb;

typedef std::function<void(uint16_t seqNum, dmq::DelegateRemoteId id, TransportMonitor::Status status)> SendStatusCallback;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_timeoutThread("NetworkMgrTimeout"),
    m_argStream(ios::in | ios::out | ios::binary),
    m_transportMonitor(TIMEOUT)
{
    // Create the threads
    m_thread.CreateThread();
    m_timeoutThread.CreateThread();
}

NetworkMgr::~NetworkMgr()
{
    m_thread.ExitThread();
    m_timeoutThread.ExitThread();
}

int NetworkMgr::Create()
{
    // Reinvoke call onto NetworkMgr thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Create, m_thread, WAIT_INFINITE)();

    // Setup the remote delegate interfaces
    m_alarmMsgDel.SetStream(&m_argStream);
    m_alarmMsgDel.SetSerializer(&m_alarmMsgSer);
    m_alarmMsgDel.SetDispatcher(&m_dispatcher);
    m_alarmMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_alarmMsgDel = MakeDelegate(this, &NetworkMgr::RecvAlarmMsg, ALARM_MSG_ID);

    m_dataMsgDel.SetStream(&m_argStream);
    m_dataMsgDel.SetSerializer(&m_dataMsgSer);
    m_dataMsgDel.SetDispatcher(&m_dispatcher);
    m_dataMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_dataMsgDel = MakeDelegate(this, &NetworkMgr::RecvDataMsg, DATA_MSG_ID);

    m_commandMsgDel.SetStream(&m_argStream);
    m_commandMsgDel.SetSerializer(&m_commandMsgSer);
    m_commandMsgDel.SetDispatcher(&m_dispatcher);
    m_commandMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_commandMsgDel = MakeDelegate(this, &NetworkMgr::RecvCommandMsg, COMMAND_MSG_ID);

    m_actuatorMsgDel.SetStream(&m_argStream);
    m_actuatorMsgDel.SetSerializer(&m_actuatorMsgSer);
    m_actuatorMsgDel.SetDispatcher(&m_dispatcher);
    m_actuatorMsgDel.SetErrorHandler(MakeDelegate(this, &NetworkMgr::ErrorHandler));
    m_actuatorMsgDel = MakeDelegate(this, &NetworkMgr::RecvActuatorMsg, ACTUATOR_MSG_ID);

    int err = 0;
#ifdef SERVER_APP
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_SERVER, "tcp://*:5555");
#else
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://localhost:5555");   // Connect to local server
    //err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://192.168.0.123:5555"); // Connect to remote server
#endif

    m_transportMonitor.SendStatusCb += dmq::MakeDelegate(this, &NetworkMgr::SendStatusHandler);
    m_transport.SetTransportMonitor(&m_transportMonitor);

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_transport);

    // Set receive async delegates into map
    m_receiveIdMap[ALARM_MSG_ID] = &m_alarmMsgDel;
    m_receiveIdMap[COMMAND_MSG_ID] = &m_commandMsgDel;
    m_receiveIdMap[DATA_MSG_ID] = &m_dataMsgDel;
    m_receiveIdMap[ACTUATOR_MSG_ID] = &m_actuatorMsgDel;

    return err;
}

void NetworkMgr::Start()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Start, m_thread)();

    // Start a timer to poll data
    m_recvTimer.Expired = MakeDelegate(this, &NetworkMgr::Poll, m_thread);
    m_recvTimer.Start(std::chrono::milliseconds(20));

    // Start a timer to process message timeouts
    m_timeoutTimer.Expired = MakeDelegate(this, &NetworkMgr::Timeout, m_timeoutThread);
    m_timeoutTimer.Start(std::chrono::milliseconds(100));
}

void NetworkMgr::Stop()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Stop, m_thread, WAIT_INFINITE)();

    m_recvTimer.Stop();
    m_recvTimer.Expired = nullptr;
    m_timeoutTimer.Stop();
    m_timeoutTimer.Expired = nullptr;
    m_dispatcher.SetTransport(nullptr);
    m_transport.Close();
    //m_transport.Destroy();
}

void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);

    // Send alarm to remote. 
    m_alarmMsgDel(msg, note);
}

void NetworkMgr::SendCommandMsg(CommandMsg& command)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendCommandMsg, m_thread)(command);

    // Send data to remote. 
    m_commandMsgDel(command);
}

void NetworkMgr::SendDataMsg(DataMsg& data)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendDataMsg, m_thread)(data);

    // Send data to remote. 
    m_dataMsgDel(data);
}

// Async send an actuator command and block the caller waiting for the client 
// to respond. The execution sequence numbers shown below.
bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg)
{
    // If caller is not executing on m_thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        bool success = false;
        std::mutex mtx;
        std::condition_variable cv;

        // 1. Callback handler for transport monitor send status
        SendStatusCallback statusCb = [&success, &cv](uint16_t seqNum, dmq::DelegateRemoteId id, TransportMonitor::Status status) {
            if (id == ACTUATOR_MSG_ID) {
                // Client received ActuatorMsg?
                if (status == TransportMonitor::Status::SUCCESS)
                    success = true;
                cv.notify_one();
            }
        };

        // 2. Register for send status callback (success or failure)
        m_transportMonitor.SendStatusCb += dmq::MakeDelegate(statusCb);

        // 3. Renvoke SendActuatorMsgWait() on m_thread context
        MakeDelegate(this, &NetworkMgr::SendActuatorMsgWait, m_thread)(msg);

        // 5. Wait for statusCb callback to be triggered on m_thread
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, TIMEOUT + std::chrono::milliseconds(100)) == std::cv_status::timeout)
        {
            // A timeout should never happen. The transport monitor is setup for a max 
            // TIMEOUT, so success or failure should never cause cv_status::timeout
            std::cout << "Timeout SendActuatorMsgWait()" << std::endl;
        }

        // 6. Unregister from status callback
        m_transportMonitor.SendStatusCb -= dmq::MakeDelegate(statusCb);

        // 7. Return the blocking async function invoke status
        return success;
    }

    // 4. Send actuator command to remote on m_thread. 
    m_actuatorMsgDel(msg);
    return true;
}

// Poll called periodically on m_thread context
void NetworkMgr::Poll()
{
    // Get incoming data
    DmqHeader header;
    auto arg_data = m_transport.Receive(header);

    // Incoming remote delegate data arrived?
    if (!arg_data.str().empty())
    {
        if (header.GetId() != ACK_REMOTE_ID)
        {
            // Process remote delegate message
            auto receiveDelegate = m_receiveIdMap[header.GetId()];
            if (receiveDelegate)
            {
                // Invoke the receiver target function with the sender's argument data
                receiveDelegate->Invoke(arg_data);
            }
            else
            {
                cout << "Receiver delegate not found!" << endl;
            }
        }
    }
}

void NetworkMgr::Timeout()
{
    // Preiodically process message timeouts
    m_transportMonitor.Process();
}

void NetworkMgr::ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
{
    ErrorCb(id, error, aux);
}

void NetworkMgr::SendStatusHandler(uint16_t seqNum, dmq::DelegateRemoteId id, TransportMonitor::Status status)
{
    SendStatusCb(seqNum, id, status);
}

void NetworkMgr::RecvAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    AlarmMsgCb(msg, note);
}

void NetworkMgr::RecvCommandMsg(CommandMsg& command)
{
    CommandMsgCb(command);
}

void NetworkMgr::RecvDataMsg(DataMsg& data)
{
    DataMsgCb(data);
}

void NetworkMgr::RecvActuatorMsg(ActuatorMsg& msg)
{
    ActuatorMsgCb(msg);
}

