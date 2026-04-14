#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#if defined(DMQ_DATABUS)

int DataBusQosTestMain() {
    std::cout << "Starting DataBusQosTest..." << std::endl;

    // 1. Test Last Value Cache (LVC)
    {
        std::cout << "Testing LVC..." << std::endl;
        dmq::DataBus::ResetForTesting();
        
        // Ensure LVC is enabled for this topic by doing a dummy subscription or 
        // using a future "EnableLVC" method. For now, a subscription does it.
        {
            dmq::QoS qos;
            qos.lastValueCache = true;
            auto conn = dmq::DataBus::Subscribe<int>("status", [](int){}, nullptr, qos);
        }

        // Publish before the "real" subscriber
        dmq::DataBus::Publish<int>("status", 10);
        
        // Subscribe WITH LVC
        int receivedLvc = 0;
        {
            dmq::QoS qos;
            qos.lastValueCache = true;
            auto conn = dmq::DataBus::Subscribe<int>("status", [&](int val) {
                receivedLvc = val;
            }, nullptr, qos);
            
            ASSERT_TRUE(receivedLvc == 10); // Should receive the cached value immediately
        }
        
        // Subscribe WITHOUT LVC
        int receivedNoLvc = 0;
        {
            auto conn = dmq::DataBus::Subscribe<int>("status", [&](int val) {
                receivedNoLvc = val;
            });
            ASSERT_TRUE(receivedNoLvc == 0); // Should NOT receive the cached value
        }
    }

    // 2. Test Topic Filtering
    {
        std::cout << "Testing Filtering..." << std::endl;
        dmq::DataBus::ResetForTesting();

        int lastLowValue = 0;
        auto conn = dmq::DataBus::SubscribeFilter<int>("sensor", 
            [&](int val) { lastLowValue = val; },
            [](int val) { return val < 50; } // Only values < 50
        );

        dmq::DataBus::Publish<int>("sensor", 30);
        ASSERT_TRUE(lastLowValue == 30);

        dmq::DataBus::Publish<int>("sensor", 70);
        ASSERT_TRUE(lastLowValue == 30); // Should still be 30, 70 was filtered out

        dmq::DataBus::Publish<int>("sensor", 10);
        ASSERT_TRUE(lastLowValue == 10);
    }

    // 3. Test Lifespan QoS
    {
        std::cout << "Testing Lifespan..." << std::endl;
        dmq::DataBus::ResetForTesting();

        // Arm LVC for the topic
        {
            dmq::QoS qos;
            qos.lastValueCache = true;
            auto conn = dmq::DataBus::Subscribe<int>("lvc/topic", [](int){}, nullptr, qos);
        }
        dmq::DataBus::Publish<int>("lvc/topic", 42);

        // 3a. Within lifespan: LVC should be delivered
        {
            int received = 0;
            dmq::QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(500);
            auto conn = dmq::DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 42);
        }

        // 3b. Without lifespan: LVC is always delivered regardless of age
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            int received = 0;
            dmq::QoS qos;
            qos.lastValueCache = true;
            // no lifespan set
            auto conn = dmq::DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 42);
        }

        // 3c. Past lifespan: LVC should NOT be delivered
        {
            int received = 0;
            dmq::QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(10); // already expired (we slept 60ms above)
            auto conn = dmq::DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 0);
        }

        // 3d. Fresh publish resets the timestamp; new subscriber within lifespan receives it
        {
            dmq::DataBus::Publish<int>("lvc/topic", 99);
            int received = 0;
            dmq::QoS qos;
            qos.lastValueCache = true;
            qos.lifespan = std::chrono::milliseconds(500);
            auto conn = dmq::DataBus::Subscribe<int>("lvc/topic", [&](int val) {
                received = val;
            }, nullptr, qos);
            ASSERT_TRUE(received == 99);
        }
    }

    // 4. Test Min Separation QoS
    {
        std::cout << "Testing Min Separation..." << std::endl;
        dmq::DataBus::ResetForTesting();

        // 4a. Rapid publishes are throttled: only the first gets through
        {
            int deliveryCount = 0;
            dmq::QoS qos;
            qos.minSeparation = std::chrono::milliseconds(100);
            auto conn = dmq::DataBus::Subscribe<int>("fast/topic", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            for (int i = 0; i < 5; i++) {
                dmq::DataBus::Publish<int>("fast/topic", i);
            }
            ASSERT_TRUE(deliveryCount == 1);
        }

        // 4b. After minSeparation elapses, the next publish goes through
        {
            int deliveryCount = 0;
            dmq::QoS qos;
            qos.minSeparation = std::chrono::milliseconds(50);
            auto conn = dmq::DataBus::Subscribe<int>("rate/topic", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            dmq::DataBus::Publish<int>("rate/topic", 1); // passes
            ASSERT_TRUE(deliveryCount == 1);

            dmq::DataBus::Publish<int>("rate/topic", 2); // dropped, too soon
            ASSERT_TRUE(deliveryCount == 1);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            dmq::DataBus::Publish<int>("rate/topic", 3); // passes, enough time elapsed
            ASSERT_TRUE(deliveryCount == 2);

            dmq::DataBus::Publish<int>("rate/topic", 4); // dropped, too soon
            ASSERT_TRUE(deliveryCount == 2);
        }

        // 4c. Each subscriber has an independent rate limit
        {
            dmq::DataBus::ResetForTesting();
            int throttledCount = 0;
            int unthrottledCount = 0;

            dmq::QoS slowQos;
            slowQos.minSeparation = std::chrono::milliseconds(200);

            auto conn1 = dmq::DataBus::Subscribe<int>("shared/topic", [&](int) {
                throttledCount++;
            }, nullptr, slowQos);

            auto conn2 = dmq::DataBus::Subscribe<int>("shared/topic", [&](int) {
                unthrottledCount++;
            }); // no rate limit

            dmq::DataBus::Publish<int>("shared/topic", 1);
            dmq::DataBus::Publish<int>("shared/topic", 2);
            dmq::DataBus::Publish<int>("shared/topic", 3);

            ASSERT_TRUE(throttledCount == 1);   // only first got through
            ASSERT_TRUE(unthrottledCount == 3); // all got through
        }

        // 4d. Min separation with LVC: the LVC delivery counts as the first delivery,
        //     so an immediate publish afterwards is throttled
        {
            dmq::DataBus::ResetForTesting();

            {
                dmq::QoS qos;
                qos.lastValueCache = true;
                auto conn = dmq::DataBus::Subscribe<int>("lvc/fast", [](int){}, nullptr, qos);
            }
            dmq::DataBus::Publish<int>("lvc/fast", 99);

            int deliveryCount = 0;
            dmq::QoS qos;
            qos.lastValueCache = true;
            qos.minSeparation = std::chrono::milliseconds(100);
            auto conn = dmq::DataBus::Subscribe<int>("lvc/fast", [&](int) {
                deliveryCount++;
            }, nullptr, qos);

            ASSERT_TRUE(deliveryCount == 1); // LVC delivery went through

            dmq::DataBus::Publish<int>("lvc/fast", 100); // dropped: minSeparation not elapsed
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
