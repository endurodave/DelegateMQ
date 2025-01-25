/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include <thread>


void RecvSensorData(SensorData& data)
{
    std::cout << "Data: " << data.value << std::endl;
}

int main()
{
    NetworkMgr::Instance().Start();
    NetworkMgr::SensorDataRecv += MakeDelegate(&RecvSensorData);

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(20));

    NetworkMgr::Instance().Stop();

    return 0;
}