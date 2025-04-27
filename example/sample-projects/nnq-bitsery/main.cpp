/// @file main.cpp
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// @brief NNG (nanomsg-next-gen) and MsgPack with remote delegates examples. 

#include "sender.h"
#include "receiver.h"

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

int main() 
{
    const DelegateRemoteId id = 1;

    // Start the thread that will run ProcessTimers
    std::thread timerThread(ProcessTimers);

    // Create Sender and Receiver objects
    Sender sender(id);
    Receiver receiver(id);

    sender.Start();
    receiver.Start();

    // Let sender and receiver communicate
    this_thread::sleep_for(chrono::seconds(5));

    receiver.Stop();
    sender.Stop();

    // Ensure the timer thread completes before main exits
    processTimerExit.store(true);
    if (timerThread.joinable())
        timerThread.join();

    return 0;
}