/// @file
/// @brief Implement an asynchronous method invocation (AMI) design pattern example. 
/// Assume the AsyncExample class cannot use a random std::async thread pool thread.
/// Instead, the target function must execute on a user-defined thread of control. 
/// See Async-SQL repo that wraps sqlite library with an asynchronous API.
/// https://github.com/endurodave/Async-SQLite

#include "AsyncMethodInvocation.h"
#include "DelegateMQ.h"
#include <iostream>
#include <thread>
#include <future>
#include <chrono>

using namespace dmq;
using namespace std;

namespace Example
{
    // A class that demonstrates the Asynchronous Method Invocation (AMI) design pattern
    class AsyncExample {
    private:
        // Async APIs use workerThread for execution context
        Thread workerThread;

    public:
        AsyncExample() : workerThread("AsyncExample") {
            workerThread.CreateThread();
        }

        // Asynchronous wrapper using delegates
        std::future<int> asyncFactorial(int n) {
            // Create a delegate that will invoke factorial on workerThread when called
            auto asyncDelegate = MakeDelegate(this, &AsyncExample::factorial, workerThread, WAIT_INFINITE);

            // std::async starts the task asynchronously on workerThread thread
            return std::async(std::launch::async, asyncDelegate, n);
        }

    private:
        // Asynchronous factorial calculation
        int factorial(int n) {
            std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate long computation
            int result = 1;
            for (int i = 1; i <= n; ++i) {
                result *= i;
            }
            return result;
        }
    };

    void AsyncMethodInvocationExample()
    {
        AsyncExample asyncExample;

        // Start the asynchronous operation
        std::cout << "Starting factorial calculation asynchronously..." << std::endl;
        std::future<int> result = asyncExample.asyncFactorial(5);

        // While waiting for the result, the main thread can do other tasks
        std::cout << "Main thread is doing other tasks..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulate some work

        // Wait for the result of the asynchronous operation
        int factorialResult = result.get();  // This will block if the result is not ready yet

        std::cout << "Factorial of 5 is: " << factorialResult << std::endl;
    }
}
