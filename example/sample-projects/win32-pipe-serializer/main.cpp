/// @file main.cpp
/// @see https://github.com/DelegateMQ/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ with remote delegates examples. 

#include "sender.h"
#include "receiver.h"
#include "predef/util/NetworkConnect.h"

std::atomic<bool> processTimerExit = false;
static void ProcessTimers()
{
    while (!processTimerExit.load())
    {
        // Process all delegate-based timers
        Timer::ProcessTimers();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

int main(int argc, char* argv[])
{
    int duration = 5;
    if (argc > 1) duration = atoi(argv[1]);

    const DelegateRemoteId id = 1;

    // Start the thread that will run ProcessTimers
    std::thread timerThread(ProcessTimers);

    // Create Sender and Receiver objects
    Receiver receiver(id);
    Sender sender(id);

    sender.Start();
    receiver.Start();

    // Let sender and receiver communicate
    this_thread::sleep_for(chrono::seconds(duration));

    receiver.Stop();
    sender.Stop();

    // Ensure the timer thread completes before main exits
    processTimerExit.store(true);
    if (timerThread.joinable())
        timerThread.join();

    return 0;
}