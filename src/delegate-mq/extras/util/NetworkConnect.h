#ifndef NETWORK_CONNECT_H
#define NETWORK_CONNECT_H

/// @brief RAII wrapper to initialize and cleanup network stacks.
/// Instantiate this ONCE at the top of main().
///
/// On bare-metal targets (no OS, no socket stack) the class compiles to
/// empty stubs so that DataBus.h can include this header unconditionally.

#include "delegate/DelegateOpt.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <cstring>
#include <net/if.h>
#endif

#include <string>

namespace dmq::util {

class NetworkContext
{
public:
    NetworkContext()
    {
#ifdef _WIN32
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0)
        {
            // WSAStartup failed — network unavailable
        }
#endif
    }

    ~NetworkContext()
    {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    /// @brief Helper to find the first non-loopback physical IPv4 address.
    /// @return The IP address as a string (e.g. "192.168.1.5") or "127.0.0.1" if none found.
    static std::string GetLocalAddress()
    {
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
            return "127.0.0.1";
        }

        addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) {
            return "127.0.0.1";
        }

        std::string firstIp = "127.0.0.1";
        for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
            char ipStr[INET_ADDRSTRLEN];
            sockaddr_in* ipv4 = (sockaddr_in*)p->ai_addr;
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);
            std::string current(ipStr);
            if (current.find("127.") != 0) {
                firstIp = current;
                break;
            }
        }

        freeaddrinfo(res);
        return firstIp;
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == -1) {
            return "127.0.0.1";
        }

        std::string firstIp = "127.0.0.1";
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                continue;
            if (ifa->ifa_flags & IFF_LOOPBACK)
                continue;
            if (!(ifa->ifa_flags & IFF_UP))
                continue;

            char ipStr[INET_ADDRSTRLEN];
            void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addrPtr, ipStr, INET_ADDRSTRLEN);
            std::string current(ipStr);
            if (current != "0.0.0.0" && current.find("127.") != 0) {
                firstIp = current;
                break;
            }
        }

        freeifaddrs(ifaddr);
        return firstIp;
#else
        return "127.0.0.1";
#endif
    }
};

} // namespace dmq::util


#endif // NETWORK_CONNECT_H
