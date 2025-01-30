/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ and MsgPack with remote delegates examples. 

#include "NetworkMgr.h"
#include "DataMgr.h"
#include <thread>

// Client sensors
static Actuator actuator3(3);
static Actuator actuator4(4);
static Sensor sensor3(3);
static Sensor sensor4(4);

static Thread pollThread("PollThread");
static Thread receiveThread("ReceiveThread");

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

void PollData()
{
    int cnt = 0;
    while (cnt++ < 30)
    {
        // Collect sensor and actuator data
        DataPackage dataPackage;
        dataPackage.actuators.push_back(actuator3.GetState());
        dataPackage.actuators.push_back(actuator4.GetState());
        dataPackage.sensors.push_back(sensor3.GetSensorData());
        dataPackage.sensors.push_back(sensor4.GetSensorData());

        // Set data collected locally
        DataMgr::Instance().SetDataPackage(dataPackage);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    NetworkMgr::Instance().Start();
    DataMgr::Instance();

    pollThread.CreateThread();
    receiveThread.CreateThread();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Register to receive local and remote data updates
    DataMgr::DataPackageRecv += MakeDelegate(&DataPackageRecv, receiveThread);

    // TODO: Send message to server to start data collection

    // Generate local test data on worker thread
    MakeDelegate(&PollData, pollThread, WAIT_INFINITE).AsyncInvoke();

    NetworkMgr::Instance().Stop();
    pollThread.ExitThread();
    receiveThread.ExitThread();
    return 0;
}