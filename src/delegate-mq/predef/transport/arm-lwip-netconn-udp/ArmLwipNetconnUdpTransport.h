#ifndef ARM_LWIP_NETCONN_UDP_TRANSPORT_H
#define ARM_LWIP_NETCONN_UDP_TRANSPORT_H

/// @file ArmLwipNetconnUdpTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
/// 
/// @brief ARM lwIP Netconn UDP transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the lwIP "Netconn" API.
/// This API is specific to lwIP+FreeRTOS and is more efficient than the BSD 
/// Socket API as it avoids the socket wrapper overhead.
/// 
/// Prerequisites:
/// - lwIP must be compiled with `LWIP_NETCONN=1`
/// - FreeRTOS must be running (Netconn relies on OS primitives)

#include "delegate/DelegateOpt.h"
#include "predef/transport/ITransportMonitor.h"

// lwIP Netconn Includes
#include "lwip/api.h"
#include "lwip/inet.h" 
#include "lwip/ip_addr.h"

#include <iostream>
#include <sstream>
#include <cstring>

class NetconnUdpTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    NetconnUdpTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~NetconnUdpTransport()
    {
        Close();
    }

    /// Initialize the Netconn UDP connection
    /// @param type PUB or SUB
    /// @param addr Target IP string (e.g. "192.168.1.50") for PUB; Ignored for SUB.
    /// @param port Local port to bind (SUB) or Remote port to target (PUB).
    /// @return 0 on success, -1 on failure
    int Create(Type type, const char* addr, uint16_t port)
    {
        m_type = type;
        m_remotePort = port;

        // 1. Create a new UDP connection
        m_conn = netconn_new(NETCONN_UDP);
        if (m_conn == nullptr)
        {
            return -1;
        }

        // 2. Configure Address
        if (type == Type::PUB)
        {
            // Parse string IP to LwIP ip_addr_t
            if (ipaddr_aton(addr, &m_remoteIp) == 0)
            {
                Close(); // Prevent memory leak on invalid IP
                return -1; 
            }
            netconn_set_recvtimeout(m_conn, 50);
        }
        else if (type == Type::SUB)
        {
            // Bind to all interfaces (IP_ADDR_ANY) on the specified port
            if (netconn_bind(m_conn, IP_ADDR_ANY, port) != ERR_OK)
            {
                Close(); // Prevent memory leak on bind failure
                return -1;
            }
            // Set a 2-second timeout to allow the thread to check for exit signals
            netconn_set_recvtimeout(m_conn, 2000);
        }

        return 0;
    }

    /// Clean up the netconn resources
    void Close()
    {
        if (m_conn)
        {
            netconn_delete(m_conn);
            m_conn = nullptr;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail() || !m_conn) {
            return -1;
        }

        // Only SUBs should block non-ACK traffic
        if (m_type == Type::SUB && header.GetId() != dmq::ACK_REMOTE_ID) {
            return -1;
        }

        if (m_sendTransport != this) {
            return -1;
        }

        DmqHeader headerCopy = header;
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        // Serialize Header (Network Byte Order)
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id     = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t length = htons(headerCopy.GetLength());

        // Allocate a Netbuf
        struct netbuf* buf = netbuf_new();
        if (!buf) return -1;

        void* dataPtr = netbuf_alloc(buf, DmqHeader::HEADER_SIZE + payload.size());
        if (!dataPtr) {
            netbuf_delete(buf);
            return -1;
        }

        // Copy Header & Payload into the linear buffer
        char* ptr = static_cast<char*>(dataPtr);
        memcpy(ptr, &marker, sizeof(marker)); ptr += sizeof(marker);
        memcpy(ptr, &id,     sizeof(id));     ptr += sizeof(id);
        memcpy(ptr, &seqNum, sizeof(seqNum)); ptr += sizeof(seqNum);
        memcpy(ptr, &length, sizeof(length)); ptr += sizeof(length);
        memcpy(ptr, payload.data(), payload.size());

        // PUB uses pre-configured IP; SUB uses last-received IP (Reply)
        err_t err = netconn_sendto(m_conn, buf, &m_remoteIp, m_remotePort);

        netbuf_delete(buf); 

        if (err != ERR_OK) return -1;

        if (header.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (m_recvTransport != this || !m_conn) {
            return -1;
        }

        struct netbuf* buf = nullptr;
        err_t err = netconn_recv(m_conn, &buf);

        if (err == ERR_TIMEOUT) return -1; 
        if (err != ERR_OK || buf == nullptr) return -1;

        // Capture Sender Info (for ACKs/Replies)
        ip_addr_t* addr = netbuf_fromaddr(buf);
        uint16_t port = netbuf_fromport(buf);
        if (addr) ip_addr_copy(m_remoteIp, *addr);
        m_remotePort = port;

        uint16_t len = netbuf_len(buf);
        if (len < DmqHeader::HEADER_SIZE) {
            netbuf_delete(buf);
            return -1;
        }

        // Ensure the stream is clean before writing new data
        is.clear(); 
        is.str(""); 

        char headerBuf[DmqHeader::HEADER_SIZE];
        netbuf_copy(buf, headerBuf, DmqHeader::HEADER_SIZE);

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(headerBuf, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t netVal;
        headerStream.read((char*)&netVal, 2); header.SetMarker(ntohs(netVal));
        headerStream.read((char*)&netVal, 2); header.SetId(ntohs(netVal));
        headerStream.read((char*)&netVal, 2); header.SetSeqNum(ntohs(netVal));
        headerStream.read((char*)&netVal, 2); header.SetLength(ntohs(netVal));

        if (header.GetMarker() != DmqHeader::MARKER || len < DmqHeader::HEADER_SIZE + header.GetLength()) {
            netbuf_delete(buf);
            return -1;
        }

        // Extract Payload
        std::vector<char> payloadBuf(header.GetLength());
        netbuf_copy_partial(buf, payloadBuf.data(), header.GetLength(), DmqHeader::HEADER_SIZE);
        
        if (is.good()) {
            is.write(payloadBuf.data(), header.GetLength());
        }

        netbuf_delete(buf);

        // Handle Reliability Logic
        if (header.GetId() == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_transportMonitor && m_sendTransport) {
            // Auto-ACK
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(header.GetSeqNum());
            ack.SetLength(0);
            m_sendTransport->Send(ss_ack, ack); 
        }

        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* monitor) { m_transportMonitor = monitor; }
    void SetSendTransport(ITransport* transport) { m_sendTransport = transport; }
    void SetRecvTransport(ITransport* transport) { m_recvTransport = transport; }

private:
    struct netconn* m_conn = nullptr;
    ip_addr_t m_remoteIp;
    uint16_t  m_remotePort = 0;
    Type m_type = Type::PUB;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
};

#endif // ARM_LWIP_NETCONN_UDP_TRANSPORT_H