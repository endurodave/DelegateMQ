/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include <thread>

int main() 
{
    //NetworkMgr::Instance().Start();

    int cnt = 0; 
    while (1)
    {
        SensorData sensorData;
        sensorData.value = 123.4f;
        NetworkMgr::Instance().SendSensorData(sensorData);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (++cnt > 20)
            break;
    }

    //NetworkMgr::Instance().Stop();

    return 0;
}