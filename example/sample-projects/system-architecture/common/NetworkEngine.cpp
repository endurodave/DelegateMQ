#include "NetworkEngine.h"

using namespace dmq;
using namespace std;

const std::chrono::milliseconds NetworkEngine::SEND_TIMEOUT(100);
const std::chrono::milliseconds NetworkEngine::RECV_TIMEOUT(2000);

NetworkEngine::NetworkEngine()
    : m_thread("NetworkEngine"),
    m_transportMonitor(RECV_TIMEOUT)
{
    m_thread.CreateThread(std::chrono::milliseconds(5000));
}

NetworkEngine::~NetworkEngine()
{
    Stop();
    m_thread.ExitThread();
    delete m_recvThread;
}

int NetworkEngine::Initialize(const std::string& sendAddr, const std::string& recvAddr, bool isServer)
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Initialize, m_thread, WAIT_INFINITE)(sendAddr, recvAddr, isServer);

    int err = 0;
    auto type = isServer ? ZeroMqTransport::Type::PAIR_SERVER : ZeroMqTransport::Type::PAIR_CLIENT;

    err += m_sendTransport.Create(type, sendAddr.c_str());
    err += m_recvTransport.Create(type, recvAddr.c_str());

    m_transportMonitor.SendStatusCb += dmq::MakeDelegate(this, &NetworkEngine::InternalStatusHandler);

    m_sendTransport.SetTransportMonitor(&m_transportMonitor);
    m_recvTransport.SetTransportMonitor(&m_transportMonitor);

    m_sendTransport.SetRecvTransport(&m_recvTransport);
    m_recvTransport.SetSendTransport(&m_sendTransport);

    m_dispatcher.SetTransport(&m_sendTransport);

    return err;
}

void NetworkEngine::Start()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        return MakeDelegate(this, &NetworkEngine::Start, m_thread)();

    if (!m_recvThread) {
        m_recvThread = new std::thread(&NetworkEngine::RecvThread, this);
    }

    m_timeoutTimerConn = m_timeoutTimer.Expired->Connect(MakeDelegate(this, &NetworkEngine::Timeout, m_thread));
    m_timeoutTimer.Start(std::chrono::milliseconds(100));
}

void NetworkEngine::Stop()
{
    if (Thread::GetCurrentThreadId() != m_thread.GetThreadId()) {
        m_recvTransport.Close();
        m_sendTransport.Close();
        m_recvThreadExit = true;
        if (m_recvThread && m_recvThread->joinable()) {
            m_recvThread->join();
            delete m_recvThread;
            m_recvThread = nullptr;
        }
        return MakeDelegate(this, &NetworkEngine::Stop, m_thread, WAIT_INFINITE)();
    }
    m_timeoutTimer.Stop();
    m_timeoutTimerConn.Disconnect();
}

void NetworkEngine::RegisterEndpoint(dmq::DelegateRemoteId id, dmq::IRemoteInvoker* endpoint)
{
    m_receiveIdMap[id] = endpoint;
}

void NetworkEngine::RecvThread()
{
    static const std::chrono::milliseconds INVOKE_TIMEOUT(1000);
    while (!m_recvThreadExit)
    {
        DmqHeader header;
        std::shared_ptr<xstringstream> arg_data(new xstringstream(std::ios::in | std::ios::out | std::ios::binary));

        int error = m_recvTransport.Receive(*arg_data, header);
        if (!error && !arg_data->str().empty() && !m_recvThreadExit)
        {
            MakeDelegate(this, &NetworkEngine::Incoming, m_thread, INVOKE_TIMEOUT).AsyncInvoke(header, arg_data);
        }
    }
}

void NetworkEngine::Incoming(DmqHeader& header, std::shared_ptr<xstringstream> arg_data)
{
    if (header.GetId() != ACK_REMOTE_ID) {
        auto it = m_receiveIdMap.find(header.GetId());
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