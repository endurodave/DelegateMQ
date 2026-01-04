/// @file NetworkEngine.h
/// @brief Base class for handling network transport, threading, and synchronization.
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#ifndef NETWORK_ENGINE_H
#define NETWORK_ENGINE_H

#include "DelegateMQ.h"
#include "RemoteEndpoint.h"
#include <map>
#include <mutex>
#include <atomic>
#include <future>
#include <iostream>
#include <functional> 

class NetworkEngine
{
public:
    NetworkEngine();
    virtual ~NetworkEngine();

    NetworkEngine(const NetworkEngine&) = delete;
    NetworkEngine& operator=(const NetworkEngine&) = delete;

    int Initialize(const std::string& sendAddr, const std::string& recvAddr, bool isServer);
    void Start();
    void Stop();

    void RegisterEndpoint(dmq::DelegateRemoteId id, dmq::IRemoteInvoker* endpoint);

    /// Generic helper function for invoking any remote delegate. The call blocks 
    /// until the remote acknowledges the message or a timeout occurs.
    /// Execution flow follows the numbered comments 1 -> 12.
    template <class TClass, class RetType, class... Args>
    bool RemoteInvokeWait(RemoteEndpoint<TClass, RetType(Args...)>& endpoint, Args&&... args)
    {
        // 1. [Caller Thread] Check if we are on the Network Thread.
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
        {
            // 2. [Caller Thread] Create shared synchronization state.
            struct SyncState {
                std::atomic<bool> success{ false };
                bool complete = false;
                std::mutex mtx;
                std::condition_variable cv;
                XALLOCATOR
            };
            auto state = std::make_shared<SyncState>();
            dmq::DelegateRemoteId remoteId = endpoint.GetRemoteId();

            // 3. [Caller Thread] Define the callback that wakes us up later.
            //    (This will eventually execute on the Network Thread)
            std::function<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)> statusCbFunc =
                [state, remoteId](dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
                if (id == remoteId) {
                    {
                        std::lock_guard<std::mutex> lock(state->mtx);
                        state->complete = true;
                        if (status == TransportMonitor::Status::SUCCESS)
                            state->success.store(true);
                    }
                    // 9. [Network Thread] Notify the waiting caller thread.
                    state->cv.notify_one();
                }
                };

            // 4. [Caller Thread] Register the callback.
            auto delegate = dmq::MakeDelegate(statusCbFunc);
            m_transportMonitor.SendStatusCb += delegate;

            // 5. [Caller Thread] Define the "Send" logic lambda.
            auto* epPtr = &endpoint;
            std::function<bool(Args...)> asyncCallFunc = [epPtr](Args... fwdArgs) -> bool {
                // 7. [Network Thread] Execute the send operation.
                (*epPtr)(fwdArgs...);
                return (epPtr->GetError() == dmq::DelegateError::SUCCESS);
                };

            // 6. [Caller Thread] Dispatch the lambda to the Network Thread queue.
            auto retVal = dmq::MakeDelegate(asyncCallFunc, m_thread, SEND_TIMEOUT)
                .AsyncInvoke(std::forward<Args>(args)...);

            if (retVal.has_value() && retVal.value() == true)
            {
                // 8. [Caller Thread] BLOCK and Wait.
                std::unique_lock<std::mutex> lock(state->mtx);
                while (!state->complete) {
                    if (state->cv.wait_for(lock, RECV_TIMEOUT) == std::cv_status::timeout) {
                        state->complete = true; // Timeout occurred
                    }
                }
                // 10. [Caller Thread] Wake up! The wait is over.
            }

            // 11. [Caller Thread] Cleanup and return result.
            m_transportMonitor.SendStatusCb -= delegate;
            return state->success.load();
        }
        else
        {
            // 12. [Network Thread] Alternative Path:
            //     We are already on the correct thread, so execute immediately.
            endpoint(std::forward<Args>(args)...);
            return (endpoint.GetError() == dmq::DelegateError::SUCCESS);
        }
    }

protected:
    Thread m_thread;
    Dispatcher m_dispatcher;
    TransportMonitor m_transportMonitor;

    virtual void OnError(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux);
    virtual void OnStatus(dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status);

private:
    void RecvThread();
    void Incoming(DmqHeader& header, std::shared_ptr<xstringstream> arg_data);
    void Timeout();
    void InternalErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux);
    void InternalStatusHandler(dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status);

    std::thread* m_recvThread = nullptr;
    std::atomic<bool> m_recvThreadExit{ false };
    Timer m_timeoutTimer;
    dmq::ScopedConnection m_timeoutTimerConn;

    ZeroMqTransport m_sendTransport;
    ZeroMqTransport m_recvTransport;

    std::map<dmq::DelegateRemoteId, dmq::IRemoteInvoker*> m_receiveIdMap;

    static const std::chrono::milliseconds SEND_TIMEOUT;
    static const std::chrono::milliseconds RECV_TIMEOUT;
};

#endif // NETWORK_ENGINE_H