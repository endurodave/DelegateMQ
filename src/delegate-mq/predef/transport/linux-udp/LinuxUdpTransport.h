#ifndef LINUX_UDP_TRANSPORT_H
#define LINUX_UDP_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using Linux UDP socket.
///

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

// Linux UDP transport example
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
        m_thread.CreateThread();
    }

    ~UdpTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, const char* addr, uint16_t port)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr, port);

        m_type = type;

        // Create UDP socket
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0)
        {
            std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
            return -1;
        }

        memset(&m_addr, 0, sizeof(m_addr));
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);

        if (type == Type::PUB)
        {
            if (inet_aton(addr, &m_addr.sin_addr) == 0)
            {
                std::cerr << "Invalid IP address format." << std::endl;
                return -1;
            }
        }
        else if (type == Type::SUB)
        {
            m_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(m_socket, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0)
            {
                std::cerr << "Bind failed: " << strerror(errno) << std::endl;
                return -1;
            }

            struct timeval timeout;
            timeout.tv_sec = 2;
            timeout.tv_usec = 0;

            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                std::cerr << "setsockopt(SO_RCVTIMEO) failed: " << strerror(errno) << std::endl;
                return -1;
            }
        }

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        if (m_socket >= 0)
        {
            close(m_socket);
            m_socket = -1;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (os.bad() || os.fail()) {
            std::cerr << "Stream state error." << std::endl;
            return -1;
        }

        if (m_type == Type::SUB || m_sendTransport != this) {
            std::cerr << "Send operation not allowed." << std::endl;
            return -1;
        }

        // Create a local copy so we can modify the length
        DmqHeader headerCopy = header;

        // Calculate payload size and set it
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Error: Payload too large." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Use the getters from the COPY, which has the correct length
        uint16_t marker = headerCopy.GetMarker();
        uint16_t id = headerCopy.GetId();
        uint16_t seqNum = headerCopy.GetSeqNum();
        uint16_t length = headerCopy.GetLength();

        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        ss.write(reinterpret_cast<const char*>(&length), sizeof(length));

        // Append Payload
        ss.write(payload.data(), payload.size());

        std::string data = ss.str();

        // Monitor Logic (Use values from copy)
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(seqNum, id);

        ssize_t sent = sendto(m_socket, data.c_str(), data.size(), 0,
            (struct sockaddr*)&m_addr, sizeof(m_addr));

        if (sent < 0)
        {
            std::cerr << "sendto failed: " << strerror(errno) << std::endl;
            return -1;
        }

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &UdpTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_type == Type::PUB || m_recvTransport != this) {
            std::cerr << "Receive operation not allowed." << std::endl;
            return -1;
        }

        sockaddr_in fromAddr;
        socklen_t addrLen = sizeof(fromAddr);
        ssize_t size = recvfrom(m_socket, m_buffer, sizeof(m_buffer), 0,
            (struct sockaddr*)&fromAddr, &addrLen);

        if (size < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                return -1; // Timeout
            std::cerr << "recvfrom failed: " << strerror(errno) << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(m_buffer, size);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (marker != DmqHeader::MARKER)
        {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;
        }

        uint16_t id = 0, seqNum = 0, length = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetId(id);
        header.SetSeqNum(seqNum);
        header.SetLength(length);

        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (id == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else if (m_transportMonitor && m_sendTransport)
        {
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(seqNum);
            m_sendTransport->Send(ss_ack, ack);
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
    int m_socket = -1;
    sockaddr_in m_addr{};
    Type m_type = Type::PUB;
    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // LINUX_UDP_TRANSPORT_H
