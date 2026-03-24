#ifndef LINUX_MULTICAST_TRANSPORT_H
#define LINUX_MULTICAST_TRANSPORT_H

#include "delegate/DelegateOpt.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/DmqHeader.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <net/if.h>
#include <fcntl.h>
#include <errno.h>

/// @brief Linux Multicast UDP transport implementation for DelegateMQ.
class MulticastTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    MulticastTransport() = default;
    ~MulticastTransport() { Close(); }

    int Create(Type type, const char* groupAddr, uint16_t port, const char* localInterface = "0.0.0.0")
    {
        m_type = type;

        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            std::cerr << "Multicast socket creation failed: " << strerror(errno) << std::endl;
            return -1;
        }

        int reuse = 1;
        if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "SO_REUSEADDR failed: " << strerror(errno) << std::endl;
            return -1;
        }

        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);

        if (type == Type::PUB) {
            if (inet_aton(groupAddr, &m_addr.sin_addr) == 0) {
                std::cerr << "Invalid multicast group address." << std::endl;
                return -1;
            }

            // Set the interface for outgoing multicast data
            struct in_addr localAddr;
            inet_aton(localInterface, &localAddr);
            if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_IF, &localAddr, sizeof(localAddr)) < 0) {
                std::cerr << "IP_MULTICAST_IF failed." << std::endl;
            }

            int ttl = 3;
            if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
                std::cerr << "IP_MULTICAST_TTL failed." << std::endl;
                return -1;
            }

            int loop = 1;
            setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
        }
        else {
            m_addr.sin_addr.s_addr = INADDR_ANY;
            if (bind(m_socket, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0) {
                std::cerr << "Multicast bind failed: " << strerror(errno) << std::endl;
                return -1;
            }

            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(groupAddr);
            mreq.imr_interface.s_addr = inet_addr(localInterface);
            if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                std::cerr << "IP_ADD_MEMBERSHIP failed: " << strerror(errno) << std::endl;
                return -1;
            }

            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        }

        return 0;
    }

    void Close() {
        if (m_socket != -1) {
            shutdown(m_socket, SHUT_RDWR);
            close(m_socket);
            m_socket = -1;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override {
        if (m_type != Type::PUB) return -1;

        std::string payload = os.str();
        DmqHeader headerCopy = header;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id     = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t length = htons(headerCopy.GetLength());

        ss.write((const char*)&marker, 2);
        ss.write((const char*)&id, 2);
        ss.write((const char*)&seqNum, 2);
        ss.write((const char*)&length, 2);
        ss.write(payload.data(), payload.size());

        std::string data = ss.str();
        ssize_t sent = sendto(m_socket, data.c_str(), data.size(), 0, (struct sockaddr*)&m_addr, sizeof(m_addr));
        return (sent == (ssize_t)data.size()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override {
        if (m_type != Type::SUB) return -1;

        int size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0, NULL, NULL);
        if (size <= (int)DmqHeader::HEADER_SIZE) return -1;

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t val = 0;
        headerStream.read((char*)&val, 2); header.SetMarker(ntohs(val));
        headerStream.read((char*)&val, 2); header.SetId(ntohs(val));
        headerStream.read((char*)&val, 2); header.SetSeqNum(ntohs(val));
        headerStream.read((char*)&val, 2); header.SetLength(ntohs(val));

        if (header.GetMarker() != DmqHeader::MARKER) return -1;

        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);
        return 0;
    }

private:
    int m_socket = -1;
    sockaddr_in m_addr{};
    Type m_type = Type::PUB;
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // LINUX_MULTICAST_TRANSPORT_H
