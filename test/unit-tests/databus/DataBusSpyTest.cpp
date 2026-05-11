#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

#if defined(DMQ_DATABUS)

using namespace dmq;
using namespace dmq::os;
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

    // 2. Async monitor — callback dispatches to the specified worker thread
    {
        DataBus::ResetForTesting();
        Thread monThread("MonitorWorker");
        monThread.CreateThread();

        std::atomic<bool> monitorFired{false};
        std::atomic<bool> calledOnWorker{false};

        auto monConn = DataBus::Monitor([&](const SpyPacket&) {
            calledOnWorker = monThread.IsCurrentThread();
            monitorFired = true;
        }, &monThread);

        DataBus::Publish<int>("async/monitor", 7);

        int retries = 0;
        while (!monitorFired && retries++ < 50)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ASSERT_TRUE(monitorFired == true);
        ASSERT_TRUE(calledOnWorker == true);

        monThread.ExitThread();
    }

    std::cout << "DataBusSpyTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusSpyTestMain() {
    return 0;
}

#endif
