#ifndef CLIENT_APP_H
#define CLIENT_APP_H

#include "DelegateMQ.h"
#include "Actuator.h"
#include "Sensor.h"
#include "DataMsg.h"

// ClientApp reads data locally and sends to DataMgr for storage. 
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
        CommandMsg command;
        command.action = CommandMsg::Action::START;
        command.pollTime = 250;
        NetworkMgr::Instance().SendCommandMsg(command);

        // Start local data collection
        m_pollTimer.Expired = MakeDelegate(this, &ClientApp::PollData, m_thread);
        m_pollTimer.Start(std::chrono::milliseconds(500));
    }

    void Stop()
    {
        CommandMsg command;
        command.action = CommandMsg::Action::STOP;
        NetworkMgr::Instance().SendCommandMsg(command);

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

        NetworkMgr::ErrorCb += MakeDelegate(this, &ClientApp::ErrorHandler, m_thread);
    }

    ~ClientApp()
    {
        m_pollTimer.Stop();
        m_thread.ExitThread();
    }

    void PollData()
    {
        // Collect sensor and actuator data
        DataMsg dataMsg;
        dataMsg.actuators.push_back(m_actuator3.GetState());
        dataMsg.actuators.push_back(m_actuator4.GetState());
        dataMsg.sensors.push_back(m_sensor3.GetSensorData());
        dataMsg.sensors.push_back(m_sensor4.GetSensorData());

        // Set data collected locally
        DataMgr::Instance().SetDataMsg(dataMsg);
    }

    void ErrorHandler(DelegateMQ::DelegateRemoteId id, DelegateMQ::DelegateError error, DelegateMQ::DelegateErrorAux aux)
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