/// @file
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include "NetworkMgr.h"

using namespace dmq;
using namespace std;

const std::chrono::milliseconds NetworkMgr::SEND_TIMEOUT(100);
const std::chrono::milliseconds NetworkMgr::RECV_TIMEOUT(2000);

MulticastDelegateSafe<void(DelegateRemoteId, DelegateError, DelegateErrorAux)> NetworkMgr::ErrorCb;
MulticastDelegateSafe<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)> NetworkMgr::SendStatusCb;
MulticastDelegateSafe<void(AlarmMsg&, AlarmNote&)> NetworkMgr::AlarmMsgCb;
MulticastDelegateSafe<void(CommandMsg&)> NetworkMgr::CommandMsgCb;
MulticastDelegateSafe<void(DataMsg&)> NetworkMgr::DataMsgCb;
MulticastDelegateSafe<void(ActuatorMsg&)> NetworkMgr::ActuatorMsgCb;

NetworkMgr::NetworkMgr() :
    m_thread("NetworkMgr"),
    m_transportMonitor(RECV_TIMEOUT),
    m_alarmMsgDel(ids::ALARM_MSG_ID, &m_dispatcher),
    m_commandMsgDel(ids::COMMAND_MSG_ID, &m_dispatcher),
    m_dataMsgDel(ids::DATA_MSG_ID, &m_dispatcher),
    m_actuatorMsgDel(ids::ACTUATOR_MSG_ID, &m_dispatcher)
{
    // Create the thread with a 5s watchdog timeout
    m_thread.CreateThread(std::chrono::milliseconds(5000));
}

NetworkMgr::~NetworkMgr()
{
    Stop();
    m_thread.ExitThread();
    delete m_recvThread;
    m_recvThread = nullptr;
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
    err = m_sendTransport.Create(ZeroMqTransport::Type::PAIR_SERVER, "tcp://*:5555");
    err = m_recvTransport.Create(ZeroMqTransport::Type::PAIR_SERVER, "tcp://*:5556");
#else
    err = m_sendTransport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://localhost:5556");
    err = m_recvTransport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://localhost:5555");       // Connect to local server
    //err = m_recvTransport.Create(ZeroMqTransport::Type::PAIR_CLIENT, "tcp://192.168.0.123:5555"); // Connect to remote server
#endif

    // Set transport monitor callback to catch communication success and error
    m_transportMonitor.SendStatusCb += dmq::MakeDelegate(this, &NetworkMgr::SendStatusHandler);

    // Set transport monitors to detect message success or timeout
    m_sendTransport.SetTransportMonitor(&m_transportMonitor);
    m_recvTransport.SetTransportMonitor(&m_transportMonitor);

    // The two transports operate as a send/recv pair. Each ZeroMQ socket operates
    // on its own thread.
    m_sendTransport.SetRecvTransport(&m_recvTransport);
    m_recvTransport.SetSendTransport(&m_sendTransport);

    // Set the transport used by the dispatcher
    m_dispatcher.SetTransport(&m_sendTransport);

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

    // Create the receive thread to process incoming messages
    m_recvThread = new std::thread(&NetworkMgr::RecvThread, this);

    // Start a timer to process message timeouts
    m_timeoutTimer.Expired = MakeDelegate(this, &NetworkMgr::Timeout, m_thread);
    m_timeoutTimer.Start(std::chrono::milliseconds(100));
}

void NetworkMgr::Stop()
{
    // Reinvoke Stop() function call on m_thread and wait for call to complete
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        // Close transports BEFORE joining to unblock the RecvThread
        m_recvTransport.Close();
        m_sendTransport.Close();

        // Ensure the receive thread loop completes 
        m_recvThreadExit.store(true);
        if (m_recvThread && m_recvThread->joinable())
            m_recvThread->join();

        return MakeDelegate(this, &NetworkMgr::Stop, m_thread, WAIT_INFINITE)();
    }

    m_timeoutTimer.Stop();
    m_timeoutTimer.Expired = nullptr;

    //m_sendTransport.Destroy();
    //m_recvTransport.Destroy();
}

void NetworkMgr::SendAlarmMsg(AlarmMsg& msg, AlarmNote& note)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendAlarmMsg, m_thread)(msg, note);

    // Send alarm to remote. Invoke remote delegate on m_thread only.
    m_alarmMsgDel(msg, note);
}

bool NetworkMgr::SendAlarmMsgWait(AlarmMsg& msg, AlarmNote& note)
{
    // Send alarm to remote and wait for success/failure. 
    return RemoteInvoke(m_alarmMsgDel, msg, note);
}

void NetworkMgr::SendCommandMsg(CommandMsg& command)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendCommandMsg, m_thread)(command);

    // Send data to remote. 
    m_commandMsgDel(command);
}

bool NetworkMgr::SendCommandMsgWait(CommandMsg& command)
{
    // Send command to remote and wait for success/failure. 
    return RemoteInvoke(m_commandMsgDel, command);
}

void NetworkMgr::SendDataMsg(DataMsg& data)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkMgr::SendDataMsg, m_thread)(data);

    // Send data to remote. 
    m_dataMsgDel(data);
}

bool NetworkMgr::SendDataMsgWait(DataMsg& data)
{
    // Send data to remote and wait for success/failure. 
    return RemoteInvoke(m_dataMsgDel, data);
}

bool NetworkMgr::SendActuatorMsgWait(ActuatorMsg& msg)
{
    return RemoteInvoke(m_actuatorMsgDel, msg);
}

// Send an actuator command and do not block. Return value success or failure captured 
// later by the caller using the std::future<bool>::get().
std::future<bool> NetworkMgr::SendActuatorMsgFuture(ActuatorMsg& msg)
{
    return std::async(&NetworkMgr::SendActuatorMsgWait, this, std::ref(msg));
}

// Receive thread loop polls for incoming messages. This thread polls the receive socket
// and sends the incoming argument data to the Incoming() handler function.
void NetworkMgr::RecvThread()
{
    static const std::chrono::milliseconds INVOKE_TIMEOUT(1000);
    while (!m_recvThreadExit.load())
    {
        {
            DmqHeader header;

            // If using XALLOCATOR explicit operator new required. See xallocator.h.
            std::shared_ptr<xstringstream> arg_data(
                new xstringstream(std::ios::in | std::ios::out | std::ios::binary)
            );

            // Poll for incoming message
            int error = m_recvTransport.Receive(*arg_data, header);
            if (!error && !arg_data->str().empty() && !m_recvThreadExit.load())
            {
                // Async invoke the incoming message handler function. A delegate blocking async
                // invoke here means header and arg_data argument are not copied but instead passed to 
                // the target function directly.
                auto success = MakeDelegate(this, &NetworkMgr::Incoming, m_thread, INVOKE_TIMEOUT).AsyncInvoke(header, arg_data);
                if (!success.has_value())
                {
                    // Timeout occurred waiting for the incoming message handler to complete
                    cout << "Incoming message handler timeout!" << endl;
                }
            }
        }
    }
}

void NetworkMgr::Incoming(DmqHeader& header, std::shared_ptr<xstringstream> arg_data)
{
    // Process incoming message
    if (header.GetId() != ACK_REMOTE_ID)
    {
        // Process remote delegate message
        auto receiveDelegate = m_receiveIdMap[header.GetId()];
        if (receiveDelegate)
        {
            // Invoke the receiver target callable with the sender's 
            // incoming argument data
            receiveDelegate->Invoke(*arg_data);
        }
        else
        {
            cout << "Receiver delegate not found!" << endl;
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



