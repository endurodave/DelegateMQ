/// @file
/// @brief A std::async thread targeting example. Use std::async and std::future with 
/// delegates allows the main thread to continue while the specified target thread completes. 

#include "AsyncFuture.h"
#include "DelegateMQ.h"
#include <iostream>
#include <future>
#include <chrono>

using namespace DelegateMQ;
using namespace std;

namespace Example
{
    static Thread comm_thread("CommunicationThread");

    // Assume send_data() is not thread-safe and may only be called on comm_thread context.
    // A random std::async thread from the pool is unacceptable and causes cross-threading.
    static size_t send_data(const std::string& data)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate sending
        return data.size();  // Return the 'bytes_sent' sent result
    }

    void AsyncFutureExample()
    {
        comm_thread.CreateThread();

        // Create an async delegate targeted at send_data()
        auto send_data_delegate = MakeDelegate(&send_data, comm_thread, WAIT_INFINITE);

        // Start the asynchronous task using std::async. send_data() will be called on 
        // comm_thread context.
        std::future<size_t> result = std::async(std::launch::async, send_data_delegate, "send_data message");

        // Do other work while send_data() is executing on comm_thread
        std::cout << "Doing other work in main thread while data is sent...\n";

        // Continue other work in the main thread while the task runs asynchronously
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get bytes sent. This will block until send_data() completes.
        size_t bytes_sent = result.get();

        std::cout << "Result from send_data: " << bytes_sent << std::endl;
        comm_thread.ExitThread();
    }
}
