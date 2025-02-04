/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and msgpack for C++ with remote delegates examples. 
/// https://github.com/msgpack/msgpack-c/tree/cpp_master
/// https://github.com/zeromq
/// 
/// ClientApp and ServerApp communicate using ZeroMQ transport, msgpack 
/// serialization, and DelegateMQ dispatching.
/// 
/// The ServerApp collects data locally and remotely from sensors and 
/// actuators. Start both applications to run sample. 

#include "NetworkMgr.h"
#include "DataMgr.h"
#include "ClientApp.h"
#include <thread>

static Thread receiveThread("ReceiveThread");

// Receive all local and remote data callback
void DataMsgRecv(DataMsg& data)
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

    NetworkMgr::Instance().Create();
    NetworkMgr::Instance().Start();
    DataMgr::Instance();
    ClientApp::Instance();

    receiveThread.CreateThread();

    // Register to receive local and remote data updates on receiveThread
    DataMgr::DataMsgCb += MakeDelegate(&DataMsgRecv, receiveThread);

    // Start all data collection
    ClientApp::Instance().Start();

    // Let client and server communicate
    std::this_thread::sleep_for(std::chrono::seconds(30));

    ClientApp::Instance().Stop();

    NetworkMgr::Instance().Stop();
    receiveThread.ExitThread();
    return 0;
}