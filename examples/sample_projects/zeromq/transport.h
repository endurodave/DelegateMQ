#ifndef TRANSPORT_H
#define TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include <thread>
#include <chrono>
#include <zmq.h>
#include <sstream>
#include <cstdio>

/// @brief ZeroMQ transport example. Each Transport object must only be called
/// by a single thread of control per ZeroMQ library.
class Transport
{
public:
    enum class Type 
    {
        SUB,  // Subscriber
        PUB   // Publisher
    };

    int Create(Type type, const char* addr)
    {
        m_zmqContext = zmq_ctx_new();

        if (type == Type::PUB)
        {
            // Create a PUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PUB);

            // Bind the socket to a TCP port
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed");  // TODO
                return 1;
            }
        }
        else
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
        return 0;
    }

    void Close()
    {
        // Close the subscriber socket and context
        zmq_close(m_zmq);
        m_zmq = nullptr;
        zmq_ctx_destroy(m_zmqContext);
        m_zmqContext = nullptr;
    }

    void Send(std::stringstream& os) 
    {
        size_t length = os.str().length();
        if (os.bad() || os.fail() || length <= 0)
            return;

        // Send delegate argument data using ZeroMQ
        zmq_send(m_zmq, os.str().c_str(), length, 0);
    }

    std::stringstream Receive() 
    {
        std::stringstream is(std::ios::in | std::ios::out | std::ios::binary);

        char buffer[256];
        int size = zmq_recv(m_zmq, buffer, 255, 0);
        if (size == -1) {
            // Check if the error is due to a timeout
            if (zmq_errno() == EAGAIN) {
                std::cout << "Receive timeout!" << std::endl;
            }
            return is;
        }

        // Write the received data into the stringstream
        is.write(buffer, size);
        is.seekg(0);  
        return is;
    }

private:
    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;
};

#endif
