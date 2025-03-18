/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

static const std::chrono::milliseconds TIMEOUT(2000);

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)> NetworkMgr::SendStatusCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;
MulticastDelegateSafe<void(ActuatorMsg&)> NetworkMgr::ActuatorMsgCb;

typedef std::function<void(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)> SendStatusCallback;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_timeoutThread("NetworkMgrTimeout"),
    m_transportMonitor(TIMEOUT),
    m_alarmMsgDel(ids::ALARM_MSG_ID, &m_dispatcher),
    m_commandMsgDel(ids::COMMAND_MSG_ID, &m_dispatcher),
    m_dataMsgDel(ids::DATA_MSG_ID, &m_dispatcher),
    m_actuatorMsgDel(ids::ACTUATOR_MSG_ID, &m_dispatcher)
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

    // Bind the remote delegate interface to a delegate container. Each 
    // incoming remote message will invoke all registered callbacks.
    m_alarmMsgDel.Bind(&AlarmMsgCb, &AlarmDel::operator(), ids::ALARM_MSG_ID);
    m_alarmMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::ErrorHandler);

    m_dataMsgDel.Bind(&DataMsgCb, &DataDel::operator(), ids::DATA_MSG_ID);
    m_dataMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::ErrorHandler);

    m_commandMsgDel.Bind(&CommandMsgCb, &CommandDel::operator(), ids::COMMAND_MSG_ID);
    m_commandMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::ErrorHandler);

    m_actuatorMsgDel.Bind(&ActuatorMsgCb, &ActuatorDel::operator(), ids::ACTUATOR_MSG_ID);
    m_actuatorMsgDel.ErrorCb += MakeDelegate(this, &NetworkMgr::ErrorHandler);

    int err = 0;
#ifdef SERVER_APP
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_SERVER, "tcp://*:5555");
#else
    err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://localhost:5555");   // Connect to local server
    //err = m_transport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://192.168.0.123:5555"); // Connect to remote server
#endif

    // Set transport monitor callback to catch communication success and error
    m_transportMonitor.SendStatusCb += dmq::MakeDelegate(this, &NetworkMgr::SendStatusHandler);
    m_transport.SetTransportMonitor(&m_transportMonitor);

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_transport);

    // Set remote delegates into map for receiving messages
    m_receiveIdMap[ids::ALARM_MSG_ID] = &m_alarmMsgDel;
    m_receiveIdMap[ids::COMMAND_MSG_ID] = &m_commandMsgDel;
    m_receiveIdMap[ids::DATA_MSG_ID] = &m_dataMsgDel;
    m_receiveIdMap[ids::ACTUATOR_MSG_ID] = &m_actuatorMsgDel;

    return err;
}

void NetworkMgr::Start()
{
    // Reinvoke Start() function call onto m_thread 
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
    // Reinvoke Stop() function call on m_thread and wait for call to complete
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::Stop, m_thread, WAIT_INFINITE)();

    m_recvTimer.Stop();
    m_recvTimer.Expired = nullptr;
    m_timeoutTimer.Stop();
    m_timeoutTimer.Expired = nullptr;
    m_transport.Close();
    //m_transport.Destroy();
}

void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);

    // Send alarm to remote. Invoke remote delegate on m_thread only.
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

// Async send an actuator command and block the caller waiting until the remote 
// client responds or timeout. The execution sequence numbers shown below.
bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg)
{
    // If caller is not executing on m_thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        // *** Caller's thread executes this control branch ***

        bool success = false;
        std::mutex mtx;
        std::condition_variable cv;

        // 5. Callback handler for transport monitor send status (success or failure)
        SendStatusCallback statusCb = [&success, &cv](dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status) {
            if (id == ids::ACTUATOR_MSG_ID) {
                // Client received ActuatorMsg?
                if (status == TransportMonitor::Status::SUCCESS)
                    success = true;
                cv.notify_one();
            }
        };

        // 1. Register for send status callback (success or failure)
        m_transportMonitor.SendStatusCb += dmq::MakeDelegate(statusCb);

        // 2. Reinvoke SendActuatorMsgWait() on m_thread context
        MakeDelegate(this, &NetworkMgr::SendActuatorMsgWait, m_thread)(msg);

        // 3. Wait for statusCb callback to be invoked on m_thread
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, TIMEOUT) == std::cv_status::timeout)
        {
            // A timeout should never happen. The transport monitor is setup for a max 
            // TIMEOUT, so success or failure should never cause cv_status::timeout
            std::cout << "Timeout SendActuatorMsgWait()" << std::endl;
        }

        // 6. Unregister from status callback
        m_transportMonitor.SendStatusCb -= dmq::MakeDelegate(statusCb);

        // 7. Return the blocking async function invoke status to caller
        return success;
    }
    else
    {
        // *** NetworkMgr::m_thread executes this control branch ***

        // 4. Send actuator command to remote on m_thread. 
        m_actuatorMsgDel(msg);
        return true;
    }
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
                // Invoke the receiver target callable with the sender's 
                // incoming argument data
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
    // Periodically process message timeouts
    m_transportMonitor.Process();
}

void NetworkMgr::ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
{
    ErrorCb(id, error, aux);
}

void NetworkMgr::SendStatusHandler(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)
{
    SendStatusCb(id, seqNum, status);
}


