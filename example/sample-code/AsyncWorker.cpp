/// @file
/// @brief Implement a dedicated worker thread using AsyncInvoke.

#include "AsyncWorker.h"
#include "DelegateMQ.h"
#include <iostream>
#include <atomic>
#include <chrono>

using namespace dmq;
using namespace std;

namespace Example
{
    // -------------------------------------------------------------------------
    // Worker Class
    // -------------------------------------------------------------------------
    class Worker
    {
    public:
        Worker(Thread& t) : m_thread(t) {}

        /// @brief The "User Thread" logic.
        ///
        /// This function runs a custom loop, effectively taking over the 
        /// thread's execution. While this is running, the thread will NOT 
        /// process any other incoming delegates (they will remain queued).
        void DedicatedWorkLoop()
        {
            //  - Ideally shows a thread switching from "Event Loop" to "User Loop"
            cout << "[Worker] Hijacked " << m_thread.GetThreadName() << " for dedicated work." << endl;

            int count = 0;
            while (!m_stop)
            {
                // Simulate heavy processing
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                cout << "[Worker] Processing chunk " << ++count << "..." << endl;
            }

            cout << "[Worker] Dedicated work finished. Returning to event loop." << endl;
        }

        /// @brief Starts the worker on the target thread.
        void Start()
        {
            m_stop = false;

            // Use MakeDelegate(...).AsyncInvoke() for "Fire and Forget".
            // We do NOT wait for the result because the function runs indefinitely.
            // This asynchronously injects our custom loop onto the target thread.
            MakeDelegate(this, &Worker::DedicatedWorkLoop, m_thread).AsyncInvoke();
        }

        /// @brief Signals the worker loop to exit.
        void Stop()
        {
            // We must use an atomic flag because the worker thread is busy 
            // in the loop and cannot process a "Stop" delegate message.
            m_stop = true;
        }

    private:
        Thread& m_thread;
        std::atomic<bool> m_stop{ false };
    };

    // -------------------------------------------------------------------------
    // Main Example
    // -------------------------------------------------------------------------
    void AsyncWorkerExample()
    {
        // 1. Create a generic DelegateMQ Thread
        Thread tempThread("TempUserThread");
        tempThread.CreateThread();

        Worker worker(tempThread);

        cout << "[Main] Starting temporary user thread..." << endl;

        // 2. "Hijack" the thread to run our specific task
        worker.Start();

        // 3. Main thread continues its own work in parallel
        std::this_thread::sleep_for(std::chrono::seconds(2));
        cout << "[Main] Main thread doing other work..." << endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 4. Signal the user loop to end
        cout << "[Main] Signaling worker to stop..." << endl;
        worker.Stop();

        // Wait for the worker to finish its loop and return control to the Thread class
        // (In a real app, you might use a future or another synchronization primitive)
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 5. Clean up
        tempThread.ExitThread();
        cout << "[Main] Example complete." << endl;
    }
}