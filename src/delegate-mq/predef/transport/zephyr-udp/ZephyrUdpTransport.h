#ifndef ZEPHYR_UDP_TRANSPORT_H
#define ZEPHYR_UDP_TRANSPORT_H

/// @file ZephyrUdpTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
/// 
/// @brief Zephyr RTOS UDP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the Zephyr Networking Subsystem
/// (BSD Socket API). It is designed for embedded targets running Zephyr RTOS.
/// 
/// **Prerequisites:**
/// * Enable BSD Sockets: `CONFIG_NET_SOCKETS=y`
/// * Enable POSIX Names: `CONFIG_NET_SOCKETS_POSIX_NAMES=y` (Default)
/// * Enable IPv4: `CONFIG_NET_IPV4=y`
/// 
/// **Key Features:**
/// 1. **Direct Execution**: Executes network operations directly on the calling thread,
///    avoiding context switch overhead and preventing deadlocks.
/// 2. **Zero-Copy Friendly**: Uses the standard BSD API which Zephyr optimizes internally.
/// 3. **Reliability**: Fully integrated with `TransportMonitor` for ACKs/Retries.
/// 4. **Endianness**: Uses `htons`/`ntohs` for standard network byte order compatibility.

#include "DelegateMQ.h"
#include "predef/transport/ITransportMonitor.h"

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cerrno>

class ZephyrUdpTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    ZephyrUdpTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~ZephyrUdpTransport()
    {
        Close();
    }

    int Create(Type type, const char* addr, uint16_t port)
    {
        m_type = type;

        // Create UDP socket using Zephyr BSD API
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket < 0)
        {
            // printk("Socket creation failed: %d\n", errno);
            return -1;
        }

        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);

        if (type == Type::PUB)
        {
            // inet_pton is standard in Zephyr's socket.h
            if (inet_pton(AF_INET, addr, &m_addr.sin_addr) != 1)
            {
                // printk("Invalid IP address format.\n");
                return -1;
            }

            // Set a short timeout (e.g. 50ms) so Sender::Send() doesn't hang
            // while polling for ACKs.
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000; // 50ms

            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                // printk("setsockopt(SO_RCVTIMEO) failed\n");
                return -1;
            }
        }
        else if (type == Type::SUB)
        {
            m_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(m_socket, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0)
            {
                // printk("Bind failed: %d\n", errno);
                return -1;
            }

            // Set a 2-second receive timeout
            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;

            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                // printk("setsockopt(SO_RCVTIMEO) failed\n");
                return -1;
            }
        }

        return 0;
    }

    void Close()
    {
        if (m_socket >= 0)
        {
            // zsock_shutdown helps wake up blocked threads
            zsock_shutdown(m_socket, ZSOCK_SHUT_RDWR);
            zsock_close(m_socket);
            m_socket = -1;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail()) {
            return -1;
        }

        // Allow ACKs on SUB sockets. Block only regular data.
        if (m_type == Type::SUB && header.GetId() != dmq::ACK_REMOTE_ID) {
            return -1;
        }

        if (m_sendTransport != this) {
            return -1;
        }

        // Create a local copy to modify the length
        DmqHeader headerCopy = header;

        // Calculate payload size and set it
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Convert to Network Byte Order (Big Endian)
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id     = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t length = htons(headerCopy.GetLength());

        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        ss.write(reinterpret_cast<const char*>(&length), sizeof(length));

        // Append Payload
        ss.write(payload.data(), payload.size());

        std::string data = ss.str();

        // Always track the message (unless it is an ACK)
        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        ssize_t sent = sendto(m_socket, data.c_str(), data.size(), 0,
            (struct sockaddr*)&m_addr, sizeof(m_addr));

        return (sent == (ssize_t)data.size()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (m_recvTransport != this) {
            return -1;
        }

        sockaddr_in fromAddr;
        socklen_t addrLen = sizeof(fromAddr);
        
        ssize_t size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0,
            (struct sockaddr*)&fromAddr, &addrLen);

        if (size < 0)
        {
            // Zephyr uses standard errno values
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return -1; // Timeout
            
            return -1;
        }

        // Important: Update m_addr to the sender's address so we can ACK back
        if (m_type == Type::SUB) {
            m_addr = fromAddr;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(m_buffer, size);
        headerStream.seekg(0);

        uint16_t val = 0;

        // 1. Read Marker (Convert Network -> Host)
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetMarker(ntohs(val));

        if (header.GetMarker() != DmqHeader::MARKER)
        {
            return -1; // Invalid marker
        }

        // 2. Read ID
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetId(ntohs(val));

        // 3. Read SeqNum
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetSeqNum(ntohs(val));

        // 4. Read Length
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetLength(ntohs(val));

        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        // Logic check using Host values
        uint16_t id = header.GetId();
        uint16_t seqNum = header.GetSeqNum();

        if (id == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else if (m_transportMonitor && m_sendTransport)
        {
            // Send ACK
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(seqNum);
            m_sendTransport->Send(ss_ack, ack);
        }

        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        m_transportMonitor = transportMonitor;
    }

    void SetSendTransport(ITransport* sendTransport)
    {
        m_sendTransport = sendTransport;
    }

    void SetRecvTransport(ITransport* recvTransport)
    {
        m_recvTransport = recvTransport;
    }

private:
    int m_socket = -1;
    sockaddr_in m_addr{};
    Type m_type = Type::PUB;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 1500; // Ethernet MTU size
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // ZEPHYR_UDP_TRANSPORT_H