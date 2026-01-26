#ifndef WIN32_UDP_TRANSPORT_H
#define WIN32_UDP_TRANSPORT_H

/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief Win32 UDP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using Windows Sockets (Winsock2) 
/// for connectionless UDP communication. It supports two modes: PUB (Publisher/Sender) 
/// and SUB (Subscriber/Receiver).
/// 
/// Key Features:
/// 1. **Active Object**: Uses an internal worker thread to manage socket operations 
///    and ensure thread-safe execution via delegate marshaling.
/// 2. **Message Oriented**: Transmits discrete packets containing serialized delegate 
///    arguments and framing headers.
/// 3. **Reliability Support**: Integrates with `TransportMonitor` to track outgoing 
///    sequence numbers and process incoming ACKs to detect packet loss.
/// 4. **Socket Management**: Automatically handles `WSAStartup` and socket creation/cleanup.
/// 
/// @note This implementation uses blocking sockets with timeouts (`SO_RCVTIMEO`) to 
/// prevent indefinite blocking during receive operations.

#if !defined(_WIN32) && !defined(_WIN64)
#error This code must be compiled as a Win32 or Win64 application.
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")  // Link with Winsock library

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <windows.h>
#include <sstream>
#include <cstdio>

/// @brief Win32 UDP transport example. 
class UdpTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    UdpTransport() : m_thread("UdpTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        // Create thread with a 5s watchdog timeout
        m_thread.CreateThread(std::chrono::milliseconds(5000));
    }

    ~UdpTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, LPCSTR addr, USHORT port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

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

            // Convert IP address string to binary form
            if (inet_pton(AF_INET, addr, &m_addr.sin_addr) != 1)
            {
                std::cerr << "Invalid IP address format: " << addr << std::endl;
                return -1;
            }

            DWORD timeout = 2;
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
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
            return MakeDelegate(this, &UdpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        closesocket(m_socket);
        WSACleanup();
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (os.bad() || os.fail()) {
            std::cout << "Error: xostringstream is in a bad state!" << std::endl;
            return -1;
        }

        if (m_type == Type::SUB && header.GetId() != dmq::ACK_REMOTE_ID) {
            std::cout << "Error: Cannot send (non-ACK) on SUB socket!" << std::endl;
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
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        ss.write(payload.data(), payload.size());

        size_t length = ss.str().length();

        // Always track the message (unless it is an ACK)
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor) {
            m_transportMonitor->Add(seqNum, id);
        }

        char* sendBuf = (char*)malloc(length);
        if (!sendBuf)
            return -1;

        ss.rdbuf()->sgetn(sendBuf, length);

        int err = sendto(m_socket, sendBuf, (int)length, 0, (sockaddr*)&m_addr, sizeof(m_addr));
        free(sendBuf);

        return (err == SOCKET_ERROR) ? -1 : 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_recvTransport != this) {
            std::cout << "Error: This transport used for send only!" << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int addrLen = sizeof(m_addr);
        int size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, (sockaddr*)&m_addr, &addrLen);
        if (size == SOCKET_ERROR)
        {
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
            return -1;
        }

        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        uint16_t length = 0;
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetLength(length);

        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (id == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {
            if (m_transportMonitor && m_sendTransport)
            {
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                //m_sendTransport->Send(ss_ack, ack);
            }
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
