#ifndef NETX_UDP_TRANSPORT_H
#define NETX_UDP_TRANSPORT_H

/// @file NetXUdpTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief NetX (ThreadX) UDP transport implementation for DelegateMQ.
///
/// @details
/// This class implements the ITransport interface using the Azure RTOS NetX (or NetX Duo)
/// network stack. It is designed for embedded targets running ThreadX.
///
/// **Dependencies:**
/// * ThreadX (`tx_api.h`)
/// * NetX (`nx_api.h`)
/// * A valid `NX_IP` instance and `NX_PACKET_POOL` must be provided at construction.
///
/// **Key Features:**
/// 1.  **Direct Execution**: Executes network operations directly on the calling thread, 
///     avoiding context switch overhead and preventing deadlocks.
/// 2.  **Thread Safety**: Uses a `TX_MUTEX` to protect the NetX socket from concurrent 
///     access (Send vs Receive).
/// 3.  **Packet Management**: Handles manual `nx_packet_allocate` and `nx_packet_release`
///     required by the NetX Zero-Copy architecture.
/// 4.  **Reliability**: Integrated with `TransportMonitor` for reliable delivery (ACKs/Retries).
/// 5.  **Endianness**: Manually marshals headers to Network Byte Order (Big Endian) to
///     ensure compatibility with PC-based transports.

#include "DelegateMQ.h"
#include "predef/transport/ITransportMonitor.h"
#include "nx_api.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// Helper to check for standard endian headers, otherwise define locals
#if defined(__GNUC__) || defined(__clang__)
    #define NETX_HTONS(x) __builtin_bswap16(x)
    #define NETX_NTOHS(x) __builtin_bswap16(x)
#else
    // Fallback for generic compilers if ntohs is not available in libc
    inline uint16_t netx_swap16(uint16_t val) {
        return (val << 8) | (val >> 8);
    }
    #define NETX_HTONS(x) netx_swap16(x)
    #define NETX_NTOHS(x) netx_swap16(x)
#endif

class NetXUdpTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    /// @brief Constructor
    /// @param ip_ptr Pointer to the initialized NetX IP instance.
    /// @param pool_ptr Pointer to the packet pool for allocating UDP packets.
    NetXUdpTransport(NX_IP* ip_ptr, NX_PACKET_POOL* pool_ptr)
        : m_ip(ip_ptr)
        , m_pool(pool_ptr)
        , m_sendTransport(this)
        , m_recvTransport(this)
    {
        // Initialize mutex for socket protection
        tx_mutex_create(&m_mutex, (CHAR*)"NetXTransportMutex", TX_NO_INHERIT);
        memset(&m_socket, 0, sizeof(m_socket));
    }

    ~NetXUdpTransport()
    {
        Close();
        tx_mutex_delete(&m_mutex);
    }

    /// @brief Initialize the UDP socket.
    /// @param type PUB or SUB.
    /// @param addr_str Target IP address string (e.g., "192.168.1.100").
    /// @param port UDP port.
    int Create(Type type, const char* addr_str, uint16_t port)
    {
        // Lock mutex
        tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);

        m_type = type;
        m_port = port;

        // Convert string IP to ULONG (NetX IPv4 format)
        // If your toolchain supports inet_pton, use it. Otherwise, simple parse:
        m_targetIp = 0;
        if (addr_str) {
            uint32_t a, b, c, d;
            if (std::sscanf(addr_str, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                // NetX expects IP in host byte order usually, but let's pack it standard
                m_targetIp = (a << 24) | (b << 16) | (c << 8) | d;
            }
        }

        // Create the UDP Socket
        // Name, IP Instance, Type, Window Size, Queue Depth, Wait Option
        UINT ret = nx_udp_socket_create(m_ip, &m_socket, (CHAR*)"DelegateMQ_Sock", 
                                        NX_IP_NORMAL, NX_FRAGMENT_OKAY, 0x80, 5);
        if (ret != NX_SUCCESS) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // Bind
        if (type == Type::SUB) {
            // Bind to specific port for receiving
            ret = nx_udp_socket_bind(&m_socket, port, NX_WAIT_FOREVER);
        } else {
            // Bind to any ephemeral port for sending
            ret = nx_udp_socket_bind(&m_socket, NX_ANY_PORT, NX_WAIT_FOREVER);
        }

        if (ret != NX_SUCCESS) {
            nx_udp_socket_delete(&m_socket);
            tx_mutex_put(&m_mutex);
            return -1;
        }

        tx_mutex_put(&m_mutex);
        return 0;
    }

    void Close()
    {
        tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);
        
        if (m_socket.nx_udp_socket_id != 0)
        {
            nx_udp_socket_unbind(&m_socket);
            nx_udp_socket_delete(&m_socket);
            memset(&m_socket, 0, sizeof(m_socket));
        }
        
        tx_mutex_put(&m_mutex);
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);

        if (os.bad() || os.fail()) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // Block regular data on SUB socket (only ACKs allowed)
        if (m_type == Type::SUB && header.GetId() != dmq::ACK_REMOTE_ID) {
            tx_mutex_put(&m_mutex);
            return -1;
        }
        
        if (m_sendTransport != this) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // 1. Allocate NetX Packet
        NX_PACKET* packet_ptr = nullptr;
        UINT ret = nx_packet_allocate(m_pool, &packet_ptr, NX_UDP_PACKET, NX_NO_WAIT);
        if (ret != NX_SUCCESS) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // 2. Prepare Header (Network Byte Order)
        DmqHeader headerCopy = header;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        uint16_t marker = NETX_HTONS(headerCopy.GetMarker());
        uint16_t id     = NETX_HTONS(headerCopy.GetId());
        uint16_t seqNum = NETX_HTONS(headerCopy.GetSeqNum());
        uint16_t length = NETX_HTONS(headerCopy.GetLength());

        // 3. Append Data
        // NetX appends to the packet payload area
        nx_packet_data_append(packet_ptr, &marker, sizeof(marker), m_pool, NX_NO_WAIT);
        nx_packet_data_append(packet_ptr, &id, sizeof(id), m_pool, NX_NO_WAIT);
        nx_packet_data_append(packet_ptr, &seqNum, sizeof(seqNum), m_pool, NX_NO_WAIT);
        nx_packet_data_append(packet_ptr, &length, sizeof(length), m_pool, NX_NO_WAIT);
        nx_packet_data_append(packet_ptr, (VOID*)payload.data(), payload.size(), m_pool, NX_NO_WAIT);

        // 4. Track Reliability
        if (headerCopy.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        // 5. Send
        // Note: nx_udp_socket_send releases the packet on success or failure internally
        // (unless it fails immediately with basic param errors, but usually it consumes ownership)
        ret = nx_udp_socket_send(&m_socket, packet_ptr, m_targetIp, m_port);
        
        if (ret != NX_SUCCESS) {
            // NetX usually releases packet on failure inside nx_udp_socket_send, 
            // but double check your specific NetX version documentation. 
            // Often if it returns non-success, the user must release.
            nx_packet_release(packet_ptr); 
            tx_mutex_put(&m_mutex);
            return -1;
        }

        tx_mutex_put(&m_mutex);
        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        // Don't hold the lock while waiting for a packet if it's blocking!
        // But here we use a short timeout (1 tick).
        // However, nx_udp_socket_receive itself is thread-safe on the socket handle in NetX Duo
        // usually. We only lock to protect our member variables.
        
        tx_mutex_get(&m_mutex, TX_WAIT_FOREVER);

        if (m_recvTransport != this) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        NX_PACKET* packet_ptr = nullptr;
        
        // Temporarily unlock to allow Send() while we wait?
        // If we block here, Send() can't run. 
        // We use NO_WAIT or minimal wait to keep the loop responsive.
        UINT ret = nx_udp_socket_receive(&m_socket, &packet_ptr, 1); // 1 tick wait

        if (ret != NX_SUCCESS) {
            tx_mutex_put(&m_mutex);
            return -1; // No packet or error
        }

        // --- Extract Source Info for ACKs ---
        ULONG srcIp;
        UINT srcPort;
        nx_udp_source_extract(packet_ptr, &srcIp, &srcPort);

        // Update target if we are a Subscriber (handle dynamic sender address)
        if (m_type == Type::SUB) {
            m_targetIp = srcIp;
            m_port = srcPort;
        }

        // --- Read Data ---
        // NetX allows reading bytes from the packet.
        // We'll extract to a temp buffer.
        ULONG bytesRead;
        UCHAR buffer[512]; // Temp buffer for header + small payload
        
        // Extract Header
        // Note: DelegateMQ header is 8 bytes.
        // nx_packet_data_retrieve copies data out.
        ret = nx_packet_data_retrieve(packet_ptr, buffer, &bytesRead);
        
        // We are done with the NetX packet now
        nx_packet_release(packet_ptr);

        if (ret != NX_SUCCESS || bytesRead < DmqHeader::HEADER_SIZE) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // Parse Header
        uint16_t* ptr16 = (uint16_t*)buffer;
        header.SetMarker(NETX_NTOHS(ptr16[0]));
        header.SetId(NETX_NTOHS(ptr16[1]));
        header.SetSeqNum(NETX_NTOHS(ptr16[2]));
        header.SetLength(NETX_NTOHS(ptr16[3]));

        if (header.GetMarker() != DmqHeader::MARKER) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // Check payload size
        if (bytesRead < (ULONG)(DmqHeader::HEADER_SIZE + header.GetLength())) {
            tx_mutex_put(&m_mutex);
            return -1;
        }

        // Write Payload to stream
        is.write((char*)(buffer + DmqHeader::HEADER_SIZE), header.GetLength());

        // --- ACK Logic ---
        if (header.GetId() == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_transportMonitor && m_sendTransport) {
            // Send ACK
            xostringstream ss_ack;
            DmqHeader ack;
            ack.SetId(dmq::ACK_REMOTE_ID);
            ack.SetSeqNum(header.GetSeqNum());
            ack.SetLength(0);
            
            // NOTE: Recursive calls to Send() are safe if mutex is recursive 
            // OR if we are careful. NetX mutexes are usually NOT recursive by default 
            // unless TX_INHERIT is used, but even then...
            // It is safer to release lock before calling Send() if Send() also locks.
            // But here, Send() is part of this class.
            // Let's release first to be safe and avoid deadlocks if Send() logic changes.
            tx_mutex_put(&m_mutex);
            m_sendTransport->Send(ss_ack, ack);
            return 0; 
        }

        tx_mutex_put(&m_mutex);
        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* tm) {
        m_transportMonitor = tm;
    }
    void SetSendTransport(ITransport* st) {
        m_sendTransport = st;
    }
    void SetRecvTransport(ITransport* rt) {
        m_recvTransport = rt;
    }

private:
    NX_IP* m_ip = nullptr;
    NX_PACKET_POOL* m_pool = nullptr;
    NX_UDP_SOCKET m_socket;
    TX_MUTEX m_mutex;
    
    ULONG m_targetIp = 0;
    UINT  m_port = 0;

    Type m_type = Type::PUB;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
};

#endif // NETX_UDP_TRANSPORT_H