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
    static const size_t HEADER_SIZE = 6;

    // Constructor
    DmqHeader() = default;
    DmqHeader(uint16_t id, uint16_t seqNum)
        : m_id(id), m_seqNum(seqNum) {}

    // Getter for the MARKER
    uint16_t GetMarker() const
    {
        return SwapIfNeeded(m_marker);
    }

    // Getter for id
    uint16_t GetId() const
    {
        return SwapIfNeeded(m_id);
    }

    // Getter for seqNum
    uint16_t GetSeqNum() const
    {
        return SwapIfNeeded(m_seqNum);
    }

    // Setter for id
    void SetId(uint16_t id)
    {
        m_id = id;  // Store as is (in system native endianness)
    }

    // Setter for seqNum
    void SetSeqNum(uint16_t seqNum)
    {
        m_seqNum = seqNum;  // Store as is (in system native endianness)
    }

    // Setter for MARKER
    void SetMarker(uint16_t marker)
    {
        m_marker = marker;  // Store as is (in system native endianness)
    }

    static uint16_t GetNextSeqNum()
    {
        static std::atomic<uint16_t> seqNum(0);

        // Atomically increment and return the previous value
        return seqNum.fetch_add(1);
    }

private:
    uint16_t m_marker = MARKER;          // Static marker value
    uint16_t m_id = 0;                   // DelegateRemoteId (id)
    uint16_t m_seqNum = 0;               // Sequence number

    /// Returns true if little endian.
    bool LE() const
    {
        const static int n = 1;
        const static bool le = (*(char*)&n == 1);
        return le;
    }

    /// Byte swap function for 16-bit integers (if necessary)
    uint16_t SwapIfNeeded(uint16_t value) const
    {
        if (LE())
        {
            return value;  // No swap needed for little-endian
        }
        else
        {
            return (value >> 8) | (value << 8);  // Swap for big-endian
        }
    }
};

#endif
