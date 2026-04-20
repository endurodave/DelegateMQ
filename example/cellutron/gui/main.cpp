/**
 * @file gui/main.cpp
 * @brief GUI CPU — Human Machine Interface & Data Logger
 */

#include "system/System.h"
#include "ui/UI.h"
#include <iostream>

using namespace cellutron;

int main() {
    std::cout << "Cellutron GUI Processor starting..." << std::endl;

    System::GetInstance().Initialize();

    // 4. Start the User Interface (blocks until UI exit)
    UI::GetInstance().Start();

    System::GetInstance().Shutdown();

    return 0;
}
