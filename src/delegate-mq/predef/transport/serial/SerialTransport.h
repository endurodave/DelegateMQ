#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

/// @file SerialTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief Libserialport-based transport implementation for DelegateMQ.
/// 
/// @details
/// This class implements the ITransport interface using the cross-platform `libserialport` 
/// library. It provides a reliable, packet-based communication layer over RS-232/UART 
/// serial links.
/// 
/// Key Features:
/// 1. **Active Object**: Maintains an internal worker thread to handle potentially 
///    blocking port operations (open/close) without freezing the application main loop.
/// 2. **Data Framing**: Encapsulates delegate arguments in a binary-safe frame structure:
///    `[Header (8 bytes)] + [Payload (N bytes)] + [CRC16 (2 bytes)]`.
/// 3. **Data Integrity**: Automatically calculates and verifies a 16-bit CRC for every 
///    packet to detect transmission errors common in serial communication.
/// 4. **Reliability**: Integrates with `TransportMonitor` to track sequence numbers and 
///    support ACK-based reliability when paired with the `RetryMonitor`.
/// 
/// @note This class requires `libserialport` to be linked.

#include "libserialport.h"
#include "DelegateMQ.h" 
#include "predef/util/crc16.h"
#include "predef/transport/ITransportMonitor.h"

#include <sstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstring>

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
        if (ret != SP_OK) {
            std::cerr << "SerialTransport: Could not find port " << portName << std::endl;
            return -1;
        }

        ret = sp_open(m_port, SP_MODE_READ_WRITE);
        if (ret != SP_OK) {
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

        if (m_port) {
            sp_close(m_port);
            sp_free_port(m_port);
            m_port = nullptr;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        // 1. Thread Marshalling
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        if (!m_port) return -1;

        // 2. Prepare Header Copy and Payload
        DmqHeader headerCopy = header;
        std::string payload = os.str();

        // Safety check for 16-bit length limit
        if (payload.length() > UINT16_MAX) {
            std::cerr << "SerialTransport: Payload too large." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // 3. Serialize Header
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // 4. Append Payload
        ss.write(payload.data(), payload.size());

        // 5. Calculate and Append CRC16
        std::string packetWithoutCrc = ss.str();
        uint16_t crc = Crc16CalcBlock((unsigned char*)packetWithoutCrc.c_str(),
            (int)packetWithoutCrc.length(), 0xFFFF);
        ss.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

        std::string packetData = ss.str();

        // 6. Monitor Logic 
        // Always track message (unless it is an ACK). 
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor) {
            m_transportMonitor->Add(seqNum, id);
        }

        // 7. Physical Write
        int result = sp_blocking_write(m_port, packetData.c_str(), packetData.length(), 1000);
        return (result == (int)packetData.length()) ? 0 : -1;
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

        // 1. Read Fixed-Size Header
        char headerBuf[DmqHeader::HEADER_SIZE];
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE, 2000))
            return -1;

        // 2. Deserialize Header
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
        headerStream.write(headerBuf, DmqHeader::HEADER_SIZE);
        headerStream.seekg(0);

        uint16_t marker = 0;
        headerStream.read(reinterpret_cast<char*>(&marker), sizeof(marker));
        header.SetMarker(marker);

        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "SerialTransport: Invalid sync marker!" << std::endl;
            sp_flush(m_port, SP_BUF_INPUT);
            return -1;
        }

        uint16_t id = 0, seqNum = 0, length = 0;
        headerStream.read(reinterpret_cast<char*>(&id), sizeof(id));
        headerStream.read(reinterpret_cast<char*>(&seqNum), sizeof(seqNum));
        headerStream.read(reinterpret_cast<char*>(&length), sizeof(length));
        header.SetId(id);
        header.SetSeqNum(seqNum);
        header.SetLength(length);

        // 3. Read Payload
        if (length > 0)
        {
            if (length > BUFFER_SIZE) return -1;
            if (!ReadExact(m_buffer, length, 1000)) return -1;
            is.write(m_buffer, length);
        }

        // 4. Read & Verify CRC
        uint16_t receivedCrc = 0;
        if (!ReadExact(reinterpret_cast<char*>(&receivedCrc), sizeof(receivedCrc), 500)) return -1;

        uint16_t calcCrc = Crc16CalcBlock((unsigned char*)headerBuf, DmqHeader::HEADER_SIZE, 0xFFFF);
        if (length > 0) calcCrc = Crc16CalcBlock((unsigned char*)m_buffer, length, calcCrc);

        if (receivedCrc != calcCrc) {
            std::cerr << "SerialTransport: CRC mismatch!" << std::endl;
            sp_flush(m_port, SP_BUF_INPUT);
            return -1;
        }

        // 5. Ack Logic
        if (id == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) m_transportMonitor->Remove(seqNum);
        }
        else {
            if (m_sendTransport) {
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
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
    bool ReadExact(char* dest, size_t size, unsigned int timeoutMs)
    {
        size_t totalRead = 0;
        while (totalRead < size)
        {
            int ret = sp_blocking_read(m_port, dest + totalRead, size - totalRead, timeoutMs);
            if (ret <= 0) return false;
            totalRead += ret;
        }
        return true;
    }

    sp_port* m_port = nullptr;
    Thread m_thread;
    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif