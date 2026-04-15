#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <atomic>
#include <thread>

#if defined(DMQ_DATABUS)

int DataBusLocalTestMain() {
    std::cout << "Starting DataBusLocalTest..." << std::endl;
    dmq::DataBus::ResetForTesting();

    // 1. Simple Synchronous Subscription
    {
        bool received = false;
        float receivedValue = 0.0f;
        auto conn = dmq::DataBus::Subscribe<float>("sensor/temp", [&](float value) {
            received = true;
            receivedValue = value;
            std::cout << "Received temp: " << value << std::endl;
        });

        dmq::DataBus::Publish<float>("sensor/temp", 25.5f);
        ASSERT_TRUE(received == true);
        ASSERT_TRUE(receivedValue == 25.5f);
    }

    // 2. Verify ScopedConnection auto-disconnect
    {
        bool received = false;
        {
            auto conn = dmq::DataBus::Subscribe<float>("sensor/temp", [&](float value) {
                (void)value;
                received = true;
            });
        }
        dmq::DataBus::Publish<float>("sensor/temp", 30.0f);
        ASSERT_TRUE(received == false);
    }

    // 3. Multiple subscribers
    {
        int sub1Count = 0;
        int sub2Count = 0;
        auto conn1 = dmq::DataBus::Subscribe<int>("count", [&](int c) { (void)c; sub1Count++; });
        auto conn2 = dmq::DataBus::Subscribe<int>("count", [&](int c) { (void)c; sub2Count++; });

        dmq::DataBus::Publish<int>("count", 1);
        dmq::DataBus::Publish<int>("count", 2);

        ASSERT_TRUE(sub1Count == 2);
        ASSERT_TRUE(sub2Count == 2);
    }

    // 4. Asynchronous Thread Dispatch
    {
        std::cout << "Testing Async Thread Dispatch..." << std::endl;
        Thread workerThread("WorkerThread");
        workerThread.CreateThread();

        std::atomic<bool> asyncReceived{false};
        std::atomic<int> asyncValue{0};
        auto mainThreadId = Thread::GetCurrentThreadId();

        auto conn = dmq::DataBus::Subscribe<int>("async/data", [&](int val) {
            std::cout << "Async callback on thread: " << Thread::GetCurrentThreadId() << std::endl;
            ASSERT_TRUE(Thread::GetCurrentThreadId() != mainThreadId);
            ASSERT_TRUE(Thread::GetCurrentThreadId() == workerThread.GetThreadId());
            asyncValue = val;
            asyncReceived = true;
        }, &workerThread);

        dmq::DataBus::Publish<int>("async/data", 100);

        // Wait for async callback
        int retry = 0;
        const int maxRetries = 100;
        while (!asyncReceived && retry++ < maxRetries) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (!asyncReceived) {
            std::cerr << "FAILED: Async callback not received within 1 second for topic 'async/data'" << std::endl;
            ASSERT_TRUE(false);
        }
        ASSERT_TRUE(asyncValue == 100);

        workerThread.ExitThread();
    }

    // 5. Different types on different topics
    {
        bool stringReceived = false;
        auto conn = dmq::DataBus::Subscribe<std::string>("name", [&](std::string name) {
            stringReceived = (name == "DelegateMQ");
        });
        dmq::DataBus::Publish<std::string>("name", "DelegateMQ");
        ASSERT_TRUE(stringReceived == true);
    }

    std::cout << "DataBusLocalTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusLocalTestMain() {
    return 0;
}

#endif
