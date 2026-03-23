#ifndef DMQ_SPY_PACKET_H
#define DMQ_SPY_PACKET_H

#include <string>
#include <cstdint>

namespace dmq {

/// @brief Standardized packet containing bus traffic metadata.
/// @details This struct is passed to DataBus::Monitor subscribers and is 
/// designed to be serialized for transmission to external diagnostic tools.
/// 
/// The timestamp_us field uses dmq::Clock::now(), which typically provides 
/// monotonic time since boot.
struct SpyPacket {
    std::string topic;      ///< The name of the data topic.
    std::string value;      ///< Stringified representation of the data (or "?" if no stringifier registered).
    uint64_t timestamp_us;  ///< Microseconds (usually since boot) when the message was published.

    /// @brief Bitsery serialization method.
    template <typename S>
    void serialize(S& s) {
        s.text1b(topic, 1024);
        s.text1b(value, 8192);
        s.value8b(timestamp_us);
    }
};

} // namespace dmq

#endif // DMQ_SPY_PACKET_H
