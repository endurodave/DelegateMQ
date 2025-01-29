/// @file
/// @brief Implement a countdown latch using C++20 std::latch and delegates. 

#if defined(_MSVC_LANG) && _MSVC_LANG >= 202002L || __cplusplus >= 202002L

#include "CountdownLatch.h"
#include "DelegateMQ.h"
#include <iostream>
#include <chrono>
#include <latch>

using namespace DelegateMQ;
using namespace std;

namespace Example
{
    // Process is always called by a Thread instance, not a std::thread
    // from the thread pool.
    static void process(int id, std::latch& startLatch) {
        startLatch.wait();  // Wait until all threads are ready to start

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work
        std::cout << "Task " << id << " is done!" << std::endl;
    }

    // Countdown Latch (Synchronizing Tasks Before Proceeding). Assumes the 
    // process() must be called by a user-defined worker thread, not a random
    // std::thread from the thread pool. 
    void CountdownLatchExample()
    {
        const int numThreads = 3;
        std::latch startLatch(numThreads);  // Wait for 3 threads

        // Create worker threads
        Thread wt1("Thread1");
        wt1.CreateThread();
        Thread wt2("Thread2");
        wt2.CreateThread();
        Thread wt3("Thread3");
        wt3.CreateThread();

        // Create async blocking delegates
        auto delegate1 = MakeDelegate(&process, wt1, WAIT_INFINITE);
        auto delegate2 = MakeDelegate(&process, wt2, WAIT_INFINITE);
        auto delegate3 = MakeDelegate(&process, wt3, WAIT_INFINITE);

        // Create a thread attached to a delegate
        std::thread t1(delegate1, 1, std::ref(startLatch));
        std::thread t2(delegate2, 2, std::ref(startLatch));
        std::thread t3(delegate3, 3, std::ref(startLatch));

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate some setup time
        std::cout << "All threads ready, proceeding..." << std::endl;

        startLatch.count_down();  // Decrease latch count and allow threads to proceed
        startLatch.count_down();
        startLatch.count_down();

        t1.join();
        t2.join();
        t3.join();
    }
}
#endif