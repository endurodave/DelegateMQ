// NodeInfoPacket.h
// @see https://github.com/DelegateMQ/DelegateMQ
// Node heartbeat packet for the DelegateMQ Node Monitor.

#ifndef NODE_INFO_PACKET_H
#define NODE_INFO_PACKET_H

#include <string>
#include <cstdint>
#include "port/serialize/serialize/msg_serialize.h"

namespace dmq {

/// @brief Heartbeat packet broadcast by NodeBridge to identify a node on the DataBus network.
/// @details Periodically serialized and transmitted by NodeBridge. The dmq-monitor console
/// listens for these packets and builds a live view of all active nodes.
struct NodeInfoPacket : public serialize::I {
    std::string nodeId;        ///< User-assigned node name (e.g. "SensorNode-1")
    std::string hostname;      ///< Machine hostname
    std::string ipAddress;     ///< Self-reported IP address
    uint64_t    uptimeMs    = 0;    ///< Milliseconds since NodeBridge::Start()
    uint32_t    totalMsgCount = 0;  ///< Total DataBus messages published since start
    std::string topicsStr;     ///< Semicolon-delimited topics (e.g. "sensor/temp;system/status")

    std::ostream& write(serialize& ms, std::ostream& os) override {
        ms.write(os, nodeId);
        ms.write(os, hostname);
        ms.write(os, ipAddress);
        ms.write(os, uptimeMs);
        ms.write(os, totalMsgCount);
        return ms.write(os, topicsStr);
    }

    std::istream& read(serialize& ms, std::istream& is) override {
        ms.read(is, nodeId);
        ms.read(is, hostname);
        ms.read(is, ipAddress);
        ms.read(is, uptimeMs);
        ms.read(is, totalMsgCount);
        return ms.read(is, topicsStr);
    }
};

} // namespace dmq

#endif // NODE_INFO_PACKET_H
