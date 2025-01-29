/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include <thread>

// Server sensors
static Actuator actuator1(1);
static Actuator actuator2(2);
static Sensor sensor1(1);
static Sensor sensor2(2);

static Thread pollThread("PollThread");

void PollData()
{
    int cnt = 0;
    while (cnt++ < 30)
    {
        // Collect sensor and actuator data
        DataPackage dataPackage;
        dataPackage.actuators.push_back(actuator1.GetState());
        dataPackage.actuators.push_back(actuator2.GetState());
        dataPackage.sensors.push_back(sensor1.GetSensorData());
        dataPackage.sensors.push_back(sensor2.GetSensorData());

        // Send data to remote client
        NetworkMgr::Instance().SendDataPackage(dataPackage);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() 
{
    NetworkMgr::Instance().Start();

    pollThread.CreateThread();

    // TODO: solve startup problem. All threads must be started.
    // Maybe CreateThread() waits for signal from main loop before 
    // returning. 
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Generate remote test data on worker thread
    MakeDelegate(&PollData, pollThread, WAIT_INFINITE).AsyncInvoke();

    NetworkMgr::Instance().Stop();
    pollThread.ExitThread();
    return 0;
}