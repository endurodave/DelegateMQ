/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

static const std::chrono::milliseconds SEND_TIMEOUT(100);
static const std::chrono::milliseconds RECV_TIMEOUT(2000);

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)> NetworkMgr::SendStatusCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;
MulticastDelegateSafe<void(ActuatorMsg&)> NetworkMgr::ActuatorMsgCb;

typedef std::function<void(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)> SendStatusCallback;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_transportMonitor(RECV_TIMEOUT),
    m_alarmMsgDel(ids::ALARM_MSG_ID, &m_dispatcher),
    m_commandMsgDel(ids::COMMAND_MSG_ID, &m_dispatcher),
    m_dataMsgDel(ids::DATA_MSG_ID, &m_dispatcher),
    m_actuatorMsgDel(ids::ACTUATOR_MSG_ID, &m_dispatcher)
{
    // Create the threads
    m_thread.CreateThread();
}

NetworkMgr::~NetworkMgr()
{
    m_thread.ExitThread();
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
    m_timeoutTimer.Expired = MakeDelegate(this, &NetworkMgr::Timeout, m_thread);
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
// client successfully receives the message or timeout. The execution order sequence 
// numbers shown below.
bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg)
{
    // If caller is not executing on m_thread
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        // *** Caller's thread executes this control branch ***

        std::atomic<bool> success(false);
        bool complete = false;
        std::mutex mtx;
        std::condition_variable cv;

        // 7. Callback lambda handler for transport monitor when remote receives message (success or failure).
        //    Callback context is m_thread.
        SendStatusCallback statusCb = [&success, &complete, &cv, &mtx](dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status) {
            if (id == ids::ACTUATOR_MSG_ID) {
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

        // 2. Async reinvoke SendActuatorMsgWait() on m_thread context and wait for send to complete
        auto del = MakeDelegate(this, &NetworkMgr::SendActuatorMsgWait, m_thread, SEND_TIMEOUT);
        auto retVal = del.AsyncInvoke(msg);

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
                    std::cout << "Timeout SendActuatorMsgWait()" << std::endl;
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
        m_actuatorMsgDel(msg);        

        // 4. Check if send succeeded
        if (m_actuatorMsgDel.GetError() == DelegateError::SUCCESS)
        {
            return true;
        }

        std::cout << "Send failed!" << std::endl;
        return false;
    }
}

// Send an actuator command and do not block. Return value success or failure captured 
// later by the caller using the std::future<bool>::get().
std::future<bool> NetworkMgr::SendActuatorMsgFuture(ActuatorMsg& msg)
{
    return std::async(&NetworkMgr::SendActuatorMsgWait, this, std::ref(msg));
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


