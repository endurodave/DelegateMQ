#include "DataBus.h"
#include "DelegateMQ.h"
#include <iostream>
#include <string>
#include <atomic>
#include <thread>

#if defined(DMQ_DATABUS)

using namespace std;

namespace Example
{
    void DataBusExample()
    {
        cout << "DataBusExample" << endl;

        // Reset the DataBus to a clean state
        dmq::DataBus::ResetForTesting();

        // 1. Simple Synchronous Subscription
        {
            // Subscribe to "sensor/temp" topic
            auto conn = dmq::DataBus::Subscribe<float>("sensor/temp", [](float value) {
                cout << "Received temp: " << value << " C" << endl;
            });

            // Publish to "sensor/temp" topic
            dmq::DataBus::Publish<float>("sensor/temp", 25.5f);
        }

        // 2. Different types on different topics
        {
            // Subscribe to "system/status" topic
            auto conn = dmq::DataBus::Subscribe<string>("system/status", [](string status) {
                cout << "System status: " << status << endl;
            });

            // Publish to "system/status" topic
            dmq::DataBus::Publish<string>("system/status", "OK");
        }
        
        // 3. Multiple subscribers
        {
            auto conn1 = dmq::DataBus::Subscribe<int>("count", [](int c) { cout << "Sub 1 count: " << c << endl; });
            auto conn2 = dmq::DataBus::Subscribe<int>("count", [](int c) { cout << "Sub 2 count: " << c << endl; });

            dmq::DataBus::Publish<int>("count", 1);
        }

        // 4. Asynchronous Subscription on a worker thread
        {
            cout << "Testing Async Subscription on worker thread..." << endl;
            
            // Create and start a worker thread
            Thread workerThread("DataBusWorker");
            workerThread.CreateThread();

            atomic<bool> received{false};

            // Subscribe to "async/topic" and specify the worker thread for callback execution
            auto conn = dmq::DataBus::Subscribe<int>("async/topic", [&](int val) {
                cout << "Async callback received value: " << val << " on thread: " << Thread::GetCurrentThreadId() << endl;
                received = true;
            }, &workerThread);

            // Publish to "async/topic"
            dmq::DataBus::Publish<int>("async/topic", 123);

            // Wait for the callback to be processed on the worker thread
            while (!received) {
                this_thread::sleep_for(chrono::milliseconds(10));
            }

            workerThread.ExitThread();
        }
    }
}

#else

namespace Example
{
    void DataBusExample() { }
}

#endif
