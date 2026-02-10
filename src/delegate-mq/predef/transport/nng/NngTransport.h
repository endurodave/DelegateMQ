#ifndef NNG_TRANSPORT_H
#define NNG_TRANSPORT_H

/// @file NngTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief NNG transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the NNG (Nanomsg Next Gen) 
/// lightweight messaging library. It supports multiple scalability protocols including 
/// PAIR (1-to-1 bidrectional) and PUB/SUB (1-to-many unidirectional).
/// 
/// Key Features:
/// 1. **Thread Safety**: Uses a `std::recursive_mutex` to protect the NNG socket, 
///    allowing safe concurrent access from the Send and Receive threads (NNG sockets 
///    are not inherently thread-safe).
/// 2. **Scalability Protocols**: flexible configuration for different topologies:
///    * `PAIR_CLIENT`/`PAIR_SERVER`: Exclusive 1-to-1 connection.
///    * `PUB`/`SUB`: Efficient distribution to multiple subscribers.
/// 3. **Non-Blocking**: Uses asynchronous messaging patterns provided by NNG.
/// 4. **Reliability**: Integrates with `TransportMonitor` to providing sequence tracking 
///    and ACKs even over PUB/SUB (when a return channel is available).
/// 
/// @note Requires the `libnng` library.

// Add Windows Sockets headers for htons/ntohs
#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
#endif

#include "delegate/DelegateOpt.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <sstream>
#include <cstdio>
#include <mutex>
#include <iostream>

/// @brief NNG transport class. 
/// @details Logic now executes directly on the caller's thread, protected by a mutex
/// to prevent concurrent access to the underlying NNG socket.
class NngTransport : public ITransport
{
public:
    enum class Type
    {
        PAIR_CLIENT,
        PAIR_SERVER,
        PUB,
        SUB
    };

    NngTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~NngTransport()
    {
        Destroy();
    }

    int Create(Type type, const char* addr)
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // Initialize NNG context
        m_type = type;

        int rc = 0;
        if (m_type == Type::PAIR_CLIENT)
        {
            rc = nng_pair0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG pair client socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_dial(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to dial address: " << nng_strerror(rc) << std::endl;
                nng_close(m_nngSocket);
                return rc;
            }
            // Set recv timeout to avoid blocking forever
            nng_duration timeout = 100; // 100ms
            nng_setopt_ms(m_nngSocket, NNG_OPT_RECVTIMEO, timeout);
        }
        else if (m_type == Type::PAIR_SERVER)
        {
            rc = nng_pair0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG pair server socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_listen(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to listen on address: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            // Set recv timeout
            nng_duration timeout = 100; // 100ms
            nng_setopt_ms(m_nngSocket, NNG_OPT_RECVTIMEO, timeout);
        }
        else if (m_type == Type::PUB)
        {
            rc = nng_pub0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG PUB socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_listen(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to listen on address: " << nng_strerror(rc) << std::endl;
                return rc;
            }
        }
        else if (m_type == Type::SUB)
        {
            rc = nng_sub0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG SUB socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_dial(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to dial address: " << nng_strerror(rc) << std::endl;
                return rc;
            }

            // Subscribe to all messages
            rc = nng_setopt(m_nngSocket, NNG_OPT_SUB_SUBSCRIBE, "", 0);
            if (rc != 0) {
                std::cerr << "Failed to set subscription filter: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            
            // Set recv timeout
            nng_duration timeout = 100; // 100ms
            nng_setopt_ms(m_nngSocket, NNG_OPT_RECVTIMEO, timeout);
        }
        else
        {
            std::cerr << "Invalid socket type" << std::endl;
            return 1;
        }

        return 0;
    }

    void Close()
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // Close the socket
        if (nng_socket_id(m_nngSocket) != 0) {
            nng_close(m_nngSocket);
        }
        m_nngSocket = NNG_SOCKET_INITIALIZER;
    }

    void Destroy()
    {
        Close();
        // NNG doesn't require explicit context terminations like ZMQ
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (os.bad() || os.fail()) {
            std::cout << "Error: xostringstream is in a bad state!" << std::endl;
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

        // Get payload and set length on the copy
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Error: Payload too large for 16-bit length." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write header values from the COPY (using htons)
        uint16_t marker = htons(headerCopy.GetMarker());
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        uint16_t id = htons(headerCopy.GetId());
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        uint16_t len = htons(headerCopy.GetLength());
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Insert delegate arguments (payload)
        ss.write(payload.data(), payload.size());

        std::string fullPacket = ss.str();

        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());
        }

        // Send data using NNG
        // Use NNG_FLAG_NONBLOCK to prevent deadlocks if peer is gone
        int err = nng_send(m_nngSocket, (void*)fullPacket.data(), fullPacket.size(), NNG_FLAG_NONBLOCK);
        if (err != 0)
        {
            // NNG_EAGAIN means queue full (congestion), treat as error to trigger retry logic
            if (err != NNG_EAGAIN) {
                // std::cerr << "nng_send failed with error: " << nng_strerror(err) << std::endl;
            }
            return err;
        }

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        // Lock Guard
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (nng_socket_id(m_nngSocket) == 0) {
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

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        size_t size = sizeof(m_buffer);
        
        // Receive with 100ms timeout (set in Create)
        int err = nng_recv(m_nngSocket, m_buffer, &size, 0);
        if (err != 0) {
            if (err == NNG_ETIMEDOUT) {
                // Just a timeout, not a fatal error
                return -1;
            }
            // std::cerr << "nng_recv failed with error: " << nng_strerror(err) << std::endl;
            return -1;
        }

        if (size < DmqHeader::HEADER_SIZE) {
            std::cerr << "Received data is too small to process." << std::endl;
            return -1;
        }

        // Write the received header data into the stringstream
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t val = 0;

        // Read Marker (Convert Network -> Host)
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetMarker(ntohs(val));

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;  // @TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetId(ntohs(val));

        // Read seqNum using the getter for byte swapping
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetSeqNum(ntohs(val));

        // Read length using the getter for byte swapping
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetLength(ntohs(val));

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
                
                // Note: Recursive mutex allows Send() here
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
    nng_socket m_nngSocket = NNG_SOCKET_INITIALIZER;
    Type m_type = Type::PAIR_CLIENT;

    // Mutex to protect non-thread-safe socket
    std::recursive_mutex m_mutex;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = {};
};

#endif