/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ with remote delegates examples. 

#include "sender.h"
#include "receiver.h"

int main() 
{
    const DelegateRemoteId id = 1;

    // Create Sender and Receiver objects
    Receiver receiver(id);
    Sender sender(id);

    sender.Start();
    receiver.Start();

    // Let sender and receiver communicate
    this_thread::sleep_for(chrono::seconds(5));

    receiver.Stop();
    sender.Stop();

    return 0;
}