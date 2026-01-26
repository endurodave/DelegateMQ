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
/// 1. **Active Object**: Uses an internal worker thread to manage connection state 
///    and perform blocking I/O, ensuring the main application thread remains responsive.
/// 2. **Low Latency**: Configures `TCP_NODELAY` to disable Nagle's algorithm, optimized 
///    for the small, frequent packets typical of RPC/delegate calls.
/// 3. **Non-Blocking Poll**: Utilizes `select()` with a short timeout in the receive 
///    loop to allow for cooperative multitasking and clean shutdowns.
/// 4. **Reliability**: Fully integrated with `TransportMonitor` to handle sequence 
///    tracking and ACK generation.
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

class TcpTransport : public ITransport
{
public:
    enum class Type { SERVER, CLIENT };

    TcpTransport() : m_thread("TcpTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread();
    }

    ~TcpTransport() { Close(); m_thread.ExitThread(); }

    int Create(Type type, const char* addr, uint16_t port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

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

            // Note: accept() happens on the worker thread
            m_connFd = accept(m_socket, nullptr, nullptr);
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

        return (m_connFd < 0) ? -1 : 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        if (m_connFd >= 0 && m_connFd != m_socket) close(m_connFd);
        if (m_socket >= 0) close(m_socket);
        m_connFd = m_socket = -1;
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (m_connFd < 0) return -1;

        std::string payload = os.str();
        DmqHeader headerCopy = header;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::binary);
        uint16_t m = headerCopy.GetMarker(), i = headerCopy.GetId();
        uint16_t s = headerCopy.GetSeqNum(), l = headerCopy.GetLength();

        ss.write((char*)&m, 2);
        ss.write((char*)&i, 2);
        ss.write((char*)&s, 2);
        ss.write((char*)&l, 2);
        ss.write(payload.data(), payload.size());

        std::string packet = ss.str();

        if (i != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(s, i);

        ssize_t sent = write(m_connFd, packet.data(), packet.size());
        return (sent == (ssize_t)packet.size()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_connFd < 0) return -1;

        // --- Poll check to prevent blocking if no data is waiting ---
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_connFd, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000; // 1ms poll

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
        ss.read((char*)&val, 2); header.SetMarker(val);
        if (header.GetMarker() != DmqHeader::MARKER) return -1;

        ss.read((char*)&val, 2); header.SetId(val);
        ss.read((char*)&val, 2); header.SetSeqNum(val);
        ss.read((char*)&val, 2); header.SetLength(val);

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
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(tm);
        m_transportMonitor = tm;
    }
    void SetSendTransport(ITransport* st) {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(st);
        m_sendTransport = st;
    }
    void SetRecvTransport(ITransport* rt) {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(rt);
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
    Thread m_thread;
    ITransport* m_sendTransport, * m_recvTransport;
    ITransportMonitor* m_transportMonitor = nullptr;
};

#endif