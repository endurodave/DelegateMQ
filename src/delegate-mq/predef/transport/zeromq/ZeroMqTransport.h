#ifndef ZEROMQ_TRANSPORT_H
#define ZEROMQ_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using ZeroMQ library. Update 
/// BUFFER_SIZE below as necessary.

#include "DelegateMQ.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <zmq.h>
#include <sstream>
#include <cstdio>

/// @brief ZeroMQ transport class. ZeroMQ socket instances must only be called by a 
/// single thread of control. Each transport instance has its own internal thread of 
/// control and all transport APIs are asynchronous. Each instances is an "active 
/// object" with a private internal thread of control and async public APIs.
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

    ZeroMqTransport() : m_thread("ZeroMQTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        // Create thread with a 5s watchdog timeout
        m_thread.CreateThread(std::chrono::milliseconds(5000));
    }

    ~ZeroMqTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, const char* addr)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr);

        m_zmqContext = zmq_ctx_new();
        m_type = type;

        if (m_type == Type::PAIR_CLIENT)
        {
            // Create a PAIR socket and connect the client to the server address
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PAIR);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_connect failed");
                zmq_close(m_zmq);
                return 1;
            }

            // Set the receive timeout to 100 milliseconds
            int timeout = 100; // 100mS 
            zmq_setsockopt(m_zmq, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        }
        else if (m_type == Type::PAIR_SERVER)
        {
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
        else if (m_type == Type::PUB)
        {
            // Create a PUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PUB);
            int rc = zmq_bind(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_bind failed"); 
                return 1;
            }
        }
        else if (m_type == Type::SUB)
        {
            // Create a SUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_SUB);
            int rc = zmq_connect(m_zmq, addr);
            if (rc != 0) {
                perror("zmq_connect failed");
                zmq_close(m_zmq);
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
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        // Close the subscriber socket and context
        if (m_zmq)
            zmq_close(m_zmq);
        m_zmq = nullptr;
    }

    void Destroy()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::Destroy, m_thread, dmq::WAIT_INFINITE)();

        if (m_zmqContext)
            zmq_ctx_term(m_zmqContext);
        m_zmqContext = nullptr;
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (os.bad() || os.fail()) {
            std::cout << "Error: xostringstream is in a bad state!" << std::endl;
            return -1;
        }

        if (m_type == Type::SUB) {
            std::cout << "Error: Cannot send on SUB socket!" << std::endl;
            return -1;
        }

        if (m_sendTransport != this) {
            std::cout << "Error: This transport used for receive only!" << std::endl;
            return -1;
        }

        // Create a local copy to modify the length
        DmqHeader headerCopy = header;

        // Calculate payload size and set it on the copy
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Error: Payload too large for 16-bit length." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write header values from the COPY
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Insert delegate arguments (payload)
        ss << payload;

        size_t length = ss.str().length();

        if (id != dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Add(seqNum, id);
        }

        // Send delegate argument data using ZeroMQ
        int err = zmq_send(m_zmq, ss.str().c_str(), length, ZMQ_DONTWAIT);
        if (err == -1)
        {
            std::cerr << "zmq_send failed with error: " << zmq_strerror(zmq_errno()) << std::endl;
            return zmq_errno();
        }
        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (m_zmq == nullptr) {
            std::cout << "Error: Socket not created!" << std::endl;
            return -1;
        }

        if (m_type == Type::PUB) {
            std::cout << "Error: Cannot receive on PUB socket!" << std::endl;
            return -1;
        }

        if (m_recvTransport != this) {
            std::cout << "Error: This transport used for send only!" << std::endl;
            return -1;
        }

        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        int size = zmq_recv(m_zmq, m_buffer, BUFFER_SIZE, 0);
        if (size == -1) {
            // Check if the error is due to a timeout
            if (zmq_errno() == EAGAIN) {
                //std::cout << "Receive timeout!" << std::endl;
            }
            return -1;
        }

        if (size < DmqHeader::HEADER_SIZE) {
            std::cerr << "Received data is too small to process." << std::endl;
            return -1;
        }

        // Write the received header data into the stringstream
        headerStream.write(m_buffer, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return -1;  // @TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum using the getter for byte swapping
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        // Read length using the getter for byte swapping
        uint16_t length = 0;
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetLength(length);

        // Write the remaining target function argument data to stream
        is.write(m_buffer + DmqHeader::HEADER_SIZE, size - DmqHeader::HEADER_SIZE);

        if (id == dmq::ACK_REMOTE_ID)
        {
            // Receiver ack'ed message. Remove sequence number from monitor.
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {          
            if (m_transportMonitor && m_sendTransport)
            {
                // Send header with received seqNum as the ack message
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                m_sendTransport->Send(ss_ack, ack);
            }
        }

        return 0;  // Success
    }

    // Set a transport monitor for checking message ack
    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(transportMonitor);
        m_transportMonitor = transportMonitor;
    }

    // Set an alternative send transport
    void SetSendTransport(ITransport* sendTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(sendTransport);
        m_sendTransport = sendTransport;
    }

    // Set an alternative receive transport
    void SetRecvTransport(ITransport* recvTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &ZeroMqTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(recvTransport);
        m_recvTransport = recvTransport;
    }

private:
    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;
    Type m_type = Type::PAIR_CLIENT;

    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = {};
};

#endif
