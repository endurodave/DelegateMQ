#ifndef SERIAL_TRANSPORT_H
#define SERIAL_TRANSPORT_H

/// @file SerialTransport.h
/// @brief Libserialport-based transport implementation for DelegateMQ.
/// 
/// @details
/// This class provides a reliable serial communication layer for DelegateMQ using the 
/// libserialport library. It is designed as an "Active Object" with its own internal 
/// worker thread to prevent blocking the caller.
///
/// ### Reliability Stack Structure
/// 1. **TransportMonitor**: Detects missing ACKs via sequence number tracking and timeouts.
/// 2. **RetryMonitor**: Decorates the Send process to store and re-send packets if timed out.
/// 3. **SerialTransport**: The physical layer handling CRC16, binary framing, and COM hardware.
///
/// ### Recursion Management
/// To maintain compatibility with the generic ITransport interface, this class uses a 
/// reentrancy guard (`m_isSending`) and an RAII `SendGuard`. This allows the `RetryMonitor` 
/// to call `Send()` recursively without causing a stack overflow; the second call is 
/// automatically diverted directly to the physical hardware write (`RawPhysicalSend`).
///
/// ### Data Framing (Binary Safe)
/// [Sync Marker (2)] + [Remote ID (2)] + [Seq Num (2)] + [Payload Len (2)] + [Payload (N)] + [CRC16 (2)]
/// 
/// @see https://github.com/endurodave/DelegateMQ
/// @see https://sigrok.org/wiki/Libserialport

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

    /// @brief Public Entry Point for ITransport
    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        // 1. Thread Marshalling
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::Send, m_thread, dmq::WAIT_INFINITE)(os, header);

        // 2. Recursion Detection
        if (m_isSending) {
            return RawPhysicalSend(os, header);
        }

        // 3. Normal Entry Point with RAII Guard
        if (m_retryMonitor) {
            // Define a local guard that resets the flag when it goes out of scope
            struct SendGuard {
                bool& flag;
                SendGuard(bool& f) : flag(f) { flag = true; }
                ~SendGuard() { flag = false; }
            } guard(m_isSending);

            // Even if SendWithRetry returns -1 early, 'guard' destructor runs!
            return m_retryMonitor->SendWithRetry(os, header);
        }

        // 4. Fallback (No monitor attached)
        return RawPhysicalSend(os, header);
    }

    /// @brief The actual "Bit-Banging" method. 
    /// Note: This is PUBLIC so the RetryMonitor can call it without causing recursion.
    int RawPhysicalSend(xostringstream& os, const DmqHeader& header)
    {
        if (!m_port) return -1;

        // 1. Prepare Header Copy and Payload
        DmqHeader headerCopy = header;
        std::string payload = os.str();
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // 2. Serialize Header
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // 3. Append Payload safely (Binary safe)
        ss.write(payload.data(), payload.size());

        // 4. Calculate and Append CRC16
        std::string packetWithoutCrc = ss.str();
        uint16_t crc = Crc16CalcBlock((unsigned char*)packetWithoutCrc.c_str(),
            (int)packetWithoutCrc.length(), 0xFFFF);
        ss.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

        std::string packetData = ss.str();

        // 5. Monitor Logic (Only for non-ACKs)
        // If RetryMonitor is active, IT will handle the Add() call. 
        // We only Add() here if sending directly without a RetryMonitor.
        if (!m_retryMonitor && id != dmq::ACK_REMOTE_ID && m_transportMonitor) {
            m_transportMonitor->Add(seqNum, id);
        }

        // 6. Physical Write
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

        // 4. Read Trailing CRC (2 bytes)
        uint16_t receivedCrc = 0;
        if (!ReadExact(reinterpret_cast<char*>(&receivedCrc), sizeof(receivedCrc), 500))
        {
            std::cerr << "SerialTransport: Failed to read trailing CRC." << std::endl;
            return -1;
        }

        // 5. Verify Integrity
        // Calculate CRC over Header (headerBuf) + Payload (m_buffer)
        uint16_t calcCrc = Crc16CalcBlock((unsigned char*)headerBuf, DmqHeader::HEADER_SIZE, 0xFFFF);
        if (length > 0) {
            calcCrc = Crc16CalcBlock((unsigned char*)m_buffer, length, calcCrc);
        }

        if (receivedCrc != calcCrc)
        {
            std::cerr << "SerialTransport: CRC mismatch! Data corrupted." << std::endl;
            sp_flush(m_port, SP_BUF_INPUT); // Flush to attempt re-sync
            return -1;
        }

        // 6. Ack Logic
        if (id == dmq::ACK_REMOTE_ID)
        {
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {
            if (m_sendTransport)
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

    void SetRetryMonitor(RetryMonitor* retryMonitor) 
    {
        if (Thread::GetCurrentThreadId() != m_thread.GetThreadId())
            return dmq::MakeDelegate(this, &SerialTransport::SetRetryMonitor, m_thread, dmq::WAIT_INFINITE)(retryMonitor);
        m_retryMonitor = retryMonitor;
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
    RetryMonitor* m_retryMonitor = nullptr;
    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    bool m_isSending = false; // The reentrancy guard

    // @TODO Update buffer size if necessary. Max 64KB for 16-bit length.
    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif // SERIAL_TRANSPORT_H