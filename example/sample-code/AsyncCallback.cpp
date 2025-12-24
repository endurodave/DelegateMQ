/// @file
/// @brief Examples of receiving asynchronous callbacks on a specific thread.
/// Demonstrates manual lifetime management, RAII automatic management, and lambdas.

#include "DelegateMQ.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <functional> // Required for std::function

using namespace dmq;
using namespace std;

namespace Example
{
    // -------------------------------------------------------------------------
    // Publisher: Simulates a sensor that produces data asynchronously
    // -------------------------------------------------------------------------
    class Sensor
    {
    public:
        // Thread-safe multicast delegate. 
        // Subscribers register here to receive updates.
        MulticastDelegateSafe<void(int value, const std::string& source)> OnData;

        // Simulate incoming data
        void ProduceData(int value)
        {
            // Invoke all registered delegates. 
            OnData(value, "Sensor1");
        }
    };

    // -------------------------------------------------------------------------
    // Subscriber 1: Manual Lifetime Management (Init / Term Pattern)
    // -------------------------------------------------------------------------
    class DataLogger : public std::enable_shared_from_this<DataLogger>
    {
    public:
        DataLogger() = default;

        // 1. Init: Register for callbacks using shared_from_this()
        // Pass workerThread by reference instead of using global static
        void Init(Sensor& sensor, Thread& workerThread)
        {
            sensor.OnData += MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                workerThread
            );
        }

        // 2. Term: Must allow manual unregistration before destruction
        void Term(Sensor& sensor, Thread& workerThread)
        {
            sensor.OnData -= MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                workerThread
            );
        }

        ~DataLogger()
        {
            // WARNING: Cannot call shared_from_this() here!
            cout << "DataLogger destroyed" << endl;
        }

    private:
        void HandleData(int value, const std::string& source)
        {
            cout << "[DataLogger] Received " << value << " from " << source
                << " on thread " << Thread::GetCurrentThreadId() << endl;
        }
    };

    // -------------------------------------------------------------------------
    // Subscriber 2: Automatic RAII Management (Stored Delegate Pattern)
    // -------------------------------------------------------------------------
    class DataAnalyzer : public std::enable_shared_from_this<DataAnalyzer>
    {
    public:
        DataAnalyzer() = default;

        // Pass workerThread by reference
        void Init(Sensor& sensor, Thread& workerThread)
        {
            // Create the delegate ONCE and store it as a member.
            m_delegate = MakeDelegate(
                shared_from_this(),
                &DataAnalyzer::Analyze,
                workerThread
            );

            // Register using the member
            sensor.OnData += m_delegate;
        }

        ~DataAnalyzer()
        {
            // Destructor handles unregistration automatically via m_delegate
            cout << "DataAnalyzer destroyed (Delegate auto-removed)" << endl;
        }

        // Helper to unregister if we have the sensor instance
        void Stop(Sensor& sensor)
        {
            sensor.OnData -= m_delegate;
        }

    private:
        void Analyze(int value, const std::string& source)
        {
            cout << "[DataAnalyzer] Analysis: " << (value * 2)
                << " on thread " << Thread::GetCurrentThreadId() << endl;
        }

        // Store the delegate to support RAII unregistration
        DelegateMemberAsyncSp<DataAnalyzer, void(int, const std::string&)> m_delegate;
    };

    // -------------------------------------------------------------------------
    // Main Example Execution
    // -------------------------------------------------------------------------
    void AsyncCallbackExample()
    {
        Thread callback_thread("CallbackThread");
        callback_thread.CreateThread();

        Sensor sensor;

        // 1. Setup Subscriber 1 (Manual)
        auto logger = std::make_shared<DataLogger>();
        logger->Init(sensor, callback_thread);

        // 2. Setup Subscriber 2 (RAII)
        auto analyzer = std::make_shared<DataAnalyzer>();
        analyzer->Init(sensor, callback_thread);

        // 3. Setup Subscriber 3 (Lambda)
        std::function<void(int, const std::string&)> lambdaFunc =
            [](int val, const std::string& src) {
            cout << "[Lambda] Got " << val << " on thread " << Thread::GetCurrentThreadId() << endl;
            };

        // Create async delegate from std::function using local thread
        auto lambdaDelegate = MakeDelegate(lambdaFunc, callback_thread);
        sensor.OnData += lambdaDelegate;

        // 4. Produce Data
        sensor.ProduceData(10);
        sensor.ProduceData(20);

        // Give time for callbacks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 5. Cleanup

        // Manual cleanup for Logger
        logger->Term(sensor, callback_thread);
        logger.reset();

        // Automatic cleanup for Analyzer
        analyzer->Stop(sensor);
        analyzer.reset();

        sensor.OnData -= lambdaDelegate;

        callback_thread.ExitThread();
    }
}