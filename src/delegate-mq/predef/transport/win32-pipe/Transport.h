#ifndef TRANSPORT_H
#define TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using Win32 data pipe. 

#if !defined(_WIN32) && !defined(_WIN64)
    #error This code must be compiled as a Win32 or Win64 application.
#endif

#include "predef/dispatcher/MsgHeader.h"
#include <windows.h>
#include <sstream>
#include <cstdio>
#include <iostream>

/// @brief Win32 data pipe transport example. 
class Transport
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

    int Send(std::stringstream& os) 
    {
        size_t length = os.str().length();
        if (os.bad() || os.fail() || length <= 0)
            return -1;

        size_t len = (size_t)os.tellp();
        char* sendBuf = (char*)malloc(len);

        // Copy char buffer into heap allocated memory
        os.rdbuf()->sgetn(sendBuf, len);

        // Send message through named pipe
        DWORD sentLen = 0;
        BOOL success = WriteFile(m_hPipe, sendBuf, (DWORD)len, &sentLen, NULL);
        free(sendBuf);

        if (!success || sentLen != len)
            return -1;
        return 0;
    }

    std::stringstream Receive(MsgHeader& header)
    {
        std::stringstream is(std::ios::in | std::ios::out | std::ios::binary);

        // Check if client connected
        BOOL connected = ConnectNamedPipe(m_hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected == FALSE)
            return is;

        DWORD size = 0;
        BOOL success = ReadFile(m_hPipe, m_buffer, BUFFER_SIZE, &size, NULL);

        if (success == FALSE || size <= 0)
            return is;

        // Write the received data into the stringstream
        is.write(m_buffer, size);
        is.seekg(0);  

        uint16_t marker = 0;
        is.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != MsgHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return is;  // TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        is.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum (again using the getter for byte swapping)
        uint16_t seqNum = 0;
        is.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        // Now `is` contains the rest of the remote argument data
        return is;
    }

private:
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE];

    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
};

#endif
