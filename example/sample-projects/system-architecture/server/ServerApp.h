#ifndef SERVER_APP_H
#define SERVER_APP_H

#include "DelegateMQ.h"
#include "Actuator.h"
#include "Sensor.h"

// Server application subsystem reads data and sends to a client
class ServerApp
{
public:
    static ServerApp& Instance()
    {
        static ServerApp instance;
        return instance;
    }

    void CommandRecv(Command& command)
    {
        if (command.action == Command::Action::START)
        {
            m_pollTimer.Expired = MakeDelegate(this, &ServerApp::PollData, m_thread);
            m_pollTimer.Start(std::chrono::milliseconds(command.pollTime));
        }
        else if (command.action == Command::Action::STOP)
        {
            m_pollTimer.Stop();
        }
        else
        {
            // Do something if unknown command
        }
    }

private:
    ServerApp() :
        m_thread("ServerAppThread"),
        m_actuator1(1),
        m_actuator2(2),
        m_sensor1(1),
        m_sensor2(2)
    {
        m_thread.CreateThread();

        // Register for incoming client commands
        NetworkMgr::CommandRecv += MakeDelegate(this, &ServerApp::CommandRecv, m_thread);
        NetworkMgr::Error += MakeDelegate(this, &ServerApp::ErrorHandler, m_thread);
    }

    ~ServerApp()
    {
        m_thread.ExitThread();
    }

    void PollData()
    {
        // Collect sensor and actuator data
        DataPackage dataPackage;
        dataPackage.actuators.push_back(m_actuator1.GetState());
        dataPackage.actuators.push_back(m_actuator2.GetState());
        dataPackage.sensors.push_back(m_sensor1.GetSensorData());
        dataPackage.sensors.push_back(m_sensor2.GetSensorData());

        // Send data to remote client
        NetworkMgr::Instance().SendDataPackage(dataPackage);
    }

    void ErrorHandler(DelegateRemoteId id, DelegateError error, DelegateErrorAux aux)
    {
        std::cout << "ServerApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    Thread m_thread;

    Timer m_pollTimer;

    Actuator m_actuator1;
    Actuator m_actuator2;
    Sensor m_sensor1;
    Sensor m_sensor2;
};

#endif