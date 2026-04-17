/**
 * @file gui/main.cpp
 * @brief GUI CPU — Human Machine Interface & Data Logger
 * 
 * ROLE:
 * This node provides the primary operator interface using the FTXUI terminal library. 
 * It is responsible for:
 * 1. Publishing user commands (START/ABORT) to the distributed DataBus.
 * 2. Visualizing real-time instrument status and centrifuge speed.
 * 3. Maintaining a persistent, timestamped audit trail (Logs subsystem) by 
 *    spying on all network traffic.
 * 
 * ENVIRONMENT: Standard C++ (Desktop OS)
 */

#include "Network.h"
#include "ui/UI.h"
#include "logs/Logs.h"
#include "RemoteConfig.h"
#include "extras/util/NetworkConnect.h"
#include <iostream>
#include <thread>
#include <atomic>

int main() {
    static NetworkContext networkContext;
    std::cout << "Cellutron GUI Processor starting..." << std::endl;

    // 1. Initialize Network
    Network::GetInstance().Initialize(5010); 

    // Register Topics
    Network::GetInstance().RegisterIncomingTopic<CentrifugeSpeedMsg>("cell/cmd/centrifuge_speed", RID_CENTRIFUGE_SPEED, serSpeed);
    Network::GetInstance().RegisterIncomingTopic<RunStatusMsg>("cell/status/run", RID_RUN_STATUS, serRun);
    Network::GetInstance().RegisterIncomingTopic<FaultMsg>("cell/fault", RID_FAULT_EVENT, serFault);
    Network::GetInstance().RegisterIncomingTopic<ActuatorStatusMsg>("hw/status/actuator", RID_ACTUATOR_STATUS, serActuator);
    Network::GetInstance().RegisterIncomingTopic<SensorStatusMsg>("hw/status/sensor", RID_SENSOR_STATUS, serSensor);

    Network::GetInstance().AddRemoteNode("Controller", "127.0.0.1", 5011);
    Network::GetInstance().MapTopicToRemote("cell/cmd/run", RID_START_PROCESS, "Controller");
    Network::GetInstance().RegisterOutgoingTopic<StartProcessMsg>("cell/cmd/run", serStart);
    Network::GetInstance().MapTopicToRemote("cell/cmd/abort", RID_STOP_PROCESS, "Controller");
    Network::GetInstance().RegisterOutgoingTopic<StopProcessMsg>("cell/cmd/abort", serStop);

    // 2. Initialize Logging Subsystem
    Logs::GetInstance().Initialize();

    // 3. Setup Timer processing for the GUI
    std::atomic<bool> timerRunning{true};
    Thread timerThread("TimerThread");
    timerThread.CreateThread();
    dmq::MakeDelegate([&]() {
        while (timerRunning) {
            Timer::ProcessTimers();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }, timerThread).AsyncInvoke();

    // 4. Start the User Interface (blocks until UI exit)
    UI::GetInstance().Start();

    // 5. Shutdown
    timerRunning = false;
    timerThread.ExitThread();
    
    Logs::GetInstance().Shutdown();
    Network::GetInstance().Shutdown();

    return 0;
}
