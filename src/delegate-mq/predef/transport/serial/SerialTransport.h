#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// @see https://sigrok.org/wiki/Libserialport or https://github.com/sigrokproject/libserialport
/// 
/// Transport callable argument data to/from a remote using libserialport. 
/// Uses DmqHeader::m_length for stream framing.

#include "libserialport.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include "DelegateMQ.h" 

#include <sstream>
#include <iostream>
#include <vector>
#include <cstdint>

/// @brief Libserialport transport example for DelegateMQ. 
class SerialTransport : public ITransport
{
public:
    SerialTransport() : m_thread("SerialTransport"), m_sendTransport(this), m_recvTransport(this)
    {
        m_thread.CreateThread(std::chrono::milliseconds(5000));
    }

    ~SerialTransport()
    {
        Close();
        m_thread.ExitThread();
    }

    int Create(const char* portName, int baudRate)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Create, m_thread, dmq::WAIT_INFINITE)(portName, baudRate);

        sp_return ret = sp_get_port_by_name(portName, &m_port);
        if (ret != SP_OK)
        {
            std::cerr << "SerialTransport: Could not find port " << portName << std::endl;
            return -1;
        }

        ret = sp_open(m_port, SP_MODE_READ_WRITE);
        if (ret != SP_OK)
        {
            std::cerr << "SerialTransport: Could not open port " << portName << std::endl;
            sp_free_port(m_port);
            m_port = nullptr;
            return -1;
        }

        sp_set_baudrate(m_port, baudRate);
        sp_set_bits(m_port, 8);
        sp_set_parity(m_port, SP_PARITY_NONE);
        sp_set_stopbits(m_port, 1);
        sp_set_flowcontrol(m_port, SP_FLOWCONTROL_NONE);

        return 0;
    }

    void Close()
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Close, m_thread, dmq::WAIT_INFINITE)();

        if (m_port)
        {
            sp_close(m_port);
            sp_free_port(m_port);
            m_port = nullptr;
        }
    }

    // We create a local copy to modify the length field.
    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (!m_port) return -1;

        if (os.bad() || os.fail()) return -1;

        if (m_sendTransport != this) {
            std::cerr << "Error: This transport used for receive only!" << std::endl;
            return -1;
        }

        // 1. Create a local copy to modify the length
        DmqHeader headerCopy = header;

        // 2. Get Payload and Set Length on the COPY
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Error: Payload exceeds 64KB limit for 16-bit length field." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        // 3. Serialize Header (using the modified copy)
        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));

        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));

        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));

        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // 4. Append Payload
        ss << payload;

        std::string packetData = ss.str();

        // 5. Monitor Logic (Using ID/Seq from the copy)
        if (id != dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Add(seqNum, id);
        }

        // 6. Blocking Write
        // Write the entire [Header + Payload] buffer in one go.
        int result = sp_blocking_write(m_port, packetData.c_str(), packetData.length(), 1000);

        if (result < 0 || (size_t)result != packetData.length())
        {
            std::cerr << "SerialTransport: Write failed or incomplete." << std::endl;
            return -1;
        }

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Receive, m_thread, dmq::WAIT_INFINITE)(is, header);

        if (!m_port) return -1;

        if (m_recvTransport != this) {
            std::cerr << "Error: This transport used for send only!" << std::endl;
            return -1;
        }

        // 1. Read Fixed-Size Header (8 bytes)
        // We read exactly DmqHeader::HEADER_SIZE to get framing info.
        char headerBuf[DmqHeader::HEADER_SIZE];
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE, 2000)) // 2s timeout waiting for start of msg
        {
            return -1;
        }

        // 2. Deserialize Header
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(headerBuf, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "SerialTransport: Invalid sync marker!" << std::endl;
            // Ideally flush buffer here to re-sync
            sp_flush(m_port, SP_BUF_INPUT);
            return -1;
        }

        uint16_t id = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        header.SetId(id);

        uint16_t seqNum = 0;
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        header.SetSeqNum(seqNum);

        uint16_t length = 0;
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetLength(length);

        // 3. Read Payload (Based on Header Length)
        if (length > 0)
        {
            if (length > BUFFER_SIZE) {
                std::cerr << "SerialTransport: Payload too large for buffer." << std::endl;
                return -1;
            }

            if (!ReadExact(m_buffer, length, 1000)) // 1s timeout for body
            {
                std::cerr << "SerialTransport: Failed to read full payload." << std::endl;
                return -1;
            }

            // Write payload to output stream
            is.write(m_buffer, length);
        }

        // 4. Ack Logic
        if (id == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {
            if (m_transportMonitor && m_sendTransport)
            {
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                // Send() will automatically set length to 0 for empty payload
                m_sendTransport->Send(ss_ack, ack);
            }
        }

        return 0;
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::SetTransportMonitor, m_thread, dmq::WAIT_INFINITE)(transportMonitor);
        m_transportMonitor = transportMonitor;
    }

    void SetSendTransport(ITransport* sendTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::SetSendTransport, m_thread, dmq::WAIT_INFINITE)(sendTransport);
        m_sendTransport = sendTransport;
    }

    void SetRecvTransport(ITransport* recvTransport)
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::SetRecvTransport, m_thread, dmq::WAIT_INFINITE)(recvTransport);
        m_recvTransport = recvTransport;
    }

private:
    /// @brief Helper to strictly read N bytes. Handles partial reads common in Serial.
    bool ReadExact(char* dest, size_t size, unsigned int timeoutMs)
    {
        size_t totalRead = 0;
        // Simple retry loop. In production, you might want a more sophisticated timeout 
        // that decreases with each iteration.
        while (totalRead < size)
        {
            int ret = sp_blocking_read(m_port, dest + totalRead, size - totalRead, timeoutMs);
            if (ret < 0) return false; // Error
            if (ret == 0) return false; // Timeout (incomplete data)
            totalRead += ret;
        }
        return true;
    }

    sp_port* m_port = nullptr;

    Thread m_thread;

    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    // @TODO Update buffer size if necessary. Max 64KB for 16-bit length.
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // SERIAL_TRANSPORT_H