#include "DelegateMQ.h"
#include <iostream>
#include <string>

#if defined(DMQ_DATABUS)

int DataBusSpyTestMain() {
    std::cout << "Starting DataBusSpyTest..." << std::endl;

    dmq::DataBus::ResetForTesting();

    // Register stringifiers for topics we want to spy on
    dmq::DataBus::RegisterStringifier<int>("sensor/temp", [](int val) {
        return std::to_string(val) + " C";
    });
    dmq::DataBus::RegisterStringifier<std::string>("system/status", [](const std::string& val) {
        return val;
    });

    std::string lastTopic;
    std::string lastValue;

    // Connect the spy monitor
    auto spyConn = dmq::DataBus::Monitor([&](const std::string& topic, const std::string& value) {
        std::cout << "[SPY] " << topic << " = " << value << std::endl;
        lastTopic = topic;
        lastValue = value;
    });

    // Publish data
    dmq::DataBus::Publish<int>("sensor/temp", 22);
    ASSERT_TRUE(lastTopic == "sensor/temp");
    ASSERT_TRUE(lastValue == "22 C");

    dmq::DataBus::Publish<std::string>("system/status", "OK");
    ASSERT_TRUE(lastTopic == "system/status");
    ASSERT_TRUE(lastValue == "OK");

    // Publish something without a stringifier
    dmq::DataBus::Publish<float>("unknown/topic", 1.23f);
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
