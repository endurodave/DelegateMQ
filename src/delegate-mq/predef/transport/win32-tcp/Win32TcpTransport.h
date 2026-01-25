#ifndef WIN32_TCP_TRANSPORT_H
#define WIN32_TCP_TRANSPORT_H

/// @file 
/// @brief Win32 TCP transport (Non-blocking Create). 

#if !defined(_WIN32) && !defined(_WIN64)
#error This code must be compiled as a Win32 or Win64 application.
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <windows.h>
#include <sstream>
#include <cstdio>
#include <iostream>

class TcpTransport : public ITransport
{
public:
    enum class Type
    {
        SERVER,
        CLIENT
    };

    TcpTransport() : m_thread("TcpTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread(std::chrono::milliseconds(5000));
    }

    ~TcpTransport()
    {
        Close();
        m_thread.ExitThread();
    }

    int Create(Type type, LPCSTR addr, USHORT port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

        WSADATA wsaData;
        m_type = type;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            std::cerr << "WSAStartup failed." << std::endl;
            return -1;
        }

        // Setup the Service Address
        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_port = htons(port);
        inet_pton(AF_INET, addr, &service.sin_addr);

        if (type == Type::SERVER)
        {
            // Only Bind and Listen here. Do NOT Accept yet.
            m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (m_listenSocket == INVALID_SOCKET) return -1;

            if (bind(m_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
            {
                std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
                Close();
                return -1;
            }

            if (listen(m_listenSocket, 1) == SOCKET_ERROR)
            {
                std::cerr << "Listen failed." << std::endl;
                Close();
                return -1;
            }
            std::cout << "Server listening on " << port << " (Async accept)..." << std::endl;
        }
        else if (type == Type::CLIENT)
        {
            // For Client, we attempt to connect immediately. 
            // If the server isn't ready, this might fail. Retry logic is handled by the app or user.
            m_clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (connect(m_clientSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
            {
                // It's common for this to fail if the server hasn't started listening yet.
                // We'll leave m_clientSocket as INVALID and try again in Send/Receive if needed,
                // or just report error.
                std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
                Close();
                return -1;
            }
            std::cout << "Connected to server." << std::endl;

            // Set timeout
            DWORD timeout = 2000;
            setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        }

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        if (m_clientSocket != INVALID_SOCKET) {
            closesocket(m_clientSocket);
            m_clientSocket = INVALID_SOCKET;
        }
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
        WSACleanup();
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (m_clientSocket == INVALID_SOCKET) return -1;

        // 1. Copy Header & Set Length
        DmqHeader headerCopy = header;
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) return -1;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        // 2. Serialize
        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        auto marker = headerCopy.GetMarker(); ss.write((char*)&marker, 2);
        auto id = headerCopy.GetId();         ss.write((char*)&id, 2);
        auto seq = headerCopy.GetSeqNum();    ss.write((char*)&seq, 2);
        auto len = headerCopy.GetLength();    ss.write((char*)&len, 2);
        ss.write(payload.data(), payload.size());

        // 3. Send
        std::string data = ss.str();
        const char* ptr = data.c_str();
        int remaining = (int)data.length();

        while (remaining > 0)
        {
            int sent = send(m_clientSocket, ptr, remaining, 0);
            if (sent == SOCKET_ERROR) return -1;
            ptr += sent;
            remaining -= sent;
        }

        // Monitor Ack
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(seq, id);

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &TcpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        // Lazy Accept Logic
        if (m_type == Type::SERVER && m_clientSocket == INVALID_SOCKET)
        {
            // Check if we have a pending connection
            // We use select() here to avoid blocking indefinitely if no client is waiting
            TIMEVAL tv = { 0, 10000 }; // 10ms poll
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_listenSocket, &fds);

            int res = select(0, &fds, NULL, NULL, &tv);
            if (res > 0)
            {
                m_clientSocket = accept(m_listenSocket, NULL, NULL);
                if (m_clientSocket != INVALID_SOCKET) {
                    std::cout << "Client accepted!" << std::endl;
                    DWORD timeout = 2000;
                    setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
                }
            }
        }

        if (m_clientSocket == INVALID_SOCKET) return -1;

        // 1. Read Header
        char headerBuf[DmqHeader::HEADER_SIZE];
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE)) return -1;

        xstringstream hs(std::ios::in | std::ios::out | std::ios::binary);
        hs.write(headerBuf, DmqHeader::HEADER_SIZE);
        hs.seekg(0);

        uint16_t val;
        hs.read((char*)&val, 2); header.SetMarker(val);
        if (header.GetMarker() != DmqHeader::MARKER) return -1;

        hs.read((char*)&val, 2); header.SetId(val);
        hs.read((char*)&val, 2); header.SetSeqNum(val);
        hs.read((char*)&val, 2); header.SetLength(val);

        // 2. Read Payload
        uint16_t length = header.GetLength();
        if (length > 0)
        {
            if (length > BUFFER_SIZE) return -1;
            if (!ReadExact(m_buffer, length)) return -1;
            is.write(m_buffer, length);
        }

        // 3. Ack
        if (header.GetId() == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_transportMonitor && m_sendTransport) {
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(header.GetSeqNum());
            m_sendTransport->Send(ss_ack, ack);
        }

        return 0;
    }

    // Setters omitted for brevity (same as before)
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
    bool ReadExact(char* dest, int size)
    {
        int total = 0;
        while (total < size) {
            int r = recv(m_clientSocket, dest + total, size - total, 0);
            if (r <= 0) return false;
            total += r;
        }
        return true;
    }

    SOCKET m_listenSocket = INVALID_SOCKET;
    SOCKET m_clientSocket = INVALID_SOCKET;
    Type m_type = Type::SERVER;
    Thread m_thread;
    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif