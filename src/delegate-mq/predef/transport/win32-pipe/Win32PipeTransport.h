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

    int Create(Type type, LPCSTR pipeName)
    {
        if (type == Type::PUB)
        {
            // Create named pipe
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
            // Create named pipe
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
            DWORD dwError = GetLastError();
            std::cout << "Create pipe failed: " << dwError << std::endl;
            return -1;
        }
        return 0;
    }

    void Close()
    {
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail())
            return -1;

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

        // Write header values using the getters from the COPY
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Insert delegate arguments (payload)
        ss.write(payload.data(), payload.size());

        // --- Efficient Write Logic (No Malloc) ---
        std::string fullPacket = ss.str();

        DWORD sentLen = 0;
        BOOL success = WriteFile(m_hPipe, fullPacket.c_str(), (DWORD)fullPacket.length(), &sentLen, NULL);

        if (!success || sentLen != fullPacket.length())
            return -1;

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        // Check if client connected
        BOOL connected = ConnectNamedPipe(m_hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected == FALSE)
            return -1;

        DWORD size = 0;
        BOOL success = ReadFile(m_hPipe, m_buffer, BUFFER_SIZE, &size, NULL);

        if (success == FALSE || size <= 0)
            return -1;

        if (size <= DmqHeader::HEADER_SIZE) {
            std::cerr << "Received data is too small to process." << std::endl;
            return -1;
        }

        // Write the received data into the stringstream
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;  // @TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum (again using the getter for byte swapping)
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        // Read length (again using the getter for byte swapping)
        uint16_t length = 0;
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetLength(length);

        // Write the remaining argument data to stream
        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        // Now `is` contains the rest of the remote argument data
        return 0;  // Success
    }

private:
    // @TODO Update buffer size if necessary.
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE];

    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
};

#endif
