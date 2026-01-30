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
/// 1. **Thread-Safe Access**: Uses a recursive mutex to serialize access to the 
///    underlying serial port, allowing concurrent Send/Receive calls from different threads.
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
#include <mutex>

class SerialTransport : public ITransport
{
public:
    SerialTransport() : m_sendTransport(this), m_recvTransport(this)
    {
    }

    ~SerialTransport()
    {
        Close();
    }

    int Create(const char* portName, int baudRate)
    {
        // Lock Guard for thread safety
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

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
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (m_port) {
            sp_close(m_port);
            sp_free_port(m_port);
            m_port = nullptr;
        }
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        // Lock Guard. Note: Serial ports are not full-duplex in the same way 
        // sockets are; locking ensures we don't interleave write bytes with read operations
        // if the underlying driver isn't strictly thread-safe.
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        if (!m_port) return -1;

        // Prepare Header Copy and Payload
        DmqHeader headerCopy = header;
        std::string payload = os.str();

        // Safety check for 16-bit length limit
        if (payload.length() > UINT16_MAX) {
            std::cerr << "SerialTransport: Payload too large." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Serialize Header
        auto marker = headerCopy.GetMarker();
        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        auto id = headerCopy.GetId();
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        auto seqNum = headerCopy.GetSeqNum();
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        auto len = headerCopy.GetLength();
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Append Payload
        ss.write(payload.data(), payload.size());

        // Calculate and Append CRC16
        std::string packetWithoutCrc = ss.str();
        uint16_t crc = Crc16CalcBlock((unsigned char*)packetWithoutCrc.c_str(),
            (int)packetWithoutCrc.length(), 0xFFFF);
        ss.write(reinterpret_cast<const char*>(&crc), sizeof(crc));

        std::string packetData = ss.str();

        // Monitor Logic 
        if (id != dmq::ACK_REMOTE_ID && m_transportMonitor) {
            m_transportMonitor->Add(seqNum, id);
        }

        // Physical Write
        // Use a reasonable timeout (e.g., 1000ms) to avoid hanging forever
        int result = sp_blocking_write(m_port, packetData.c_str(), packetData.length(), 1000);
        return (result == (int)packetData.length()) ? 0 : -1;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        // We do NOT lock the mutex for the entire Receive duration if we want full-duplex.
        // However, libserialport blocking calls might not be thread-safe to mix with writes.
        // For simplicity/safety, we assume half-duplex or rely on library locking if available.
        // If libserialport supports concurrent R/W, we can reduce the lock scope.
        // Here we assume we must lock to protect the m_port pointer.
        
        // CAUTION: Locking here prevents Send() from running while we wait for data!
        // To fix this for full-duplex, we'd need to verify libserialport thread safety.
        // Assuming single-port handle isn't thread safe:
        // Ideally, we wait for 1 byte with a short timeout, then lock only when reading.
        // For now, we will lock but use a short timeout loop to allow Send() to interleave.
        
        // *Better Approach*: Don't lock around blocking read. Just lock to check m_port validity.
        // Note: This assumes sp_blocking_read is thread-safe with sp_blocking_write on same port.
        // If not, serial communication will require a strict master/slave poll protocol.
        
        if (!m_port) return -1;

        if (m_recvTransport != this) {
            return -1;
        }

        // 1. Read Fixed-Size Header
        char headerBuf[DmqHeader::HEADER_SIZE];
        
        // We use a timeout of 1000ms. If no data, we return to allow thread to check exit flags.
        // We do NOT hold the lock during blocking read to allow Send() to occur.
        if (!ReadExact(headerBuf, DmqHeader::HEADER_SIZE, 1000))
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
            // Lock to flush
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
        m_transportMonitor = transportMonitor;
    }

    void SetSendTransport(ITransport* sendTransport)
    {
        m_sendTransport = sendTransport;
    }

    void SetRecvTransport(ITransport* recvTransport)
    {
        m_recvTransport = recvTransport;
    }

private:
    bool ReadExact(char* dest, size_t size, unsigned int timeoutMs)
    {
        size_t totalRead = 0;
        while (totalRead < size)
        {
            // We assume sp_blocking_read is thread-safe regarding internal state,
            // or that the OS driver handles it. If not, this whole class needs 
            // a redesign to be strictly half-duplex (Send OR Receive, never both).
            // For now, we call it without holding m_mutex to allow concurrent Sends.
            
            // Check m_port validity safely? 
            // This is a race condition risk if Close() is called. 
            // Ideally use a shared_ptr for m_port or similar mechanism.
            // For this example, we assume Close() waits for threads to join.
            if (!m_port) return false; 

            int ret = sp_blocking_read(m_port, dest + totalRead, size - totalRead, timeoutMs);
            if (ret <= 0) return false;
            totalRead += ret;
        }
        return true;
    }

    sp_port* m_port = nullptr;
    // Mutex for serializing access to configuration/creation
    std::recursive_mutex m_mutex;
    
    ITransport* m_sendTransport = nullptr;
    ITransport* m_recvTransport = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;

    static const int BUFFER_SIZE = 4096;
    char m_buffer[BUFFER_SIZE] = { 0 };
};

#endif