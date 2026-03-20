/// @file
/// @brief Examples of receiving asynchronous callbacks on a specific thread.
/// Demonstrates RAII ScopedConnection lifetime management with Signal.

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
        dmq::Signal<void(int value, const std::string& source)> OnData;

        // Simulate incoming data
        void ProduceData(int value)
        {
            OnData(value, "Sensor1");
        }
    };

    // -------------------------------------------------------------------------
    // Subscriber 1: RAII Connection with Manual Disconnect (Init / Term Pattern)
    // -------------------------------------------------------------------------
    class DataLogger : public std::enable_shared_from_this<DataLogger>
    {
    public:
        DataLogger() = default;

        // 1. Init: Register for callbacks using shared_from_this()
        void Init(Sensor& sensor, Thread& workerThread)
        {
            m_connection = sensor.OnData.Connect(MakeDelegate(
                shared_from_this(),
                &DataLogger::HandleData,
                workerThread
            ));
        }

        // 2. Term: Explicitly disconnect before destruction if needed
        void Term()
        {
            m_connection.Disconnect();
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

        ScopedConnection m_connection;
    };

    // -------------------------------------------------------------------------
    // Subscriber 2: Stored Connection Pattern (Semi-Automatic)
    // -------------------------------------------------------------------------
    class DataAnalyzer : public std::enable_shared_from_this<DataAnalyzer>
    {
    public:
        DataAnalyzer() = default;

        void Init(Sensor& sensor, Thread& workerThread)
        {
            // Connect and store the RAII handle as a member.
            m_connection = sensor.OnData.Connect(MakeDelegate(
                shared_from_this(),
                &DataAnalyzer::Analyze,
                workerThread
            ));
        }

        ~DataAnalyzer()
        {
            // m_connection destructor auto-disconnects
            cout << "DataAnalyzer destroyed (Connection auto-severed)" << endl;
        }

        void Stop()
        {
            // Unregister explicitly using the stored connection
            m_connection.Disconnect();
        }

    private:
        void Analyze(int value, const std::string& source)
        {
            cout << "[DataAnalyzer] Analysis: " << (value * 2)
                << " on thread " << Thread::GetCurrentThreadId() << endl;
        }

        ScopedConnection m_connection;
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
            m_connection = sensor.OnData.Connect(MakeDelegate(
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

        // 1. Setup Subscriber 1 (Manual Disconnect)
        auto logger = xmake_shared<DataLogger>();
        logger->Init(sensor, callback_thread);

        // 2. Setup Subscriber 2 (Stored Connection)
        auto analyzer = xmake_shared<DataAnalyzer>();
        analyzer->Init(sensor, callback_thread);

        // 3. Setup Subscriber 3 (Scoped Connection)
        auto monitor = xmake_shared<DataMonitor>();
        monitor->Init(sensor, callback_thread);

        // 4. Setup Lambda (ScopedConnection)
        std::function<void(int, const std::string&)> lambdaFunc =
            [](int val, const std::string& src) {
            cout << "[Lambda] Got " << val << " on thread " << Thread::GetCurrentThreadId() << endl;
            };
        auto lambdaConnection = sensor.OnData.Connect(MakeDelegate(lambdaFunc, callback_thread));

        // 5. Produce Data
        cout << "\n--- Producing Batch 1 ---" << endl;
        sensor.ProduceData(10); // Monitor ignores (<= 15)
        sensor.ProduceData(20); // Monitor alerts (> 15)

        // Give time for callbacks to process
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 6. Cleanup & Demonstration
        cout << "\n--- Cleanup Phase ---" << endl;

        // Manual disconnect then destroy
        logger->Term();
        logger.reset();

        // Explicit stop, then destroy (m_connection destructor also disconnects)
        analyzer->Stop();
        analyzer.reset();

        // Automatic cleanup (Monitor - ScopedConnection)
        // Just destroying the object disconnects it.
        monitor.reset();

        // 7. Verify Cleanup
        cout << "\n--- Producing Batch 2 (Should only show Lambda) ---" << endl;
        sensor.ProduceData(30);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Cleanup Lambda (explicit disconnect or let lambdaConnection go out of scope)
        lambdaConnection.Disconnect();

        callback_thread.ExitThread();
    }
}
