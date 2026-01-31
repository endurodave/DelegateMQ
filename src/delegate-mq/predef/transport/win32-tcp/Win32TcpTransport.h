#ifndef WIN32_TCP_TRANSPORT_H
#define WIN32_TCP_TRANSPORT_H

/// @file Win32TcpTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief Win32 TCP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using Windows Sockets (Winsock2). 
/// It supports both CLIENT and SERVER modes for transmitting serialized delegate data.
/// 
/// Key Features:
/// 1. **Direct I/O**: Executes socket operations directly on the calling thread, 
///    relying on OS-level thread safety to avoid deadlocks.
/// 2. **Reliability Support**: Integrates with `TransportMonitor` to track sequence 
///    numbers and acknowledge (ACK) receipts.
/// 3. **Non-Blocking I/O**: Utilizes `select()` in the receive loop to prevent 
///    thread blocking when no data is available, facilitating clean shutdowns.
/// 4. **Socket Management**: Use WinsockConnect class in main() for `WSAStartup` and 
///    socket creation/cleanup.

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

    TcpTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~TcpTransport()
    {
        Close();
    }

    int Create(Type type, LPCSTR addr, USHORT port)
    {
        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_port = htons(port);
        inet_pton(AF_INET, addr, &service.sin_addr);

        if (type == Type::SERVER)
        {
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
            m_clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (connect(m_clientSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
            {
                std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
                Close();
                return -1;
            }
            std::cout << "Connected to server." << std::endl;

            DWORD timeout = 2000;
            setsockopt(m_clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        }

        return 0;
    }

    void Close()
    {    
        // Close Client Socket (Breaks recv())
        if (m_clientSocket != INVALID_SOCKET) {
            shutdown(m_clientSocket, SD_BOTH); 
            closesocket(m_clientSocket);
            m_clientSocket = INVALID_SOCKET;
        }

        // Close Listen Socket (Breaks accept())
        if (m_listenSocket != INVALID_SOCKET) {
            closesocket(m_listenSocket);
            m_listenSocket = INVALID_SOCKET;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (m_clientSocket == INVALID_SOCKET) return -1;

        DmqHeader headerCopy = header;
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) return -1;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Convert to Network Byte Order (Big Endian)
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id = htons(headerCopy.GetId());
        uint16_t seq = htons(headerCopy.GetSeqNum());
        uint16_t len = htons(headerCopy.GetLength());

        ss.write((char*)&marker, 2);
        ss.write((char*)&id, 2);
        ss.write((char*)&seq, 2);
        ss.write((char*)&len, 2);
        ss.write(payload.data(), payload.size());

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

        // Always track the message (unless it is an ACK)
        // Use Host Byte Order for ID check
        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        // Lazy Accept Logic (Server)
        if (m_type == Type::SERVER && m_clientSocket == INVALID_SOCKET)
        {
            TIMEVAL tv = { 0, 1000 }; // 1ms poll
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(m_listenSocket, &fds);

            if (select(0, &fds, NULL, NULL, &tv) > 0)
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

        // Poll check to prevent blocking if no data is waiting (Critical for Sender polling ACKs)
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_clientSocket, &readfds);
        TIMEVAL tv = { 1, 0 }; // 1s poll to prevent busy loop

        if (select(0, &readfds, NULL, NULL, &tv) <= 0) {
            return -1;
        }

        // 1. Read Header
        char headerBuf[DmqHeader::HEADER_SIZE];
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE)) return -1;

        xstringstream hs(std::ios::in | std::ios::out | std::ios::binary);
        hs.write(headerBuf, DmqHeader::HEADER_SIZE);
        hs.seekg(0);

        uint16_t val;

        // Read Marker (Convert Network -> Host)
        hs.read((char*)&val, 2); header.SetMarker(ntohs(val));
        if (header.GetMarker() != DmqHeader::MARKER) return -1;

        hs.read((char*)&val, 2); header.SetId(ntohs(val));
        hs.read((char*)&val, 2); header.SetSeqNum(ntohs(val));
        hs.read((char*)&val, 2); header.SetLength(ntohs(val));

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

    void SetTransportMonitor(ITransportMonitor* tm)
    {
        m_transportMonitor = tm;
    }

    void SetSendTransport(ITransport* st)
    {
        m_sendTransport = st;
    }

    void SetRecvTransport(ITransport* rt)
    {
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
    
    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif