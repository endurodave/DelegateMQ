#ifndef ZEROMQ_TRANSPORT_H
#define ZEROMQ_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using ZeroMQ library. Update 
/// BUFFER_SIZE below as necessary.

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <zmq.h>
#include <sstream>
#include <cstdio>

/// @brief ZeroMQ transport example. Each Transport object must only be called
/// by a single thread of control per ZeroMQ library.
class ZeroMqTransport : public ITransport
{
public:
    enum class Type 
    {
        PAIR_CLIENT,
        PAIR_SERVER,
        PUB,
        SUB
    };

    int Create(Type type, const char* addr)
    {
        m_zmqContext = zmq_ctx_new();

        if (type == Type::PAIR_CLIENT)
        {
            m_zmqContext = zmq_ctx_new();

            // Create a PAIR socket and connect the client to the server address
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PAIR);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_connect failed");
                return 1;
            }
        }
        else if (type == Type::PAIR_SERVER)
        {
            m_zmqContext = zmq_ctx_new();

            // Create a PAIR socket and bind the server to an address
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PAIR);
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed");
                return 1;
            }

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else if (type == Type::PUB)
        {
            // Create a PUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PUB);
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed"); 
                return 1;
            }
        }
        else if (type == Type::SUB)
        {
            // Create a SUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_SUB);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_connect failed");
                return 1;
            }

            // Subscribe to all messages
            zmq_setsockopt(m_zmq, ZMQ_SUBSCRIBE, "", 0);

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else
        {
            // Handle other types, if needed
            perror("Invalid socket type");
            return 1;
        }
        return 0;
    }

    void Close()
    {
        // Close the subscriber socket and context
        if (m_zmq)
            zmq_close(m_zmq);
        m_zmq = nullptr;
    }

    void Destroy()
    {
        if (m_zmqContext)
            zmq_ctx_destroy(m_zmqContext);
        m_zmqContext = nullptr;
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
        ss << os.rdbuf();

        size_t length = ss.str().length();

        // Send delegate argument data using ZeroMQ
        int err = zmq_send(m_zmq, ss.str().c_str(), length, ZMQ_DONTWAIT);
        if (err == -1)
        {
            std::cerr << "zmq_send failed with error: " << zmq_strerror(zmq_errno()) << std::endl;
            return zmq_errno();
        }
        else
        {
            if (id != dmq::ACK_REMOTE_ID)
            {
                // Add sequence number to monitor
                if (m_transportMonitor)
                    m_transportMonitor->Add(seqNum, id);
            }
            return 0;
        }
    }

    virtual xstringstream Receive(DmqHeader& header) override
    {
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int size = zmq_recv(m_zmq, buffer, BUFFER_SIZE, ZMQ_DONTWAIT);
        if (size == -1) {
            // Check if the error is due to a timeout
            if (zmq_errno() == EAGAIN) {
                //std::cout << "Receive timeout!" << std::endl;
            }
            return headerStream;
        }

        if (size < DmqHeader::HEADER_SIZE) {
            std::cerr << "Received data is too small to process." << std::endl;
            return headerStream;
        }

        // Write the received header data into the stringstream
        headerStream.write(buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return headerStream;  // TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum using the getter for byte swapping
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        xstringstream argStream(std::ios::in | std::ios::out | std::ios::binary);

        // Write the remaining argument data to stream
        argStream.write(buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (id == dmq::ACK_REMOTE_ID)
        {
            // Receiver ack'ed message. Remove sequence number from monitor.
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {          
            if (m_transportMonitor)
            {
                // Send header with received seqNum as the ack message
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                Send(ss_ack, ack);
            }
        }

        // argStream contains the serialized remote argument data
        return argStream;
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        m_transportMonitor = transportMonitor;
    }

private:
    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;

    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
};

#endif
