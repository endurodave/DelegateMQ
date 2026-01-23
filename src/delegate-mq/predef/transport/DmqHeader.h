#ifndef DMQ_HEADER_H
#define DMQ_HEADER_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.

#include <cstdint>
#include <atomic>

/// @brief Header for remote delegate messages. Handles endinesses byte swaps 
/// as necessary. 
class DmqHeader
{
public:
    static const uint16_t MARKER = 0x55AA;
    // 4 fields * 2 bytes = 8 bytes total. Perfectly aligned.
    static const size_t HEADER_SIZE = 8;

    // Constructor
    DmqHeader() = default;
    DmqHeader(uint16_t id, uint16_t seqNum, uint16_t length = 0)
        : m_id(id), m_seqNum(seqNum), m_length(length) {
    }

    // Getter for the MARKER
    uint16_t GetMarker() const { return Swap16(m_marker); }

    // Getter for id
    uint16_t GetId() const { return Swap16(m_id); }

    // Getter for seqNum
    uint16_t GetSeqNum() const { return Swap16(m_seqNum); }

    // Getter for length (Payload size in bytes)
    // 
    // @note Transport Usage:
    // - Stream-based (TCP, Serial, Pipe): REQUIRED. Used to determine how many bytes 
    //   of payload to read after the header to frame the message correctly.
    // - Message-based (UDP, ZeroMQ): OPTIONAL. The transport handles boundaries, but 
    //   checking this field allows for additional data integrity verification.
    uint16_t GetLength() const { return Swap16(m_length); }

    // Setters (Store values in system-native endianness)
    void SetId(uint16_t id) { m_id = id; }
    void SetSeqNum(uint16_t seqNum) { m_seqNum = seqNum; }
    void SetMarker(uint16_t marker) { m_marker = marker; }
    void SetLength(uint16_t length) { m_length = length; }

    static uint16_t GetNextSeqNum()
    {
        static std::atomic<uint16_t> seqNum(0);
        return seqNum.fetch_add(1);
    }

private:
    uint16_t m_marker = MARKER;          // Static marker value
    uint16_t m_id = 0;                   // DelegateRemoteId (id)
    uint16_t m_seqNum = 0;               // Sequence number

    // @TODO Update to 32-bit length if necessary.
    uint16_t m_length = 0;               // Payload length (Max 64KB)

    /// Returns true if little endian.
    bool LE() const
    {
        const static int n = 1;
        const static bool le = (*(char*)&n == 1);
        return le;
    }

    /// Byte swap function for 16-bit integers
    uint16_t Swap16(uint16_t value) const
    {
        if (LE()) return value; // Protocol is Little-Endian
        return (value >> 8) | (value << 8);
    }
};

#endif