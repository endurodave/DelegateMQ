#ifndef WIN32_UDP_TRANSPORT_H
#define WIN32_UDP_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using Win32 UDP socket. 
/// 

#if !defined(_WIN32) && !defined(_WIN64)
    #error This code must be compiled as a Win32 or Win64 application.
#endif

#pragma comment(lib, "ws2_32.lib")  // Link with Winsock library

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <windows.h>
#include <sstream>
#include <cstdio>

/// @brief Win32 data pipe transport example. 
class Win32UdpTransport : public ITransport
{
public:
    enum class Type 
    {
        PUB,
        SUB
    };

    Win32UdpTransport() : m_thread("Win32UdpTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread();
    }

    ~Win32UdpTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, LPCSTR addr, USHORT port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

        WSADATA wsaData;
        m_type = type;

        // Initialize Winsock
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
        {
            std::cerr << "WSAStartup failed." << std::endl;
            return -1;
        }

        // Create a UDP socket
        m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == INVALID_SOCKET) 
        {
            std::cerr << "Socket creation failed." << std::endl;
            WSACleanup();
            return -1;
        }

        if (type == Type::PUB)
        {
            // Set up the server address structure
            m_addr.sin_family = AF_INET;
            m_addr.sin_port = htons(port);  // Convert port to network byte order
            m_addr.sin_addr.s_addr = inet_addr(addr);  // Convert IP address to binary
        }
        else if (type == Type::SUB)
        {
            // Set up the server address structure
            m_addr.sin_family = AF_INET;
            m_addr.sin_port = htons(port);  // Convert port to network byte order
            m_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to all available interfaces

            // Bind the socket to the address and port
            int err = ::bind(m_socket, (sockaddr*)&m_addr, sizeof(m_addr));
            if (err == SOCKET_ERROR)
            {
                std::cerr << "Bind failed: " << err << std::endl;
                return -1;
            }

            // Set a 2-second receive timeout
            DWORD timeout = 2000; // 2000 milliseconds = 2 seconds
            err = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            if (err == SOCKET_ERROR)
            {
                std::cerr << "setsockopt(SO_RCVTIMEO) failed: " << WSAGetLastError() << std::endl;
                return -1;
            }
        }

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        closesocket(m_socket);
        WSACleanup();
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

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

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write each header value using the getters from DmqHeader
        auto marker = header.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = header.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = header.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        // Insert delegate arguments from the stream (os)
        ss << os.str();

        size_t length = ss.str().length();

        if (id != dmq::ACK_REMOTE_ID)
        {
            // Add sequence number to monitor
            if (m_transportMonitor)
                m_transportMonitor->Add(seqNum, id);
        }

        char* sendBuf = (char*)malloc(length);
        if (!sendBuf)
            return -1;

        // Copy char buffer into heap allocated memory
        ss.rdbuf()->sgetn(sendBuf, length);

        int err = sendto(m_socket, sendBuf, (int)length, 0, (sockaddr*)&m_addr, sizeof(m_addr));
        free(sendBuf);
        if (err == SOCKET_ERROR) 
        {
            std::cerr << "sendto failed." << std::endl;
            return -1;
        }
        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_type == Type::PUB) {
            std::cout << "Error: Cannot receive on PUB socket!" << std::endl;
            return -1;
        }

        if (m_recvTransport != this) {
            std::cout << "Error: This transport used for send only!" << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int addrLen = sizeof(m_addr);
        int size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, (sockaddr*)&m_addr, &addrLen);
        if (size == SOCKET_ERROR) 
        {
            // timeout
            //std::cerr << "recvfrom failed." << std::endl;
            return -1;
        }

        // Write the received data into the stringstream
        headerStream.write(m_buffer, size);
        headerStream.seekg(0);  

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;  // TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum (again using the getter for byte swapping)
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        // Write the remaining argument data to stream
        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (id == dmq::ACK_REMOTE_ID)
        {
            // Receiver ack'ed message. Remove sequence number from monitor.
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {
            if (m_transportMonitor && m_sendTransport)
            {
                // Send header with received seqNum as the ack message
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                m_sendTransport->Send(ss_ack, ack);
            }
        }

        // argStream contains the serialized remote argument data
        return 0;   // Success
    }

    // Set a transport monitor for checking message ack
    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(transportMonitor);
        m_transportMonitor = transportMonitor;
    }

    // Set an alternative send transport
    void SetSendTransport(ITransport* sendTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(sendTransport);
        m_sendTransport = sendTransport;
    }

    // Set an alternative receive transport
    void SetRecvTransport(ITransport* recvTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &Win32UdpTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(recvTransport);
        m_recvTransport = recvTransport;
    }


private:
    SOCKET m_socket = INVALID_SOCKET;
    sockaddr_in m_addr;

    Type m_type = Type::PUB;

    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif
