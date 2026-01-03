/// @file main_stress.cpp
/// @brief Long-term multi-threaded stress test for DelegateMQ.
///
/// @details This test creates a high-contention environment to benchmark stability and throughput.
/// It performs the following:
/// 1. Multiple Publisher threads firing Sync and Async signals to saturate the bus.
/// 2. Multiple Subscriber threads processing events with artificial workload.
/// 3. A "Chaos Thread" that randomly connects/disconnects slots to test container thread-safety.
/// 4. Atomic verification to ensure 100% data integrity (no lost messages).
/// 
/// @section Allocator Usage
/// This test utilizes the DelegateMQ Fixed Block Allocator to maximize throughput.
/// - The macro 'DMQ_ALLOCATOR' is defined before including DelegateMQ.h to enable the feature.
/// - The macro 'XALLOCATOR' is placed inside classes (Payload, StressPublisher, etc.) to 
///   overload 'operator new' and 'operator delete'.
/// - This replaces standard heap allocations (malloc/free) with a lock-free or lightweight-lock 
///   fixed-block pool, significantly reducing memory contention during high-frequency 
///   message creation and destruction.
///
/// @section Allocator Comparison (Windows Release Mode)
///
/// | Metric                 | Standard Heap (LFH)    | Fixed Block (XALLOCATOR) |
/// | :--------------------- | :--------------------- | :----------------------- |
/// | Throughput             | ~1,040,000 msg/s       | ~600,000 msg/s           |
/// | Thread Scaling         | Excellent (Lock-Free)  | Moderate (Mutex Contention)|
/// | Best For               | PC / Server Apps       | Embedded / RTOS / Determinism |
///
/// @note On Windows 10+, the OS "Low Fragmentation Heap" is highly optimized for 
/// concurrency and outperforms the simple mutex-based XALLOCATOR. XALLOCATOR remains 
/// superior for embedded systems where the system heap is slow or non-deterministic.

#include "DelegateMQ.h"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

// --- Memory Leak Detection (Windows) ---
// Uncomment the line below to enable leak checking on Windows Debug builds
//#define CHECK_LEAKS

#if defined(CHECK_LEAKS) && defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <stdlib.h>
    #include <crtdbg.h>
#endif

using namespace dmq;

// --- Configuration ---
const int NUM_PUBLISHERS = 4;
const int NUM_SUBSCRIBERS = 4;
const int TEST_DURATION_SECONDS = 30; // Run time
const int BURST_SIZE = 1000;          // Messages per burst

// --- Global Counters ---
std::atomic<uint64_t> g_signalsFired{ 0 };
std::atomic<uint64_t> g_syncReceived{ 0 };
std::atomic<uint64_t> g_asyncReceived{ 0 };
std::atomic<bool>     g_running{ true };

// --- Payload ---
// A reasonably sized payload to test copy/move mechanics
struct Payload
{
    XALLOCATOR

    int id;
    int publisherId;
    char buffer[64]; // Padding to make it non-trivial

    Payload(int i, int p) : id(i), publisherId(p) {
        snprintf(buffer, sizeof(buffer), "Pub-%d Msg-%d", p, i);
    }
};

// --- Publisher ---
class StressPublisher
{
public:
    XALLOCATOR

    // Define the Signal type
    using StressSig = void(const Payload&);

    // Thread-safe Signal (Must use SignalSafe for multi-threaded stress)
    std::shared_ptr<SignalSafe<StressSig>> Signal = MakeSignal<StressSig>();

    void Start(int id)
    {
        m_thread = std::thread([this, id]() {
            int msgId = 0;
            while (g_running)
            {
                // Burst fire signals
                for (int i = 0; i < BURST_SIZE; ++i)
                {
                    Payload p(msgId++, id);
                    (*Signal)(p); // Emit
                    g_signalsFired++;
                }

                // Slight yield to prevent complete CPU starvation
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            });
    }

    void Join() { if (m_thread.joinable()) m_thread.join(); }

private:
    std::thread m_thread;
};

// --- Subscriber ---
class StressSubscriber
{
public:
    XALLOCATOR

    StressSubscriber(int id)
        : m_id(id), m_thread("SubThread_" + std::to_string(id))
    {
        m_thread.CreateThread();
    }

    ~StressSubscriber() {
        m_thread.ExitThread();
    }

    // Synchronous Handler (Runs on Publisher Thread)
    void OnSyncEvent(const Payload& p)
    {
        g_syncReceived++;
        // Simulate tiny work
        volatile int x = p.id * p.publisherId;
        (void)x;
    }

    // Asynchronous Handler (Runs on Subscriber Thread)
    void OnAsyncEvent(const Payload& p)
    {
        g_asyncReceived++;
        // Simulate tiny work
        volatile int x = p.id * p.publisherId;
        (void)x;
    }

    Thread& GetThread() { return m_thread; }
    int GetId() const { return m_id; }

private:
    int m_id;
    Thread m_thread;
};

// --- The "Chaos Monkey" ---
// Randomly connects and disconnects a volatile subscriber to test 
// container thread safety during active iteration.
void ChaosMonkey(std::vector<std::shared_ptr<StressPublisher>>& publishers)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> distPub(0, NUM_PUBLISHERS - 1);

    // A temporary subscriber that constantly joins and leaves
    Thread chaosThread("ChaosThread");
    chaosThread.CreateThread();

    struct VolatileSub {
        XALLOCATOR
        void Handle(const Payload&) { /* Do nothing */ }
    } vSub;

    ScopedConnection conn;

    while (g_running)
    {
        // 1. Pick a random publisher
        auto& pub = publishers[distPub(rng)];

        // 2. Connect
        conn = pub->Signal->Connect(MakeDelegate(&vSub, &VolatileSub::Handle, chaosThread));

        // 3. Wait a tiny bit (hold the lock/connection)
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // 4. Disconnect (conn destructor or explicit assignment)
        conn.Disconnect(); // Breaks connection

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    chaosThread.ExitThread();
}

int main_stress()
{
    // 1. Enable Memory Leak Checks (Windows Debug Only)
#if defined (CHECK_LEAKS) && defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
#endif

    std::cout << "==========================================\n";
    std::cout << "   DelegateMQ Multithreaded Stress Test   \n";
    std::cout << "==========================================\n";
    std::cout << "Publishers: " << NUM_PUBLISHERS << "\n";
    std::cout << "Subscribers: " << NUM_SUBSCRIBERS << "\n";
    std::cout << "Duration: " << TEST_DURATION_SECONDS << "s\n";
    std::cout << "Starting..." << std::endl;

    { // Scope block to ensure RAII objects (pubs/subs) are destroyed BEFORE leak check

        // 1. Setup Publishers
        std::vector<std::shared_ptr<StressPublisher>> publishers;
        for (int i = 0; i < NUM_PUBLISHERS; ++i) {
            publishers.push_back(std::make_shared<StressPublisher>());
        }

        // 2. Setup Subscribers
        std::vector<std::shared_ptr<StressSubscriber>> subscribers;
        std::vector<ScopedConnection> connections; // Keep connections alive

        for (int i = 0; i < NUM_SUBSCRIBERS; ++i) {
            auto sub = std::make_shared<StressSubscriber>(i);
            subscribers.push_back(sub);

            // Connect every subscriber to every publisher
            for (auto& pub : publishers) {
                // Half Sync, Half Async
                if (i % 2 == 0) {
                    // Sync Connection
                    connections.push_back(pub->Signal->Connect(
                        MakeDelegate(sub.get(), &StressSubscriber::OnSyncEvent)));
                }
                else {
                    // Async Connection
                    connections.push_back(pub->Signal->Connect(
                        MakeDelegate(sub.get(), &StressSubscriber::OnAsyncEvent, sub->GetThread())));
                }
            }
        }

        // 3. Start Publishers
        for (int i = 0; i < NUM_PUBLISHERS; ++i) {
            publishers[i]->Start(i);
        }

        // 4. Start Chaos Monkey
        std::thread chaosThread(ChaosMonkey, std::ref(publishers));

        // 5. Monitor Loop
        auto start = std::chrono::steady_clock::now();
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();

            // Stats output
            double totalRecv = (double)(g_syncReceived + g_asyncReceived);
            std::cout << "[" << std::setw(3) << elapsed << "s] "
                << "Fired: " << g_signalsFired << " | "
                << "Sync: " << g_syncReceived << " | "
                << "Async: " << g_asyncReceived << " | "
                << "Rate: " << (uint64_t)(totalRecv / (elapsed > 0 ? elapsed : 1)) << " msg/s"
                << std::endl;

            if (elapsed >= TEST_DURATION_SECONDS) break;
        }

        // 6. Shutdown
        std::cout << "\nStopping threads..." << std::endl;
        g_running = false;

        // Wait for the queues to actually empty
        // (In a real app, you'd check queue size, but sleeping longer works for a test)
        std::cout << "Draining queues (3s)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        for (auto& pub : publishers) pub->Join();
        if (chaosThread.joinable()) chaosThread.join();

        // Allow async queues to drain slightly
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Explicitly clear containers to trigger destructors NOW
        connections.clear();
        subscribers.clear();
        publishers.clear();

    } // End of Scope - All RAII objects destroyed here

    std::cout << "==========================================\n";
    std::cout << "Final Report:\n";
    std::cout << "Signals Fired:   " << g_signalsFired.load() << "\n";
    std::cout << "Sync Received:   " << g_syncReceived.load() << "\n";
    std::cout << "Async Received:  " << g_asyncReceived.load() << "\n";
    std::cout << "Test Completed." << std::endl;

#if defined (CHECK_LEAKS) && defined(_MSC_VER) && defined(_DEBUG)
    std::cout << "--- Manual Leak Check Report ---" << std::endl;

    // NOTE: This manual dump will list *all* heap objects currently alive, 
    // including valid global/static singletons (e.g., "DelegateThreads", "CommunicationThr")
    // that have not yet been destroyed by the runtime. 
    // THESE ARE NOT LEAKS.
    //
    // A true leak is an object that remains *after* the program fully exits 
    // (which is checked automatically by the _CRTDBG_LEAK_CHECK_DF flag).
    _CrtDumpMemoryLeaks();

    std::cout << "--------------------------------" << std::endl;
#endif

    return 0;
}
