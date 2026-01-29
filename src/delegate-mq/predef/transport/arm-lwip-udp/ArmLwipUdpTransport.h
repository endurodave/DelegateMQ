#ifndef ARM_LWIP_UDP_TRANSPORT_H
#define ARM_LWIP_UDP_TRANSPORT_H

/// @file ArmLwipUdpTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
/// 
/// @brief ARM lwIP UDP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the lwIP (Lightweight IP) 
/// BSD-style socket API. It is designed for embedded ARM targets running FreeRTOS 
/// (or similar) with the lwIP stack.
/// 
/// Prerequisites:
/// - lwIP must be compiled with `LWIP_SOCKET=1`
/// - lwIP must be compiled with `LWIP_SO_RCVTIMEO=1` (for non-blocking timeouts)
/// 
/// Key Features:
/// 1. **Active Object**: Uses an internal worker thread (dmq::Thread) to separate 
///    network I/O from the application tasks.
/// 2. **Memory Efficient**: Reduced buffer size to 1500 bytes (MTU) to fit constrained RAM.
/// 3. **Reliability Support**: Fully compatible with `TransportMonitor` for ACKs/Retries.
/// 4. **Endianness Safe**: Uses network byte order (htons/ntohs) for headers to support 
///    mixed-architecture (ARM <-> x86) communication.

#include "DelegateMQ.h"
#include "predef/transport/ITransportMonitor.h"

// lwIP Includes
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/sys.h"
#include "lwip/api.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

// Ensure lwIP is configured correctly
#if !defined(LWIP_SO_RCVTIMEO) || LWIP_SO_RCVTIMEO == 0
    #error "ArmLwipUdpTransport requires LWIP_SO_RCVTIMEO=1 in lwipopts.h"
#endif

class UdpTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    UdpTransport() : m_thread("LwipUdpTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread();
    }

    ~UdpTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, const char* addr, uint16_t port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

        m_type = type;

        // Create lwIP UDP socket
        m_socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0)
        {
            // std::cerr << "Socket creation failed: " << errno << std::endl;
            return -1;
        }

        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);

        if (type == Type::PUB)
        {
            // Note: inet_aton in lwIP returns 1 on success
            if (inet_aton(addr, &m_addr.sin_addr) == 0)
            {
                // std::cerr << "Invalid IP address format." << std::endl;
                return -1;
            }

            // Set a short timeout (e.g. 2ms) so Sender::Send() doesn't hang
            // while polling for ACKs.
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000; // 50ms

            if (lwip_setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                // std::cerr << "setsockopt(SO_RCVTIMEO) failed" << std::endl;
                return -1;
            }
        }
        else if (type == Type::SUB)
        {
            m_addr.sin_addr.s_addr = INADDR_ANY;

            if (lwip_bind(m_socket, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0)
            {
                // std::cerr << "Bind failed" << std::endl;
                return -1;
            }

            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;

            if (lwip_setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                // std::cerr << "setsockopt(SO_RCVTIMEO) failed" << std::endl;
                return -1;
            }
        }

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        if (m_socket >= 0)
        {
            lwip_close(m_socket);
            m_socket = -1;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

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

        // Create a local copy so we can modify the length
        DmqHeader headerCopy = header;

        // Calculate payload size and set it
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Convert header fields to Network Byte Order (Big Endian) 
        // to support cross-platform communication (e.g. ARM <-> x86)
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
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        ssize_t sent = lwip_sendto(m_socket, data.c_str(), data.size(), 0,
            (struct sockaddr*)&m_addr, sizeof(m_addr));

        return (sent == (ssize_t)data.size()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_recvTransport != this) {
            return -1;
        }

        sockaddr_in fromAddr;
        socklen_t addrLen = sizeof(fromAddr);
        
        // lwip_recvfrom
        ssize_t size = lwip_recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0,
            (struct sockaddr*)&fromAddr, &addrLen);

        if (size < 0)
        {
            // Explicit check for timeout vs real error
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return -1; // Timeout (No data)
            
            // Real socket error
            return -1; 
        }

        // Important: Update m_addr to the sender's address so we can ACK back
        if (m_type == Type::SUB) {
            m_addr = fromAddr;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(m_buffer, size);
        headerStream.seekg(0);

        uint16_t netVal = 0;

        // Read Marker (Convert Network -> Host)
        headerStream.read(reinterpret_cast<char*>(&netVal), sizeof(netVal));
        header.SetMarker(ntohs(netVal));

        if (header.GetMarker() != DmqHeader::MARKER)
        {
            return -1; // Sync marker mismatch
        }

        // Read ID
        headerStream.read(reinterpret_cast<char*>(&netVal), sizeof(netVal));
        header.SetId(ntohs(netVal));

        // Read SeqNum
        headerStream.read(reinterpret_cast<char*>(&netVal), sizeof(netVal));
        header.SetSeqNum(ntohs(netVal));

        // Read Length
        headerStream.read(reinterpret_cast<char*>(&netVal), sizeof(netVal));
        header.SetLength(ntohs(netVal));

        // Security Check: Ensure received packet is large enough for the claimed payload
        if (size < DmqHeader::HEADER_SIZE + header.GetLength())
        {
            return -1; // Malformed packet / Partial payload
        }

        // Extract payload
        is.write(m_buffer + DmqHeader::HEADER_SIZE, header.GetLength());

        if (header.GetId() == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_transportMonitor && m_sendTransport)
        {
            // Send ACK back
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(header.GetSeqNum());
            ack.SetLength(0);
            m_sendTransport->Send(ss_ack, ack);
        }

        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(transportMonitor);
        m_transportMonitor = transportMonitor;
    }

    void SetSendTransport(ITransport* sendTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(sendTransport);
        m_sendTransport = sendTransport;
    }

    void SetRecvTransport(ITransport* recvTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(recvTransport);
        m_recvTransport = recvTransport;
    }

private:
    int m_socket = -1;
    sockaddr_in m_addr{};
    Type m_type = Type::PUB;
    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    // Reduced to 1500 (Standard Ethernet MTU) for embedded environments
    static const int BUFFER_SIZE = 1500;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // ARM_LWIP_UDP_TRANSPORT_H
