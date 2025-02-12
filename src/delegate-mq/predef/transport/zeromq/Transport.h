#ifndef TRANSPORT_H
#define TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using ZeroMQ library. Update 
/// BUFFER_SIZE below as necessary.

#include "predef/transport/ITransport.h"
#include "predef/transport/DmqHeader.h"
#include <zmq.h>
#include <sstream>
#include <cstdio>

/// @brief ZeroMQ transport example. Each Transport object must only be called
/// by a single thread of control per ZeroMQ library.
class Transport : public ITransport
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

            // Set the receive timeout to 1000 milliseconds (1 second)
            int timeout = 1000; // 1 second
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

    virtual int Send(std::stringstream& os) override
    {
        size_t length = os.str().length();
        if (os.bad() || os.fail() || length <= 0)
            return -1;

        // Send delegate argument data using ZeroMQ
        int err = zmq_send(m_zmq, os.str().c_str(), length, ZMQ_DONTWAIT);
        if (err == -1)
        {
            std::cerr << "zmq_send failed with error: " << zmq_strerror(zmq_errno()) << std::endl;
            return zmq_errno();
        }
        return 0;
    }

    virtual std::stringstream Receive(DmqHeader& header) override
    {
        std::stringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int size = zmq_recv(m_zmq, buffer, BUFFER_SIZE, ZMQ_DONTWAIT);
        if (size == -1) {
            // Check if the error is due to a timeout
            if (zmq_errno() == EAGAIN) {
                //std::cout << "Receive timeout!" << std::endl;
            }
            return headerStream;
        }

        if (size <= DmqHeader::HEADER_SIZE) {
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

        std::stringstream argStream(std::ios::in | std::ios::out | std::ios::binary);

        // Write the remaining argument data to stream
        argStream.write(buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        // argStream contains the serialized remote argument data
        return argStream;
    }

private:
    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;

    static const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
};

#endif
