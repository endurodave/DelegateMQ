#ifndef ZEROMQ_TRANSPORT_H
#define ZEROMQ_TRANSPORT_H

/// @file ZeroMqTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the ZeroMQ high-performance 
/// asynchronous messaging library. It supports multiple patterns including PAIR (1-to-1) 
/// and PUB/SUB (1-to-many).
/// 
/// Key Features:
/// 1. **Thread Safety**: Uses a `std::recursive_mutex` to protect the ZeroMQ socket, 
///    allowing safe concurrent access from the Send and Receive threads (ZeroMQ sockets 
///    are not inherently thread-safe).
/// 2. **Flexible Patterns**: Supports both bidirectional (PAIR) and unidirectional (PUB/SUB)
///    communication topologies.
/// 3. **Non-Blocking**: Configures `ZMQ_RCVTIMEO` and `ZMQ_DONTWAIT` to ensure the 
///    transport remains responsive and never blocks the application indefinitely.
/// 4. **Reliability**: Integrates with `TransportMonitor` to provide sequence tracking 
///    and ACKs even over PUB/SUB (when a return channel is available).
/// 
/// @note Requires the `libzmq` library.

// Add Windows Sockets headers for htons/ntohs
#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
#endif

#include "delegate/DelegateOpt.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/DmqHeader.h"
#include "predef/transport/ITransportMonitor.h"
#include <zmq.h>
#include <sstream>
#include <cstdio>
#include <mutex>
#include <iostream> // For std::cout/cerr

/// @brief ZeroMQ transport class.
/// @details Logic now executes directly on the caller's thread, protected by a mutex
/// to prevent concurrent access to the underlying ZeroMQ socket.
class ZeroMqTransport : public ITransport
{
public:
    enum class Type
    {
        PAIR_CLIENT,
        PAIR_SERVER,
        PUB,
        SUB
    };

    ZeroMqTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~ZeroMqTransport()
    {
        Destroy();
    }

    int Create(Type type, const char* addr)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (m_zmqContext == nullptr) {
            m_zmqContext = zmq_ctx_new();
        }

        m_type = type;

        if (m_type == Type::PAIR_CLIENT)
        {
            // Create a PAIR socket and connect the client to the server address
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PAIR);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                // perror is less useful on Windows for ZMQ specific errors, but ok for console
                perror("zmq_connect failed");
                zmq_close(m_zmq);
                return 1;
            }

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else if (m_type == Type::PAIR_SERVER)
        {
            // Create a PAIR socket and bind the server to an address
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PAIR);
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed");
                return 1;
            }

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else if (m_type == Type::PUB)
        {
            // Create a PUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PUB);
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed");
                return 1;
            }
        }
        else if (m_type == Type::SUB)
        {
            // Create a SUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_SUB);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_connect failed");
                zmq_close(m_zmq);
                return 1;
            }

            // Subscribe to all messages
            zmq_setsockopt(m_zmq, ZMQ_SUBSCRIBE, "", 0);

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else
        {
            // Handle other types, if needed
            perror("Invalid socket type");
            return 1;
        }
        return 0;
    }

    void Close()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // Close the subscriber socket and context
        if (m_zmq) {
            // Linger 0 ensures close doesn't block waiting for unsent messages
            int linger = 0;
            zmq_setsockopt(m_zmq, ZMQ_LINGER, &linger, sizeof(linger));
            zmq_close(m_zmq);
            m_zmq = nullptr;
        }
    }

    void Destroy()
    {
        Close();

        // Context termination is thread-safe in ZMQ
        if (m_zmqContext) {
            zmq_ctx_term(m_zmqContext);
            m_zmqContext = nullptr;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (os.bad() || os.fail()) {
            std::cout << "Error: xostringstream is in a bad state!" << std::endl;
            return -1;
        }

        if (m_zmq == nullptr) {
            return -1;
        }

        if (m_type == Type::SUB) {
            std::cout << "Error: Cannot send on SUB socket!" << std::endl;
            return -1;
        }

        if (m_sendTransport != this) {
            std::cout << "Error: This transport used for receive only!" << std::endl;
            return -1;
        }

        // Create a local copy to modify the length
        DmqHeader headerCopy = header;

        // Calculate payload size and set it on the copy
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Error: Payload too large for 16-bit length." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write header values from the COPY
        auto marker = htons(headerCopy.GetMarker());
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = htons(headerCopy.GetId());
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = htons(headerCopy.GetSeqNum());
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        auto len = htons(headerCopy.GetLength());
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Insert delegate arguments (payload)
        ss.write(payload.data(), payload.size());

        size_t length = ss.str().length();

        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());
        }

        // Send delegate argument data using ZeroMQ
        int err = zmq_send(m_zmq, ss.str().c_str(), length, ZMQ_DONTWAIT);
        if (err == -1)
        {
            // EAGAIN is common if queue is full, treat as error so RetryMonitor retries
            return zmq_errno();
        }
        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (m_zmq == nullptr) {
            // std::cout << "Error: Socket not created!" << std::endl;
            return -1;
        }

        if (m_type == Type::PUB) {
            std::cout << "Error: Cannot receive on PUB socket!" << std::endl;
            return -1;
        }

        if (m_recvTransport != this) {
            std::cout << "Error: This transport used for send only!" << std::endl;
            return -1;
        }

        int size = zmq_recv(m_zmq, m_buffer, BUFFER_SIZE, 0);
        if (size == -1) {
            // Check if the error is due to a timeout
            if (zmq_errno() == EAGAIN) {
                //std::cout << "Receive timeout!" << std::endl;
            }
            return -1;
        }

        if (size < DmqHeader::HEADER_SIZE) {
            std::cerr << "Received data is too small to process." << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        // Write the received header data into the stringstream
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(ntohs(marker));

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;  // @TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(ntohs(id));

        // Read seqNum using the getter for byte swapping
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(ntohs(seqNum));

        // Read length using the getter for byte swapping
        uint16_t length = 0;
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetLength(ntohs(length));

        // Write the remaining target function argument data to stream
        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (header.GetId() == dmq::ACK_REMOTE_ID)
        {
            // Receiver ack'ed message. Remove sequence number from monitor.
            if (m_transportMonitor)
                m_transportMonitor->Remove(header.GetSeqNum());
        }
        else
        {
            if (m_transportMonitor && m_sendTransport)
            {
                // Send header with received seqNum as the ack message
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(header.GetSeqNum());

                // Note: Recursive mutex allows us to call Send() if m_sendTransport == this.
                // If m_sendTransport is a different instance, that instance's lock will be taken.
                m_sendTransport->Send(ss_ack, ack);
            }
        }

        return 0;  // Success
    }

    // Set a transport monitor for checking message ack
    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        m_transportMonitor = transportMonitor;
    }

    // Set an alternative send transport
    void SetSendTransport(ITransport* sendTransport)
    {
        m_sendTransport = sendTransport;
    }

    // Set an alternative receive transport
    void SetRecvTransport(ITransport* recvTransport)
    {
        m_recvTransport = recvTransport;
    }

private:
    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;
    Type m_type = Type::PAIR_CLIENT;

    std::recursive_mutex m_mutex;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = {};
};

#endif