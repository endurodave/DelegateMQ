#ifndef WIN32_PIPE_TRANSPORT_H
#define WIN32_PIPE_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using Win32 data pipe. 

#if !defined(_WIN32) && !defined(_WIN64)
    #error This code must be compiled as a Win32 or Win64 application.
#endif

// Include Winsock for htons/ntohs consistency
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include "predef/transport/ITransport.h"
#include "predef/transport/DmqHeader.h"
#include <windows.h>
#include <sstream>
#include <cstdio>
#include <iostream>

/// @brief Win32 data pipe transport example. 
class Win32PipeTransport : public ITransport
{
public:
    enum class Type 
    {
        PUB,
        SUB
    };

    Win32PipeTransport() = default;
    
    ~Win32PipeTransport()
    {
        Close();
    }

    int Create(Type type, LPCSTR pipeName)
    {
        if (type == Type::PUB)
        {
            // Connect to an existing pipe (Client)
            m_hPipe = CreateFile(
                pipeName,       // pipe name 
                GENERIC_READ |  // read and write access 
                GENERIC_WRITE,
                0,              // no sharing 
                NULL,           // default security attributes
                OPEN_EXISTING,  // opens existing pipe 
                0,              // default attributes 
                NULL);          // no template file 
        }
        else if (type == Type::SUB)
        {
            // Create named pipe (Server)
            m_hPipe = CreateNamedPipe(
                pipeName,             // pipe name 
                PIPE_ACCESS_DUPLEX,       // read/write access 
                PIPE_TYPE_MESSAGE |       // message type pipe 
                PIPE_READMODE_MESSAGE |   // message-read mode 
                PIPE_NOWAIT,              // non-blocking mode 
                PIPE_UNLIMITED_INSTANCES, // max. instances  
                BUFFER_SIZE,              // output buffer size 
                BUFFER_SIZE,              // input buffer size 
                0,                        // client time-out 
                NULL);                    // default security attribute 
        }

        if (m_hPipe == INVALID_HANDLE_VALUE)
        {
            // DWORD dwError = GetLastError();
            // std::cout << "Create pipe failed: " << dwError << std::endl;
            return -1;
        }
        return 0;
    }

    void Close() 
    {
        if (m_hPipe != INVALID_HANDLE_VALUE)
        {
            DisconnectNamedPipe(m_hPipe); // Good practice for server pipes
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail())
            return -1;

        if (m_hPipe == INVALID_HANDLE_VALUE) return -1;

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

        // Use Network Byte Order (htons) for consistency
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id     = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t len    = htons(headerCopy.GetLength());

        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Insert delegate arguments (payload)
        ss.write(payload.data(), payload.size());

        std::string fullPacket = ss.str();

        DWORD sentLen = 0;
        BOOL success = WriteFile(m_hPipe, fullPacket.c_str(), (DWORD)fullPacket.length(), &sentLen, NULL);

        if (!success || sentLen != fullPacket.length())
            return -1;

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (m_hPipe == INVALID_HANDLE_VALUE) return -1;

        // Check/Accept connection
        // Note: In non-blocking mode (PIPE_NOWAIT), this returns false immediately 
        // if connected, or error if already connected.
        BOOL connected = ConnectNamedPipe(m_hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        
        // If not connected yet, we can't read.
        if (connected == FALSE && GetLastError() == ERROR_NO_DATA) {
             return -1; 
        }

        DWORD size = 0;
        BOOL success = ReadFile(m_hPipe, m_buffer, BUFFER_SIZE, &size, NULL);

        if (success == FALSE || size <= 0)
            return -1;

        if (size <= DmqHeader::HEADER_SIZE) {
            // std::cerr << "Received data is too small to process." << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t val = 0;

        // Use Network Byte Order (ntohs)
        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetMarker(ntohs(val));

        if (header.GetMarker() != DmqHeader::MARKER) {
            return -1;
        }

        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetId(ntohs(val));

        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetSeqNum(ntohs(val));

        headerStream.read(reinterpret_cast<char*>(&val), sizeof(val));
        header.SetLength(ntohs(val));

        // Write the remaining argument data to stream
        // Note: This relies on PIPE_TYPE_MESSAGE mode preserving boundaries.
        // If 'size' is > HEADER_SIZE + payload length, we strictly write the payload part.
        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        return 0;
    }

private:
    // Increase buffer to Max Packet Size (64KB) to avoid truncation
    static const int BUFFER_SIZE = 65536; 
    char m_buffer[BUFFER_SIZE];

    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
};

#endif