#ifndef CLIENT_APP_H
#define CLIENT_APP_H

#include "DelegateMQ.h"
#include "Actuator.h"
#include "Sensor.h"
#include "DataPackage.h"

// Client application subsystem reads data and sends to DataMgr
class ClientApp
{
public:
    static ClientApp& Instance()
    {
        static ClientApp instance;
        return instance;
    }

    void Start()
    {
        // Send message to start remote data collection
        Command command;
        command.action = Command::Action::START;
        command.pollTime = 250;
        NetworkMgr::Instance().SendCommand(command);

        // Start local data collection
        m_pollTimer.Expired = MakeDelegate(this, &ClientApp::PollData, m_thread);
        m_pollTimer.Start(std::chrono::milliseconds(500));
    }

    void Stop()
    {
        Command command;
        command.action = Command::Action::STOP;
        NetworkMgr::Instance().SendCommand(command);

        m_pollTimer.Stop();
    }

private:
    ClientApp() :
        m_thread("ClientAppThread"),
        m_actuator3(3),
        m_actuator4(4),
        m_sensor3(3),
        m_sensor4(4)
    {
        m_thread.CreateThread();

        NetworkMgr::Error += MakeDelegate(this, &ClientApp::ErrorHandler, m_thread);
    }

    ~ClientApp()
    {
        m_pollTimer.Stop();
        m_thread.ExitThread();
    }

    void PollData()
    {
        // Collect sensor and actuator data
        DataPackage dataPackage;
        dataPackage.actuators.push_back(m_actuator3.GetState());
        dataPackage.actuators.push_back(m_actuator4.GetState());
        dataPackage.sensors.push_back(m_sensor3.GetSensorData());
        dataPackage.sensors.push_back(m_sensor4.GetSensorData());

        // Set data collected locally
        DataMgr::Instance().SetDataPackage(dataPackage);
    }

    void ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
    {
        std::cout << "ClientApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    Thread m_thread;

    Timer m_pollTimer;

    Actuator m_actuator3;
    Actuator m_actuator4;
    Sensor m_sensor3;
    Sensor m_sensor4;
};

#endif