#include "DelegateMQ.h"
#include <iostream>
#include <string>

#if defined(DMQ_DATABUS)

using namespace dmq;
using namespace dmq::databus;

int DataBusSpyTestMain() {
    std::cout << "Starting DataBusSpyTest..." << std::endl;

    DataBus::ResetForTesting();

    // Register stringifiers for topics we want to spy on
    DataBus::RegisterStringifier<int>("sensor/temp", [](int val) {
        return std::to_string(val) + " C";
    });
    DataBus::RegisterStringifier<std::string>("system/status", [](const std::string& val) {
        return val;
    });

    std::string lastTopic;
    std::string lastValue;
    uint64_t lastTimestamp = 0;

    // Connect the spy monitor
    auto spyConn = DataBus::Monitor([&](const SpyPacket& packet) {
        std::cout << "[SPY] " << packet.timestamp_us << " " << packet.topic << " = " << packet.value << std::endl;
        lastTopic = packet.topic;
        lastValue = packet.value;
        lastTimestamp = packet.timestamp_us;
    });

    // Publish data
    DataBus::Publish<int>("sensor/temp", 22);
    ASSERT_TRUE(lastTopic == "sensor/temp");
    ASSERT_TRUE(lastValue == "22 C");
    ASSERT_TRUE(lastTimestamp > 0);

    DataBus::Publish<std::string>("system/status", "OK");
    ASSERT_TRUE(lastTopic == "system/status");
    ASSERT_TRUE(lastValue == "OK");

    // Publish something without a stringifier
    DataBus::Publish<float>("unknown/topic", 1.23f);
    ASSERT_TRUE(lastTopic == "unknown/topic");
    ASSERT_TRUE(lastValue == "?");

    std::cout << "DataBusSpyTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusSpyTestMain() {
    return 0;
}

#endif
