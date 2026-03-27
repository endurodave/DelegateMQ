// NodeInfoPacket.h
// @see https://github.com/endurodave/DelegateMQ
// Node heartbeat packet for the DelegateMQ Node Monitor.

#ifndef NODE_INFO_PACKET_H
#define NODE_INFO_PACKET_H

#include <string>
#include <cstdint>

namespace dmq {

/// @brief Heartbeat packet broadcast by NodeBridge to identify a node on the DataBus network.
/// @details Periodically serialized and transmitted by NodeBridge. The dmq-monitor console
/// listens for these packets and builds a live view of all active nodes.
struct NodeInfoPacket {
    std::string nodeId;        ///< User-assigned node name (e.g. "SensorNode-1")
    std::string hostname;      ///< Machine hostname
    std::string ipAddress;     ///< Self-reported IP address
    uint64_t    uptimeMs    = 0;    ///< Milliseconds since NodeBridge::Start()
    uint32_t    totalMsgCount = 0;  ///< Total DataBus messages published since start
    std::string topicsStr;     ///< Semicolon-delimited topics (e.g. "sensor/temp;system/status")

    template <typename S>
    void serialize(S& s) {
        s.text1b(nodeId, 256);
        s.text1b(hostname, 256);
        s.text1b(ipAddress, 64);
        s.value8b(uptimeMs);
        s.value4b(totalMsgCount);
        s.text1b(topicsStr, 4096);
    }
};

} // namespace dmq

#endif // NODE_INFO_PACKET_H
