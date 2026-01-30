#ifndef LINUX_TCP_TRANSPORT_H
#define LINUX_TCP_TRANSPORT_H

/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief Linux TCP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using standard Linux BSD sockets. 
/// It supports both CLIENT and SERVER modes for reliable, stream-based communication.
/// 
/// Key Features:
/// 1. **Thread-Safe I/O**: Executes socket operations directly on the calling thread, 
///    relying on OS-level thread safety for concurrent Send/Receive operations.
/// 2. **Low Latency**: Configures `TCP_NODELAY` to disable Nagle's algorithm, optimized 
///    for the small, frequent packets typical of RPC/delegate calls.
/// 3. **Non-Blocking Poll**: Utilizes `select()` with a 1-second timeout in the receive 
///    loop to allow for cooperative multitasking and clean shutdowns without busy waiting.
/// 4. **Reliability**: Fully integrated with `TransportMonitor` to handle sequence 
///    tracking and ACK generation.
/// 
/// @note This class is specific to Linux and uses POSIX socket APIs.

#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <vector>

#include "DelegateMQ.h"
#include "predef/transport/ITransportMonitor.h"

class TcpTransport : public ITransport
{
public:
    enum class Type { SERVER, CLIENT };

    TcpTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~TcpTransport() { Close(); }

    int Create(Type type, const char* addr, uint16_t port)
    {
        m_type = type;
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) return -1;

        // Set TCP_NODELAY to disable Nagle's algorithm for low-latency RPC
        int flag = 1;
        setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

        sockaddr_in srv_addr{};
        srv_addr.sin_family = AF_INET;
        srv_addr.sin_port = htons(port);
        inet_aton(addr, &srv_addr.sin_addr);

        if (type == Type::SERVER) {
            int opt = 1;
            setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if (bind(m_socket, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) return -1;
            if (listen(m_socket, 1) < 0) return -1;

            // We will accept lazily in Receive().
        }
        else {
            if (connect(m_socket, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) return -1;
            m_connFd = m_socket;
        }

        // Safety: Set a read timeout so ReadExact doesn't hang forever on partial packets
        if (m_connFd >= 0) {
            struct timeval tv;
            tv.tv_sec = 2; tv.tv_usec = 0;
            setsockopt(m_connFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        }

        return 0;
    }

    void Close()
    {
        // We must call this from the Main Thread to interrupt the Worker Thread.

        // Close Connected Socket (Breaks blocking recv())
        if (m_connFd >= 0) 
        {
            // SHUT_RDWR forces any blocking recv/send on this socket 
            // in the other thread to return immediately (usually with 0 or error).
            shutdown(m_connFd, SHUT_RDWR);
            
            if (m_connFd != m_socket) 
            {
                close(m_connFd);
            }
        }

        // Close Listen Socket (Breaks blocking accept())
        if (m_socket >= 0) 
        {
            shutdown(m_socket, SHUT_RDWR);
            close(m_socket);
        }

        // Reset descriptors
        m_connFd = m_socket = -1;
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (m_connFd < 0) return -1;

        std::string payload = os.str();
        DmqHeader headerCopy = header;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::binary);
        
        // Convert to Network Byte Order (Big Endian)
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id     = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t length = htons(headerCopy.GetLength());

        ss.write((char*)&marker, 2);
        ss.write((char*)&id, 2);
        ss.write((char*)&seqNum, 2);
        ss.write((char*)&length, 2);
        ss.write(payload.data(), payload.size());

        std::string packet = ss.str();

        // Always track the message (unless it is an ACK)
        // Use Host Byte Order for ID check
        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        // write() is thread-safe on Linux sockets
        ssize_t sent = write(m_connFd, packet.data(), packet.size());
        return (sent == (ssize_t)packet.size()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        // Lazy Accept Logic (Server Mode)
        if (m_type == Type::SERVER && m_connFd < 0) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_socket, &fds);
            struct timeval tv = { 0, 1000 }; // 1ms poll

            if (select(m_socket + 1, &fds, nullptr, nullptr, &tv) > 0) {
                m_connFd = accept(m_socket, nullptr, nullptr);
                if (m_connFd >= 0) {
                    // Set timeout on new socket
                    struct timeval t; t.tv_sec = 2; t.tv_usec = 0;
                    setsockopt(m_connFd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof(t));
                }
            }
        }

        if (m_connFd < 0) return -1;

        // --- Poll check to prevent blocking if no data is waiting ---
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_connFd, &readfds);
        struct timeval tv;
        tv.tv_sec = 1; // Increased to 1s to prevent busy loop
        tv.tv_usec = 0; 

        // If no data is waiting, return -1 immediately so Sender loop continues
        if (select(m_connFd + 1, &readfds, NULL, NULL, &tv) <= 0) {
            return -1;
        }

        // 1. Read Fixed-Size Header for Framing
        char headerBuf[DmqHeader::HEADER_SIZE];
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE)) return -1;

        xstringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        ss.write(headerBuf, DmqHeader::HEADER_SIZE);
        ss.seekg(0);

        uint16_t val;
        
        // Read Marker (Convert Network -> Host)
        ss.read((char*)&val, 2); header.SetMarker(ntohs(val));
        if (header.GetMarker() != DmqHeader::MARKER) return -1;

        ss.read((char*)&val, 2); header.SetId(ntohs(val));
        ss.read((char*)&val, 2); header.SetSeqNum(ntohs(val));
        ss.read((char*)&val, 2); header.SetLength(ntohs(val));

        // 2. Read Payload based on Header Length
        uint16_t length = header.GetLength();
        if (length > 0) {
            std::vector<char> payload(length);
            if (!ReadExact(payload.data(), length)) return -1;
            is.write(payload.data(), length);
        }

        // 3. Reliability Logic
        if (header.GetId() == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_sendTransport) {
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(header.GetSeqNum());
            m_sendTransport->Send(ss_ack, ack);
        }
        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* tm) {
        m_transportMonitor = tm;
    }
    void SetSendTransport(ITransport* st) {
        m_sendTransport = st;
    }
    void SetRecvTransport(ITransport* rt) {
        m_recvTransport = rt;
    }

private:
    bool ReadExact(char* buf, size_t len)
    {
        size_t total = 0;
        while (total < len) {
            ssize_t r = read(m_connFd, buf + total, len - total);
            if (r <= 0) return false; // Connection closed or error
            total += r;
        }
        return true;
    }

    int m_socket = -1;
    int m_connFd = -1;
    Type m_type = Type::SERVER;
    
    ITransport* m_sendTransport, * m_recvTransport;
    ITransportMonitor* m_transportMonitor = nullptr;
};

#endif // LINUX_TCP_TRANSPORT_H