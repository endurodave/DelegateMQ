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

    int Create(Type type, LPCSTR pipeName)
    {
        WSADATA wsaData;

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
            m_serverAddr.sin_family = AF_INET;
            m_serverAddr.sin_port = htons(SERVER_PORT);  // Convert port to network byte order
            m_serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);  // Convert IP address to binary
        }
        else if (type == Type::SUB)
        {
            // Set up the server address structure
            m_serverAddr.sin_family = AF_INET;
            m_serverAddr.sin_port = htons(SERVER_PORT);  // Convert port to network byte order
            m_serverAddr.sin_addr.s_addr = INADDR_ANY;  // Bind to all available interfaces

            // Bind the socket to the address and port
            int err = ::bind(m_socket, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));
            if (err == SOCKET_ERROR)
            {
                std::cerr << "Bind failed: " << err << std::endl;
                return -1;
            }
        }

        return 0;
    }

    void Close()
    {
        closesocket(m_socket);
        WSACleanup();
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail())
            return -1;

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

        char* sendBuf = (char*)malloc(length);
        if (!sendBuf)
            return -1;

        // Copy char buffer into heap allocated memory
        ss.rdbuf()->sgetn(sendBuf, length);

        int err = sendto(m_socket, sendBuf, (int)length, 0, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));
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
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int clientAddrLen = sizeof(m_clientAddr);
        int size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, (sockaddr*)&m_clientAddr, &clientAddrLen);
        if (size == SOCKET_ERROR) 
        {
            std::cerr << "recvfrom failed." << std::endl;
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

        // argStream contains the serialized remote argument data
        return 0;   // Success
    }

private:
    SOCKET m_socket = INVALID_SOCKET;
    sockaddr_in m_serverAddr;
    sockaddr_in m_clientAddr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE];

    const char* SERVER_IP = "127.0.0.1";  // Localhost
    unsigned short SERVER_PORT = 8080;    // Server port
};

#endif
