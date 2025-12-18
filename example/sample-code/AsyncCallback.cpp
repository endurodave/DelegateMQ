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
    // The thread where all callbacks will be invoked
    static Thread callback_thread("CallbackThread");

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
            // The invocation happens on the subscriber's target thread (if specified),
            // or on the current thread if the subscriber didn't specify a thread.
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
        void Init(Sensor& sensor)
        {
            // We use shared_from_this() so the delegate holds a weak_ptr.
            // This prevents crashes if DataLogger is destroyed before the callback runs.
            sensor.OnData += MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                callback_thread // Force callback to run on our worker thread
            );
        }

        // 2. Term: Must allow manual unregistration before destruction
        void Term(Sensor& sensor)
        {
            sensor.OnData -= MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                callback_thread
            );
        }

        ~DataLogger()
        {
            // WARNING: Cannot call shared_from_this() here!
            // Term() must be called by the user before this object dies.
            cout << "DataLogger destroyed" << endl;
        }

    private:
        void HandleData(int value, const std::string& source)
        {
            // This runs on 'callback_thread'
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

        void Init(Sensor& sensor)
        {
            // Create the delegate ONCE and store it as a member.
            // We still use shared_from_this() for safety.
            m_delegate = MakeDelegate(
                shared_from_this(),
                &DataAnalyzer::Analyze,
                callback_thread
            );

            // Register using the member
            sensor.OnData += m_delegate;
        }

        // Destructor handles unregistration automatically!
        ~DataAnalyzer()
        {
            // We can safely access m_delegate here. 
            // Even though 'this' is partially destroyed, the delegate member 
            // is still valid enough to identify itself for removal.
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
    // Fixed Name: Matches linker expectation (Singular 'Callback')
    void AsyncCallbackExample()
    {
        callback_thread.CreateThread();

        Sensor sensor;

        // 1. Setup Subscriber 1 (Manual)
        auto logger = std::make_shared<DataLogger>();
        logger->Init(sensor);

        // 2. Setup Subscriber 2 (RAII)
        auto analyzer = std::make_shared<DataAnalyzer>();
        analyzer->Init(sensor);

        // 3. Setup Subscriber 3 (Lambda)
        // Fixed: Explicitly typed std::function to allow MakeDelegate deduction
        std::function<void(int, const std::string&)> lambdaFunc =
            [](int val, const std::string& src) {
            cout << "[Lambda] Got " << val << " on thread " << Thread::GetCurrentThreadId() << endl;
            };

        // Create async delegate from std::function
        auto lambdaDelegate = MakeDelegate(lambdaFunc, callback_thread);
        sensor.OnData += lambdaDelegate;

        // 4. Produce Data
        // These calls return immediately; callbacks run on callback_thread
        sensor.ProduceData(10);
        sensor.ProduceData(20);

        // Give time for callbacks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 5. Cleanup

        // Manual cleanup for Logger
        logger->Term(sensor);
        logger.reset(); // Destroy logger

        // Automatic cleanup for Analyzer
        // We just verify it works by destroying the object
        analyzer->Stop(sensor); // Optional explicit stop
        analyzer.reset();       // Destructor would also handle it if we tracked the sensor

        sensor.OnData -= lambdaDelegate;

        callback_thread.ExitThread();
    }
}