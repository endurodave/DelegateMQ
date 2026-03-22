#include "DelegateMQ.h"
#include <iostream>
#include <string>

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

    std::cout << "DataBusQosTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusQosTestMain() {
    return 0;
}

#endif
