#ifndef WINSOCK_CONTEXT_H
#define WINSOCK_CONTEXT_H

/// @brief RAII wrapper to initialize and cleanup Windows Sockets.
/// Instantiate this ONCE at the top of main().

#include <winsock2.h>
#include <iostream>

// Link with the Winsock library (Automatic linking for MSVC)
#pragma comment(lib, "ws2_32.lib")

class WinsockContext
{
public:
    WinsockContext()
    {
        WSADATA wsaData;
        // Request Winsock version 2.2
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            std::cerr << "CRITICAL: WSAStartup failed with error: " << result << std::endl;
            // Initialization failed. The app likely cannot proceed with networking.
        }
    }

    ~WinsockContext()
    {
        // Cleanup resources when this object goes out of scope (end of main)
        WSACleanup();
    }
};

#endif // WINSOCK_CONTEXT_H