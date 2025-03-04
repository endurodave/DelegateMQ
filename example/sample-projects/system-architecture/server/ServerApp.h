#ifndef SERVER_APP_H
#define SERVER_APP_H

#include "DelegateMQ.h"
#include "Actuator.h"
#include "Sensor.h"

// Server application subsystem reads data commands and sends to a 
// remote client using delegates.
class ServerApp
{
public:
    static ServerApp& Instance()
    {
        static ServerApp instance;
        return instance;
    }

    // Receive and handle client commands
    void CommandMsgRecv(CommandMsg& command)
    {
        if (command.action == CommandMsg::Action::START)
        {
            m_pollTimer.Expired = MakeDelegate(this, &ServerApp::PollData, m_thread);
            m_pollTimer.Start(std::chrono::milliseconds(command.pollTime));
        }
        else if (command.action == CommandMsg::Action::STOP)
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
        NetworkMgr::CommandMsgCb += MakeDelegate(this, &ServerApp::CommandMsgRecv, m_thread);
        NetworkMgr::ErrorCb += MakeDelegate(this, &ServerApp::ErrorHandler, m_thread);
        NetworkMgr::TimeoutCb += MakeDelegate(this, &ServerApp::TimeoutHandler, m_thread);
    }

    ~ServerApp()
    {
        m_thread.ExitThread();
    }

    void PollData()
    {
        static int errCnt = 0;

        // Periodically send alarm to client
        if (errCnt++ % 10 == 0)
        {
            AlarmMsg msg;
            msg.alarm = AlarmMsg::Alarm::ACTUATOR_ERROR;
            msg.source = AlarmMsg::Source::SERVER;
            AlarmNote note;
            note.note = "Server alarm!";

            AlarmMgr::Instance().SetAlarm(msg, note);
        }

        // Collect sensor and actuator data
        DataMsg dataMsg;
        dataMsg.actuators.push_back(m_actuator1.GetState());
        dataMsg.actuators.push_back(m_actuator2.GetState());
        dataMsg.sensors.push_back(m_sensor1.GetSensorData());
        dataMsg.sensors.push_back(m_sensor2.GetSensorData());

        // Send data to remote client
        NetworkMgr::Instance().SendDataMsg(dataMsg);
    }

    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        if (error != dmq::DelegateError::SUCCESS)
            std::cout << "ServerApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    void TimeoutHandler(uint16_t seqNum, dmq::DelegateRemoteId id)
    {
        std::cout << "ServerApp Timeout: " << id << " " << seqNum << std::endl;
    }

    Thread m_thread;

    Timer m_pollTimer;

    Actuator m_actuator1;
    Actuator m_actuator2;
    Sensor m_sensor1;
    Sensor m_sensor2;
};

#endif