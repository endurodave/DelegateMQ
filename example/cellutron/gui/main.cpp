/**
 * @file gui/main.cpp
 * @brief GUI CPU — Human Machine Interface & Data Logger
 */

#include "system/System.h"
#include "ui/UI.h"
#include "extras/util/NetworkConnect.h"
#include <iostream>
#include <thread>

using namespace cellutron;

int main() {
    static NetworkContext networkContext;
    std::cout << "Cellutron GUI Processor starting..." << std::endl;

    System::GetInstance().Initialize();

    // Start a background thread to tick the system (heartbeat warmup, etc.)
    // since UI::Start() is a blocking call.
    std::thread tickThread([]() {
        while (true) {
            System::GetInstance().Tick();
            Thread::Sleep(std::chrono::milliseconds(1000));
        }
    });
    tickThread.detach();

    // 4. Start the User Interface (blocks until UI exit)
    UI::GetInstance().Start();

    System::GetInstance().Shutdown();

    return 0;
}
