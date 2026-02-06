#ifndef STM32_UART_TRANSPORT_H
#define STM32_UART_TRANSPORT_H

/// @file Stm32UartTransport.h
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2026.
///
/// @brief STM32 HAL UART transport implementation for DelegateMQ.
///
/// @details
/// This class provides a concrete implementation of the `ITransport` interface using 
/// the STM32 HAL UART API. It enables DelegateMQ to send and receive serialized 
/// delegates over a serial connection (e.g., RS-232 on the Discovery Base Board).
///
/// **Key Features:**
/// * **Hardware Abstraction:** Wraps a standard `UART_HandleTypeDef` (e.g., `huart6`).
/// * **Robust Sync:** Implements byte-by-byte scanning to recover from framing errors.
/// * **Memory Safety:** Uses safe `memcpy` for serialization to avoid unaligned access faults.
/// * **Reliability:** Integrated with `TransportMonitor` for ACKs/Retries.
/// * **Heap Free:** Uses a static receive buffer to prevent heap fragmentation.

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

#include "delegate/DelegateOpt.h"
#include "predef/transport/DmqHeader.h"
#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"

#include <cstring> // For memcpy

class Stm32UartTransport : public ITransport
{
public:
    // Additions to Stm32UartTransport class:

public:
    Stm32UartTransport()
        : m_huart(nullptr)
        , m_sendTransport(this)
        , m_recvTransport(this)
    {
    }

    Stm32UartTransport(UART_HandleTypeDef* huart)
        : m_huart(huart)
        , m_sendTransport(this)
        , m_recvTransport(this)
    {
    }

    int Create(UART_HandleTypeDef* huart) {
        m_huart = huart;
        return (m_huart != nullptr) ? 0 : -1;
    }

    // No "Create" needed, generic HAL is initialized in main()
    
    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail()) return -1;

        if (m_sendTransport != this) return -1;

        // 1. Get Payload
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) return -1;
        
        // 2. Prepare Header
        DmqHeader headerCopy = header;
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        // 3. Serialize Header (Network Byte Order)
        // Use a local buffer and memcpy to avoid unaligned access faults on strict ARM cores
        uint8_t packet[DmqHeader::HEADER_SIZE];
        
        // Endian swap helper
        auto to_net = [](uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); };
        
        uint16_t val;
        
        val = to_net(headerCopy.GetMarker());
        memcpy(&packet[0], &val, 2);

        val = to_net(headerCopy.GetId());
        memcpy(&packet[2], &val, 2);

        val = to_net(headerCopy.GetSeqNum());
        memcpy(&packet[4], &val, 2);

        val = to_net(headerCopy.GetLength());
        memcpy(&packet[6], &val, 2);

        // 4. Track Reliability (Must happen before send)
        if (header.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
             m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());

        // 5. Send Header (Blocking)
        if (HAL_UART_Transmit(m_huart, packet, DmqHeader::HEADER_SIZE, 100) != HAL_OK)
            return -1;

        // 6. Send Payload (Blocking)
        if (payload.length() > 0) {
            if (HAL_UART_Transmit(m_huart, (uint8_t*)payload.data(), (uint16_t)payload.length(), 500) != HAL_OK)
                return -1;
        }

        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        if (m_recvTransport != this) return -1;

        uint8_t headerBuf[DmqHeader::HEADER_SIZE];
        
        // 1. Sync Loop: Scan for the Marker High Byte
        // This recovers the stream if we joined in the middle of a packet.
        uint8_t b = 0;
        uint8_t markerHigh = (uint8_t)(DmqHeader::MARKER >> 8); // Assumes Big Endian on wire

        while (true)
        {
            // Read 1 byte, wait forever (or long timeout)
            // Note: HAL_MAX_DELAY allows this thread to block until data arrives.
            if (HAL_UART_Receive(m_huart, &b, 1, HAL_MAX_DELAY) != HAL_OK) return -1;

            if (b == markerHigh) {
                headerBuf[0] = b; // Found sync byte
                break;
            }
        }
        
        // 2. Read rest of header (7 bytes)
        if (HAL_UART_Receive(m_huart, &headerBuf[1], DmqHeader::HEADER_SIZE - 1, 100) != HAL_OK) 
            return -1;

        // 3. Deserialize (Safe memcpy)
        auto from_net = [](uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); };
        uint16_t val;

        memcpy(&val, &headerBuf[0], 2); header.SetMarker(from_net(val));
        memcpy(&val, &headerBuf[2], 2); header.SetId(from_net(val));
        memcpy(&val, &headerBuf[4], 2); header.SetSeqNum(from_net(val));
        memcpy(&val, &headerBuf[6], 2); header.SetLength(from_net(val));

        // 4. Verify Marker matches full 16-bit value
        if (header.GetMarker() != DmqHeader::MARKER) 
            return -1; // Sync error (false positive on first byte)

        // 5. Read Payload
        uint16_t len = header.GetLength();
        if (len > 0) {
            // Check against static buffer size
            if (len > BUFFER_SIZE) return -1; // Packet too large

            if (HAL_UART_Receive(m_huart, m_rxBuffer, len, 500) != HAL_OK) 
                return -1;
            
            // Clear stream and write
            is.clear();
            is.str("");
            is.write((char*)m_rxBuffer, len);
        }

        // 6. Handle Reliability Logic (ACKs)
        if (header.GetId() == dmq::ACK_REMOTE_ID) {
            if (m_transportMonitor) 
                m_transportMonitor->Remove(header.GetSeqNum());
        }
        else if (m_transportMonitor && m_sendTransport) {
             // Send Auto-ACK
             xostringstream ss;
             DmqHeader ack;
             ack.SetId(dmq::ACK_REMOTE_ID);
             ack.SetSeqNum(header.GetSeqNum());
             m_sendTransport->Send(ss, ack);
        }

        return 0;
    }

    void Close() { }

    void SetTransportMonitor(ITransportMonitor* tm) { m_transportMonitor = tm; }
    void SetSendTransport(ITransport* st) { m_sendTransport = st; }
    void SetRecvTransport(ITransport* rt) { m_recvTransport = rt; }

private:
    UART_HandleTypeDef* m_huart;
    ITransport* m_sendTransport;
    ITransport* m_recvTransport;
    ITransportMonitor* m_transportMonitor = nullptr;

    // Static buffer to avoid heap fragmentation in embedded loop
    static const int BUFFER_SIZE = 512;
    uint8_t m_rxBuffer[BUFFER_SIZE];
};

#endif // STM32_UART_TRANSPORT_H
