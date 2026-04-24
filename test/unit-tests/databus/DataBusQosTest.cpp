#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#if defined(DMQ_DATABUS)

using namespace dmq;
using namespace dmq::databus;

int DataBusQosTestMain() {
    std::cout << "Starting DataBusQosTest..." << std::endl;

    // 1. Test Last Value Cache (LVC)
    {
        std::cout << "Testing LVC..." << std::endl;
        DataBus::ResetForTesting();
        
        // Use the new explicit method to enable LVC for a topic
        DataBus::LastValueCache("status", true);

        // Publish before the "real" subscriber
        DataBus::Publish<int>("status", 10);
        
        // Subscribe WITH LVC
        int receivedLvc = 0;
        {
            QoS qos;
            qos.lastValueCache = true;
            auto conn = DataBus::Subscribe<int>("status", [&](int val) {
                receivedLvc = val;
            }, nullptr, qos);
            
            ASSERT_TRUE(receivedLvc == 10); // Should receive the cached value immediately
        }
        
        // Subscribe WITHOUT LVC
        int receivedNoLvc = 0;
        {
            auto conn = DataBus::Subscribe<int>("status", [&](int val) {
                receivedNoLvc = val;
            });
            ASSERT_TRUE(receivedNoLvc == 0); // Should NOT receive the cached value
        }
    }

    // 2. Test Topic Filtering
    {
        std::cout << "Testing Filtering..." << std::endl;
        DataBus::ResetForTesting();

        int lastLowValue = 0;
        auto conn = DataBus::SubscribeFilter<int>("sensor", 
            [&](int val) { lastLowValue = val; },
            [](int val) { return val < 50; } // Only values < 50
        );

        DataBus::Publish<int>("sensor", 30);
        ASSERT_TRUE(lastLowValue == 30);

        DataBus::Publish<int>("sensor", 70);
        ASSERT_TRUE(lastLowValue == 30); // Should still be 30, 70 was filtered out

        DataBus::Publish<int>("sensor", 10);
        ASSERT_TRUE(lastLowValue == 10);
    }

    // 3. Test Lifespan QoS
    {
        std::cout << "Testing Lifespan..." << std::endl;
        DataBus::ResetForTesting();

        // Use the new explicit method to enable LVC for a topic
        DataBus::LastValueCache("lvc/topic", true);

        DataBus::Publish<int>("lvc/topic", 42);

        // 3a. Within lifespan: LVC should be delivered
        {
            int received = 0;
            QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(500);
            auto conn = DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 42);
        }

        // 3b. Without lifespan: LVC is always delivered regardless of age
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            int received = 0;
            QoS qos;
            qos.lastValueCache = true;
            // no lifespan set
            auto conn = DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 42);
        }

        // 3c. Past lifespan: LVC should NOT be delivered
        {
            int received = 0;
            QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(10); // already expired (we slept 60ms above)
            auto conn = DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 0);
        }

        // 3d. Fresh publish resets the timestamp; new subscriber within lifespan receives it
        {
            DataBus::Publish<int>("lvc/topic", 99);
            int received = 0;
            QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(500);
            auto conn = DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 99);
        }
    }

    // 4. Test Min Separation QoS
    {
        std::cout << "Testing Min Separation..." << std::endl;
        DataBus::ResetForTesting();

        // 4a. Rapid publishes are throttled: only the first gets through
        {
            int deliveryCount = 0;
            QoS qos;
            qos.minSeparation = std::chrono::milliseconds(100);
            auto conn = DataBus::Subscribe<int>("fast/topic", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            for (int i = 0; i < 5; i++) {
                DataBus::Publish<int>("fast/topic", i);
            }
            ASSERT_TRUE(deliveryCount == 1);
        }

        // 4b. After minSeparation elapses, the next publish goes through
        {
            int deliveryCount = 0;
            QoS qos;
            qos.minSeparation = std::chrono::milliseconds(50);
            auto conn = DataBus::Subscribe<int>("rate/topic", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            DataBus::Publish<int>("rate/topic", 1); // passes
            ASSERT_TRUE(deliveryCount == 1);

            DataBus::Publish<int>("rate/topic", 2); // dropped, too soon
            ASSERT_TRUE(deliveryCount == 1);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            DataBus::Publish<int>("rate/topic", 3); // passes, enough time elapsed
            ASSERT_TRUE(deliveryCount == 2);

            DataBus::Publish<int>("rate/topic", 4); // dropped, too soon
            ASSERT_TRUE(deliveryCount == 2);
        }

        // 4c. Each subscriber has an independent rate limit
        {
            DataBus::ResetForTesting();
            int throttledCount = 0;
            int unthrottledCount = 0;

            QoS slowQos;
            slowQos.minSeparation = std::chrono::milliseconds(200);

            auto conn1 = DataBus::Subscribe<int>("shared/topic", [&](int) {
                throttledCount++;
            }, nullptr, slowQos);

            auto conn2 = DataBus::Subscribe<int>("shared/topic", [&](int) {
                unthrottledCount++;
            }); // no rate limit

            DataBus::Publish<int>("shared/topic", 1);
            DataBus::Publish<int>("shared/topic", 2);
            DataBus::Publish<int>("shared/topic", 3);

            ASSERT_TRUE(throttledCount == 1);   // only first got through
            ASSERT_TRUE(unthrottledCount == 3); // all got through
        }

        // 4d. Min separation with LVC: the LVC delivery counts as the first delivery,
        //     so an immediate publish afterwards is throttled
        {
            DataBus::ResetForTesting();

            // Use the new explicit method to enable LVC for a topic
            DataBus::LastValueCache("lvc/fast", true);

            DataBus::Publish<int>("lvc/fast", 99);

            int deliveryCount = 0;
            QoS qos;
            qos.lastValueCache = true;
            qos.minSeparation = std::chrono::milliseconds(100);
            auto conn = DataBus::Subscribe<int>("lvc/fast", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            ASSERT_TRUE(deliveryCount == 1); // LVC delivery went through

            DataBus::Publish<int>("lvc/fast", 100); // dropped: minSeparation not elapsed
            ASSERT_TRUE(deliveryCount == 1);
        }
    }

    std::cout << "DataBusQosTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusQosTestMain() {
    return 0;
}

#endif
