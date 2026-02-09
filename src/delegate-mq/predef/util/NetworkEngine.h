/// @file NetworkEngine.h
/// @brief Base class for handling network transport, threading, and synchronization.
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#ifndef NETWORK_ENGINE_H
#define NETWORK_ENGINE_H

// Only define NetworkEngine if a compatible transport is selected
#if defined(DMQ_TRANSPORT_ZEROMQ) || defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP) || defined(DMQ_TRANSPORT_STM32_UART) || defined(DMQ_TRANSPORT_SERIAL_PORT)

#include "predef/util/RemoteEndpoint.h"
#include "predef/util/TransportMonitor.h"
#include <map>
#include <mutex>
#include <atomic>
#include <future>
#include <iostream>
#include <functional> 

// SWITCH: Include the correct transport header based on CMake definitions
#if defined(DMQ_TRANSPORT_ZEROMQ)
#include "predef/transport/zeromq/ZeroMqTransport.h"
#elif defined(DMQ_TRANSPORT_WIN32_UDP)
#include "predef/transport/win32-udp/Win32UdpTransport.h"
#include "predef/util/ReliableTransport.h"
#include "predef/util/RetryMonitor.h"
#elif defined(DMQ_TRANSPORT_LINUX_UDP)
#include "predef/transport/linux-udp/LinuxUdpTransport.h"
#include "predef/util/ReliableTransport.h"
#include "predef/util/RetryMonitor.h"
#elif defined(DMQ_TRANSPORT_STM32_UART)
#include "predef/transport/stm32-uart/Stm32UartTransport.h"
#include "predef/util/ReliableTransport.h"
#include "predef/util/RetryMonitor.h"
#elif defined(DMQ_TRANSPORT_SERIAL_PORT)
#include "predef/transport/serial/SerialTransport.h"
#include "predef/util/ReliableTransport.h"
#include "predef/util/RetryMonitor.h"
#else
#error "Select a NetworkEngine transport"
#endif

/// @brief Base class for handling network transport, threading, and synchronization.
/// 
/// @details NetworkEngine encapsulates the "plumbing" of the distributed system, 
/// separating transport mechanics from application business logic. It provides a 
/// unified interface regardless of the underlying transport protocol selected at build time.
/// 
/// **Key Responsibilities:**
/// * **Lifecycle Management:** Controls the startup and shutdown of transport sockets and receiver threads.
/// * **Thread Synchronization:** Marshals all outgoing network calls to a dedicated network thread to ensure 
///   thread safety and prevent blocking the caller's UI or logic threads.
/// * **Message Routing:** Maps incoming data (by ID) to specific `RemoteEndpoint` instances via `RegisterEndpoint`.
/// * **Reliability:** Integrates with `TransportMonitor` to handle Acknowledgments (ACKs) and retransmissions/timeouts.
class NetworkEngine
{
public:
    NetworkEngine();
    virtual ~NetworkEngine();

    NetworkEngine(const NetworkEngine&) = delete;
    NetworkEngine& operator=(const NetworkEngine&) = delete;

    // SWITCH: Initialize signature differs between transports
#if defined(DMQ_TRANSPORT_ZEROMQ)
    // ZeroMQ uses connection strings (e.g., "tcp://*:5555")
    int Initialize(const std::string& sendAddr, const std::string& recvAddr, bool isServer);
#elif defined(DMQ_TRANSPORT_WIN32_UDP) || defined(DMQ_TRANSPORT_LINUX_UDP)
    // UDP requires explicit IP and Port for sending and receiving
    int Initialize(const std::string& sendIp, int sendPort, const std::string& recvIp, int recvPort);
#elif defined(DMQ_TRANSPORT_STM32_UART)
    // Initialize with HAL Handle
    int Initialize(UART_HandleTypeDef* huart);
#elif defined(DMQ_TRANSPORT_SERIAL_PORT)
    // Initialize with COM Port name and baud rate
    int Initialize(const std::string& portName, int baudRate);
#endif

    /// @brief Starts the network engine and its receiving thread.
    /// 
    /// @details This method initializes the internal `RecvThread` to begin polling the 
    /// transport layer for incoming messages. It also starts the `Timeout` timer used 
    /// by the `TransportMonitor` to track unacknowledged messages. 
    /// 
    /// @note This method is thread-safe and will automatically marshal the call to the 
    /// internal Network Thread if called from a different context.
    void Start();

    /// @brief Stops the network engine and releases resources.
    /// 
    /// @details This method gracefully shuts down the `RecvThread`, stops the timeout 
    /// timer, and closes the underlying transport sockets (send/receive). It ensures 
    /// that all internal threads are joined before returning to prevent resource leaks.
    /// 
    /// @note This call blocks until the shutdown sequence is complete.
    void Stop();

    /// @brief Registers a remote endpoint with the network engine.
    /// 
    /// @details This function maps a unique `DelegateRemoteId` to a specific `RemoteEndpoint` 
    /// instance (via the `IRemoteInvoker` interface). When the `NetworkEngine` receives 
    /// data from the transport layer, it uses the message ID to look up the registered 
    /// endpoint in this map and invokes it to deserialize and handle the payload.
    /// 
    /// @param[in] id The unique identifier for the remote message type.
    /// @param[in] endpoint Pointer to the endpoint instance responsible for handling this ID.
    void RegisterEndpoint(dmq::DelegateRemoteId id, dmq::IRemoteInvoker* endpoint);

    /// @brief Generic helper function to synchronously invoke a remote delegate.
    /// 
    /// @details This function blocks the calling thread until one of two conditions is met:
    /// 1. The remote endpoint acknowledges receipt of the message (ACK).
    /// 2. The operation times out (as defined by `RECV_TIMEOUT`).
    ///
    /// **Thread Synchronization Logic:**
    /// * **If called from the Network Thread:** The send operation executes immediately 
    ///     and returns the result of the transport send call. No blocking wait occurs 
    ///     because we are already on the thread responsible for I/O.
    /// * **If called from any other thread:** The call is marshaled to the Network Thread.
    ///     The calling thread blocks on a condition variable. When the Network Thread 
    ///     receives an ACK (or timeout), it signals the condition variable to wake up 
    ///     the caller.
    ///
    /// @tparam TClass The class type of the remote endpoint (usually inferred).
    /// @tparam RetType The return type of the function signature (usually void).
    /// @tparam Args The argument types of the function signature.
    /// @param[in] endpoint The specific `RemoteEndpoint` instance to invoke.
    /// @param[in] args The arguments to forward to the remote function.
    /// @return `true` if the remote acknowledged the message; `false` on timeout or transport failure.
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
                dmq::Mutex mtx;              // Generic Mutex
                dmq::ConditionVariable cv;   // Generic CV
                XALLOCATOR
            };
            auto state = std::make_shared<SyncState>();
            dmq::DelegateRemoteId remoteId = endpoint.GetRemoteId();

            // 3. [Caller Thread] Define the callback that wakes us up later.
            std::function<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)> statusCbFunc =
                [state, remoteId](dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
                if (id == remoteId) {
                    {
                        std::lock_guard<dmq::Mutex> lock(state->mtx);
                        state->complete = true;
                        if (status == TransportMonitor::Status::SUCCESS)
                            state->success.store(true);
                    }
                    // 9. [Network Thread] Notify the waiting caller thread.
                    state->cv.notify_one();
                }
                };

            // 4. [Caller Thread] Register the callback.
            dmq::ScopedConnection conn = m_transportMonitor.OnSendStatus->Connect(dmq::MakeDelegate(statusCbFunc));

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
                std::unique_lock<dmq::Mutex> lock(state->mtx);
                // wait_for returns false if the predicate is still false after the timeout
                state->cv.wait_for(lock, RECV_TIMEOUT, [&] {
                    return state->complete;
                    });
                // 10. [Caller Thread] Wake up! The wait is over.
            }

            // 11. [Caller Thread] Cleanup and return result.
            // 'conn' goes out of scope here and automatically disconnects
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

    Thread m_recvThread;
    std::atomic<bool> m_recvThreadExit{ false };
    Timer m_timeoutTimer;
    dmq::ScopedConnection m_timeoutTimerConn;

    // SWITCH: Transport Members
#if defined(DMQ_TRANSPORT_ZEROMQ)
    ZeroMqTransport m_sendTransport;
    ZeroMqTransport m_recvTransport;

    // ZeroMQ already has reliable communication

#elif defined(DMQ_TRANSPORT_WIN32_UDP)
    UdpTransport m_sendTransport;
    UdpTransport m_recvTransport;

    // Reliability Layers
    RetryMonitor m_retryMonitor;
    ReliableTransport m_reliableTransport;

#elif defined(DMQ_TRANSPORT_LINUX_UDP)
    UdpTransport m_sendTransport;
    UdpTransport m_recvTransport;

    // Reliability Layers
    RetryMonitor m_retryMonitor;
    ReliableTransport m_reliableTransport;

#elif defined(DMQ_TRANSPORT_STM32_UART)
    // Single Shared Transport Instance (Owns the buffers/state)
    Stm32UartTransport m_transport;

    // References (Aliases used by generic code)
    Stm32UartTransport& m_sendTransport;
    Stm32UartTransport& m_recvTransport;

    // Reliability Layers
    RetryMonitor m_retryMonitor;
    ReliableTransport m_reliableTransport;

#elif defined(DMQ_TRANSPORT_SERIAL_PORT)
    // Single Shared Transport Instance
    SerialTransport m_transport;

    // References required by generic code
    SerialTransport& m_sendTransport;
    SerialTransport& m_recvTransport;

    // Reliability Layers
    RetryMonitor m_retryMonitor;
    ReliableTransport m_reliableTransport;
#endif

    std::map<dmq::DelegateRemoteId, dmq::IRemoteInvoker*> m_receiveIdMap;
    dmq::ScopedConnection m_statusConn;

    static const std::chrono::milliseconds SEND_TIMEOUT;
    static const std::chrono::milliseconds RECV_TIMEOUT;
};

#endif // Defined Transport Check
#endif // NETWORK_ENGINE_H
