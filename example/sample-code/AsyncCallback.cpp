/// @file
/// @brief Examples of receiving asynchronous callbacks on a specific thread.
/// Demonstrates manual lifetime management, stored delegates, and modern RAII signals.

#include "DelegateMQ.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <functional> 

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
        dmq::SignalPtr<void(int value, const std::string& source)> OnData;

        Sensor() {
            // Initialize the signal
            OnData = dmq::MakeSignal<void(int, const std::string&)>();
        }

        // Simulate incoming data
        void ProduceData(int value)
        {
            // Dereference shared_ptr to invoke
            if (OnData) {
                (*OnData)(value, "Sensor1");
            }
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
        void Init(Sensor& sensor, Thread& workerThread)
        {
            // Use (*ptr) += for operator overloading on shared_ptr
            (*sensor.OnData) += MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                workerThread
            );
        }

        // 2. Term: Must allow manual unregistration before destruction
        void Term(Sensor& sensor, Thread& workerThread)
        {
            (*sensor.OnData) -= MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                workerThread
            );
        }

        ~DataLogger()
        {
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
    // Subscriber 2: Stored Delegate Pattern (Semi-Automatic)
    // -------------------------------------------------------------------------
    class DataAnalyzer : public std::enable_shared_from_this<DataAnalyzer>
    {
    public:
        DataAnalyzer() = default;

        void Init(Sensor& sensor, Thread& workerThread)
        {
            // Create the delegate ONCE and store it as a member.
            m_delegate = MakeDelegate(
                shared_from_this(),
                &DataAnalyzer::Analyze,
                workerThread
            );

            // Register using the member
            (*sensor.OnData) += m_delegate;
        }

        ~DataAnalyzer()
        {
            // Destructor handles unregistration automatically via m_delegate
            cout << "DataAnalyzer destroyed (Delegate auto-removed)" << endl;
        }

        void Stop(Sensor& sensor)
        {
            // Unregister using the member
            (*sensor.OnData) -= m_delegate;
        }

    private:
        void Analyze(int value, const std::string& source)
        {
            cout << "[DataAnalyzer] Analysis: " << (value * 2)
                << " on thread " << Thread::GetCurrentThreadId() << endl;
        }

        DelegateMemberAsyncSp<DataAnalyzer, void(int, const std::string&)> m_delegate;
    };

    // -------------------------------------------------------------------------
    // Subscriber 3: Modern RAII Management (ScopedConnection)
    // -------------------------------------------------------------------------
    class DataMonitor : public std::enable_shared_from_this<DataMonitor>
    {
    public:
        DataMonitor() = default;

        void Init(Sensor& sensor, Thread& workerThread)
        {
            // Use arrow operator -> to call Connect() on the shared_ptr
            m_connection = sensor.OnData->Connect(MakeDelegate(
                shared_from_this(),
                &DataMonitor::CheckThreshold,
                workerThread
            ));
        }

        ~DataMonitor() {
            cout << "DataMonitor destroyed (Connection severed)" << endl;
        }

    private:
        void CheckThreshold(int value, const std::string& source)
        {
            if (value > 15) {
                cout << "[DataMonitor] ALERT! High value " << value
                    << " on thread " << Thread::GetCurrentThreadId() << endl;
            }
        }

        ScopedConnection m_connection;
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

        // 2. Setup Subscriber 2 (Stored Delegate)
        auto analyzer = std::make_shared<DataAnalyzer>();
        analyzer->Init(sensor, callback_thread);

        // 3. Setup Subscriber 3 (Scoped Connection)
        auto monitor = std::make_shared<DataMonitor>();
        monitor->Init(sensor, callback_thread);

        // 4. Setup Lambda (Manual)
        std::function<void(int, const std::string&)> lambdaFunc =
            [](int val, const std::string& src) {
            cout << "[Lambda] Got " << val << " on thread " << Thread::GetCurrentThreadId() << endl;
            };
        auto lambdaDelegate = MakeDelegate(lambdaFunc, callback_thread);

        (*sensor.OnData) += lambdaDelegate;

        // 5. Produce Data
        cout << "\n--- Producing Batch 1 ---" << endl;
        sensor.ProduceData(10); // Monitor ignores (<= 15)
        sensor.ProduceData(20); // Monitor alerts (> 15)

        // Give time for callbacks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 6. Cleanup & Demonstration
        cout << "\n--- Cleanup Phase ---" << endl;

        // Manual cleanup 
        logger->Term(sensor, callback_thread);
        logger.reset();

        // Automatic cleanup (Analyzer)
        analyzer->Stop(sensor); // Optional, reset() would also work
        analyzer.reset();

        // Automatic cleanup (Monitor - ScopedConnection)
        // Just destroying the object disconnects it.
        monitor.reset();

        // 7. Verify Cleanup
        cout << "\n--- Producing Batch 2 (Should only show Lambda) ---" << endl;
        sensor.ProduceData(30);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Cleanup Lambda
        (*sensor.OnData) -= lambdaDelegate;

        callback_thread.ExitThread();
    }
}