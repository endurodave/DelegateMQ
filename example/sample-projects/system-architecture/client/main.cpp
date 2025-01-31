/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include "DataMgr.h"
#include "ClientApp.h"
#include <thread>

static Thread receiveThread("ReceiveThread");

// Receive all local and remote data callback
void DataPackageRecv(DataPackage& data)
{
    std::cout << "Actuators: " << data.actuators.size() << std::endl;
    for (const auto& actuator : data.actuators) {
        std::cout << "Actuator ID: " << actuator.id
            << ", Position: " << actuator.position
            << ", Voltage: " << actuator.voltage << std::endl;
    }

    std::cout << "Sensors: " << data.sensors.size() << std::endl;
    for (const auto& sensor : data.sensors) {
        std::cout << "Sensor ID: " << sensor.id
            << ", Supply Voltage: " << sensor.supplyV
            << ", Reading Voltage: " << sensor.readingV << std::endl;
    }
}

int main()
{
    std::cout << "Client start!" << std::endl;

    NetworkMgr::Instance().Start();
    DataMgr::Instance();
    ClientApp::Instance();

    receiveThread.CreateThread();

    // Register to receive local and remote data updates on receiveThread
    DataMgr::DataPackageRecv += MakeDelegate(&DataPackageRecv, receiveThread);

    // Start all data collection
    ClientApp::Instance().Start();

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(30));

    ClientApp::Instance().Stop();

    NetworkMgr::Instance().Stop();
    receiveThread.ExitThread();
    return 0;
}