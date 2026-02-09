#include "NetworkEngine.h"

// Only compile implementation if a compatible transport is selected
#if defined(DMQ_TRANSPORT_ZEROMQ) || defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP) || defined(DMQ_TRANSPORT_STM32_UART) || defined(DMQ_TRANSPORT_SERIAL_PORT)

using namespace dmq;
using namespace std;

const std::chrono::milliseconds NetworkEngine::SEND_TIMEOUT(100);
const std::chrono::milliseconds NetworkEngine::RECV_TIMEOUT(2000);

NetworkEngine::NetworkEngine()
    : m_thread("NetworkEngine"),
    m_transportMonitor(RECV_TIMEOUT),
    m_recvThread("NetworkRecv")
#if defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP)
    , m_retryMonitor(m_sendTransport, m_transportMonitor)
    , m_reliableTransport(m_sendTransport, m_retryMonitor)
#elif defined(DMQ_TRANSPORT_STM32_UART)
    , m_transport()                     // Init the real object
    , m_sendTransport(m_transport)      // Alias it
    , m_recvTransport(m_transport)      // Alias it
    , m_retryMonitor(m_sendTransport, m_transportMonitor)
    , m_reliableTransport(m_sendTransport, m_retryMonitor)
#elif defined(DMQ_TRANSPORT_SERIAL_PORT)
    // Initialize references to point to the single m_transport instance
    , m_transport()
    , m_sendTransport(m_transport)
    , m_recvTransport(m_transport)
    , m_retryMonitor(m_sendTransport, m_transportMonitor)
    , m_reliableTransport(m_sendTransport, m_retryMonitor)
#endif
{
    m_thread.CreateThread();
}

NetworkEngine::~NetworkEngine()
{
    Stop();
    m_thread.ExitThread();
}

// SWITCH: Initialize Implementation

#if defined(DMQ_TRANSPORT_ZEROMQ)

// --------------------------------------------------------
// ZeroMQ Implementation
// --------------------------------------------------------
int NetworkEngine::Initialize(const std::string& sendAddr, const std::string& recvAddr, bool isServer)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Initialize, m_thread, WAIT_INFINITE)(sendAddr, recvAddr, isServer);

    int err = 0;
    auto type = isServer ? ZeroMqTransport::Type::PAIR_SERVER : ZeroMqTransport::Type::PAIR_CLIENT;

    err += m_sendTransport.Create(type, sendAddr.c_str());
    err += m_recvTransport.Create(type, recvAddr.c_str());

    m_statusConn = m_transportMonitor.OnSendStatus->Connect(dmq::MakeDelegate(this, &NetworkEngine::InternalStatusHandler));

    m_sendTransport.SetTransportMonitor(&m_transportMonitor);
    m_recvTransport.SetTransportMonitor(&m_transportMonitor);

    m_sendTransport.SetRecvTransport(&m_recvTransport);
    m_recvTransport.SetSendTransport(&m_sendTransport);

    // ZeroMQ handles its own reliability, so we DO NOT use ReliableTransport here.
    m_dispatcher.SetTransport(&m_sendTransport);

    return err;
}

#elif defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP)

// --------------------------------------------------------
// UDP Implementation (Windows & Linux)
// --------------------------------------------------------
int NetworkEngine::Initialize(const std::string& sendIp, int sendPort, const std::string& recvIp, int recvPort)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Initialize, m_thread, WAIT_INFINITE)(sendIp, sendPort, recvIp, recvPort);

    int err = 0;
    // UDP typically uses PUB/SUB or generic send/recv
    err += m_sendTransport.Create(UdpTransport::Type::PUB, sendIp.c_str(), sendPort);
    err += m_recvTransport.Create(UdpTransport::Type::SUB, recvIp.c_str(), recvPort);

    m_statusConn = m_transportMonitor.OnSendStatus->Connect(dmq::MakeDelegate(this, &NetworkEngine::InternalStatusHandler));

    m_sendTransport.SetTransportMonitor(&m_transportMonitor);
    m_recvTransport.SetTransportMonitor(&m_transportMonitor);

    m_sendTransport.SetRecvTransport(&m_recvTransport);
    m_recvTransport.SetSendTransport(&m_sendTransport);

    // UDP: Reliable wrapper usage (Adds ACKs/Retries)
    m_dispatcher.SetTransport(&m_reliableTransport);

    return err;
}

#elif defined(DMQ_TRANSPORT_STM32_UART)

// --------------------------------------------------------
// STM32 UART Implementation
// --------------------------------------------------------
int NetworkEngine::Initialize(UART_HandleTypeDef* huart)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Initialize, m_thread, WAIT_INFINITE)(huart);

    int err = 0;

    // Call Create ONCE on the shared object
    err += m_transport.Create(huart);

    m_statusConn = m_transportMonitor.OnSendStatus->Connect(dmq::MakeDelegate(this, &NetworkEngine::InternalStatusHandler));

    m_transport.SetTransportMonitor(&m_transportMonitor);

    // Point to self for full-duplex logic
    m_transport.SetRecvTransport(&m_transport);
    m_transport.SetSendTransport(&m_transport);

    // Use Reliable wrapper
    m_dispatcher.SetTransport(&m_reliableTransport);

    return err;
}
#elif defined(DMQ_TRANSPORT_SERIAL_PORT)

// --------------------------------------------------------
// Serial Port Implementation (Shared Send/Recv)
// --------------------------------------------------------
int NetworkEngine::Initialize(const std::string& portName, int baudRate)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Initialize, m_thread, WAIT_INFINITE)(portName, baudRate);

    int err = 0;

    // OPEN PORT ONCE via the shared instance
    err += m_transport.Create(portName.c_str(), baudRate);

    if (err == 0) {
        // Only hook up monitoring if open succeeded
        m_statusConn = m_transportMonitor.OnSendStatus->Connect(dmq::MakeDelegate(this, &NetworkEngine::InternalStatusHandler));

        m_transport.SetTransportMonitor(&m_transportMonitor);

        // Serial is full-duplex logic logic on one object
        m_transport.SetRecvTransport(&m_transport);
        m_transport.SetSendTransport(&m_transport);

        // Route dispatcher through reliability layer (ACKs/Retries)
        m_dispatcher.SetTransport(&m_reliableTransport);
    }

    return err;
}
#endif

void NetworkEngine::Start()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Start, m_thread)();

    static bool bRecvThreadCreated = false;

    if (!bRecvThreadCreated)
    {
        bRecvThreadCreated = true;
        m_recvThread.CreateThread();

        // Post the "RecvThread" loop to run on this new thread.
        MakeDelegate(this, &NetworkEngine::RecvThread, m_recvThread).AsyncInvoke();
    }

    m_timeoutTimerConn = m_timeoutTimer.OnExpired->Connect(MakeDelegate(this, &NetworkEngine::Timeout, m_thread));
    m_timeoutTimer.Start(std::chrono::milliseconds(100));
}

void NetworkEngine::Stop()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId()) {

        // Close calls are safe for both transport types
        m_recvTransport.Close();
        m_sendTransport.Close();

        m_recvThreadExit = true;
        m_recvThread.ExitThread();

        return MakeDelegate(this, &NetworkEngine::Stop, m_thread, WAIT_INFINITE)();
    }
    m_timeoutTimer.Stop();
    m_timeoutTimerConn.Disconnect();
    m_statusConn.Disconnect();
}

void NetworkEngine::RegisterEndpoint(dmq::DelegateRemoteId id, dmq::IRemoteInvoker* endpoint)
{
    // Thread Safety: Ensure this runs on the Network Thread to avoid 
    // racing with 'Incoming()' which reads this map.
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
    {
        // Marshal the call to the Network Thread
        MakeDelegate(this, &NetworkEngine::RegisterEndpoint, m_thread, dmq::WAIT_INFINITE)(id, endpoint);
        return;
    }

    // Actual insertion (Safe because we are now on the correct thread)
    m_receiveIdMap[id] = endpoint;
}

//------------------------------------------------------------------------------
// RecvThread
//------------------------------------------------------------------------------
/// @brief The main loop for the background receive thread.
/// 
/// @details This function continuously polls the underlying transport layer for 
/// incoming data. When a packet is successfully received:
/// 1. It extracts the header and payload into a stream.
/// 2. It marshals the data to the internal `NetworkEngine` thread by asynchronously 
///    invoking `Incoming()`.
/// 
/// The `INVOKE_TIMEOUT` ensures that if the main Network Thread is deadlocked or 
/// its queue is full, this receive thread won't hang indefinitely trying to 
/// enqueue the message.
void NetworkEngine::RecvThread()
{
    // Timeout for enqueuing the message to the main thread.
    static const std::chrono::milliseconds INVOKE_TIMEOUT(1000);

    while (!m_recvThreadExit)
    {
        DmqHeader header;
        // Use a shared_ptr for the stream to efficiently pass data between threads
        std::shared_ptr<xstringstream> arg_data(new xstringstream(std::ios::in | std::ios::out | std::ios::binary));

        // Block reading from the physical transport
        int error = m_recvTransport.Receive(*arg_data, header);

        if (!error && !arg_data->str().empty() && !m_recvThreadExit)
        {
            // Dispatch processing to the main NetworkEngine thread. 
            // Passes ownership of the data stream via shared_ptr (no deep copy).
            MakeDelegate(this, &NetworkEngine::Incoming, m_thread, INVOKE_TIMEOUT).AsyncInvoke(header, arg_data);
        }
    }
}

//------------------------------------------------------------------------------
// Incoming
//------------------------------------------------------------------------------
/// @brief Handles incoming messages on the main Network Thread.
/// 
/// @details This function acts as the central dispatcher. It:
/// 1. Ignores ACK messages (which are typically handled by the TransportMonitor 
///    or blocking wait logic).
/// 2. Looks up the registered `IRemoteInvoker` endpoint associated with the message ID.
/// 3. Invokes the endpoint to deserialize the arguments and trigger the 
///    corresponding application callback.
/// 
/// @param[in] header The message header containing the Remote ID.
/// @param[in] arg_data The serialized payload stream.
void NetworkEngine::Incoming(DmqHeader& header, std::shared_ptr<xstringstream> arg_data)
{
    // Filter out ACKs; we only dispatch application data here.
    if (header.GetId() != ACK_REMOTE_ID) {
        // Find the registered endpoint for this Message ID
        auto it = m_receiveIdMap.find(header.GetId());

        // If found and valid, let the endpoint handle deserialization and execution
        if (it != m_receiveIdMap.end() && it->second) {
            it->second->Invoke(*arg_data);
        }
    }
}

void NetworkEngine::Timeout() { m_transportMonitor.Process(); }

void NetworkEngine::InternalErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux) {
    OnError(id, error, aux);
}

void NetworkEngine::InternalStatusHandler(DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
    OnStatus(id, seq, status);
}

// Default virtual implementations
void NetworkEngine::OnError(DelegateRemoteId, DelegateError, DelegateErrorAux) {}
void NetworkEngine::OnStatus(DelegateRemoteId, uint16_t, TransportMonitor::Status) {}

#endif // Defined Transport Check
