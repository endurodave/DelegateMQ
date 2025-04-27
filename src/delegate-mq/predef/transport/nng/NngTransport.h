#ifndef NNG_TRANSPORT_H
#define NNG_TRANSPORT_H

#include "DelegateMQ.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include <nng/nng.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <sstream>
#include <cstdio>

/// @brief NNG transport class. NNG socket instances must only be called by a 
/// single thread of control. Each transport instance has its own internal thread of 
/// control and all transport APIs are asynchronous. Each instance is an "active 
/// object" with a private internal thread of control and async public APIs.
class NngTransport : public ITransport
{
public:
    enum class Type
    {
        PAIR_CLIENT,
        PAIR_SERVER,
        PUB,
        SUB
    };

    NngTransport() : m_thread("NngTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread();
    }

    ~NngTransport()
    {
        m_thread.ExitThread();
    }

    int Create(Type type, const char* addr)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::Create, m_thread, dmq::WAIT_INFINITE)(type, addr);

        // Initialize NNG context
        m_type = type;

        int rc = 0;
        if (m_type == Type::PAIR_CLIENT)
        {
            rc = nng_pair0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG pair client socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_dial(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to dial address: " << nng_strerror(rc) << std::endl;
                nng_close(m_nngSocket);
                return rc;
            }
        }
        else if (m_type == Type::PAIR_SERVER)
        {
            rc = nng_pair0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG pair server socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_listen(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to listen on address: " << nng_strerror(rc) << std::endl;
                return rc;
            }
        }
        else if (m_type == Type::PUB)
        {
            rc = nng_pub0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG PUB socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_listen(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to listen on address: " << nng_strerror(rc) << std::endl;
                return rc;
            }
        }
        else if (m_type == Type::SUB)
        {
            rc = nng_sub0_open(&m_nngSocket);
            if (rc != 0) {
                std::cerr << "Failed to open NNG SUB socket: " << nng_strerror(rc) << std::endl;
                return rc;
            }
            rc = nng_dial(m_nngSocket, addr, nullptr, 0);
            if (rc != 0) {
                std::cerr << "Failed to dial address: " << nng_strerror(rc) << std::endl;
                return rc;
            }

            // Subscribe to all messages
            rc = nng_setopt(m_nngSocket, NNG_OPT_SUB_SUBSCRIBE, "", 0);
            if (rc != 0) {
                std::cerr << "Failed to set subscription filter: " << nng_strerror(rc) << std::endl;
                return rc;
            }
        }
        else
        {
            std::cerr << "Invalid socket type" << std::endl;
            return 1;
        }

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        // Close the socket
        if (nng_socket_id(m_nngSocket) != 0) {
            nng_close(m_nngSocket);
        }
        m_nngSocket = NNG_SOCKET_INITIALIZER;
    }

    void Destroy()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::Destroy, m_thread, dmq::WAIT_INFINITE)();

        // NNG doesn't require explicit context terminations
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

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

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write each header value using the getters from DmqHeader
        auto marker = header.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = header.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = header.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        // Insert delegate arguments from the stream (os)
        ss << os.str();

        size_t length = ss.str().length();

        if (id != dmq::ACK_REMOTE_ID)
        {
            // Add sequence number to monitor
            if (m_transportMonitor)
                m_transportMonitor->Add(seqNum, id);
        }

        // Send delegate argument data using NNG
        std::string payload = ss.str();
        int err = nng_send(m_nngSocket, payload.data(), payload.size(), 0);
        if (err != 0)
        {
            std::cerr << "nng_send failed with error: " << nng_strerror(err) << std::endl;
            return err;
        }

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (nng_socket_id(m_nngSocket) == 0) {
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

        size_t size = sizeof(m_buffer);
        int err = nng_recv(m_nngSocket, m_buffer, &size, 0);
        if (size == -1) {
            std::cerr << "nng_recv failed with error: " << nng_strerror(err) << std::endl;
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
            return -1;  // TODO: Optionally handle this case more gracefully
        }

        // Read the DelegateRemoteId (2 bytes) into the `id` variable
        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        // Read seqNum using the getter for byte swapping
        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

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
            return MakeDelegate(this, &NngTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(transportMonitor);
        m_transportMonitor = transportMonitor;
    }

    // Set an alternative send transport
    void SetSendTransport(ITransport* sendTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(sendTransport);
        m_sendTransport = sendTransport;
    }

    // Set an alternative receive transport
    void SetRecvTransport(ITransport* recvTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return MakeDelegate(this, &NngTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(recvTransport);
        m_recvTransport = recvTransport;
    }

private:
    nng_socket m_nngSocket = NNG_SOCKET_INITIALIZER;
    Type m_type = Type::PAIR_CLIENT;

    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = {};
};

#endif
