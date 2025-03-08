#ifndef CLIENT_APP_H
#define CLIENT_APP_H

#include "DelegateMQ.h"
#include "Actuator.h"
#include "Sensor.h"
#include "DataMsg.h"

typedef std::function<void(dmq::DelegateRemoteId, dmq::DelegateError, dmq::DelegateErrorAux)> ErrorCallback;

// ClientApp reads data locally and sends to DataMgr for storage. 
class ClientApp
{
public:
    static ClientApp& Instance()
    {
        static ClientApp instance;
        return instance;
    }

    /// Start data collection locally (polling with a timer) and remotely (sending a 
    /// start message to ServerApp). 
    /// @return `true` if data collection command sent successfully to ServerApp. 
    /// `false` if the send fails.
    bool Start()
    {
        bool success = false;
        std::mutex mtx;
        std::condition_variable cv;

        // Callback to capture NetworkMgr::SendCommandMsg() success or error
        ErrorCallback errorCb = [&success, &cv](dmq::DelegateRemoteId id, dmq::DelegateError err, dmq::DelegateErrorAux aux) {
            // SendCommandMsg() ID?
            if (id == COMMAND_MSG_ID) {
                // Send success?
                if (err == dmq::DelegateError::SUCCESS)
                    success = true;
                cv.notify_one();
            }
        };

        // Register for callback 
        NetworkMgr::ErrorCb += MakeDelegate(errorCb);

        CommandMsg command;
        command.action = CommandMsg::Action::START;
        command.pollTime = 250;

        // Async send message to start remote data collection
        NetworkMgr::Instance().SendCommandMsg(command);

        // Wait for async send callback to be triggered
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock);

        // Send complete; unregister from callback
        NetworkMgr::ErrorCb -= MakeDelegate(errorCb);

        // Start local data collection
        m_pollTimer.Expired = MakeDelegate(this, &ClientApp::PollData, m_thread);
        m_pollTimer.Start(std::chrono::milliseconds(500));

        // Start actuator updates
        m_actuatorTimer.Expired = MakeDelegate(this, &ClientApp::ActuatorUpdate, m_thread);
        m_actuatorTimer.Start(std::chrono::milliseconds(1000), true);

        return success;
    }

    void Stop()
    {
        CommandMsg command;
        command.action = CommandMsg::Action::STOP;
        NetworkMgr::Instance().SendCommandMsg(command);

        m_pollTimer.Stop();
        m_pollTimer.Expired = nullptr;
        m_actuatorTimer.Stop();
        m_actuatorTimer.Expired = nullptr;
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
        NetworkMgr::SendStatusCb += MakeDelegate(this, &ClientApp::SendStatusHandler, m_thread);
    }

    ~ClientApp() 
    {
        NetworkMgr::ErrorCb -= MakeDelegate(this, &ClientApp::ErrorHandler, m_thread);
        NetworkMgr::SendStatusCb -= MakeDelegate(this, &ClientApp::SendStatusHandler, m_thread);
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

    void ActuatorUpdate()
    {
        static int cnt = 0;
        bool position;
        if (++cnt % 2)
            position = false;   // actuators off
        else
            position = true;    // actuators on

        // Set local acuator positions
        m_actuator3.SetPosition(position);
        m_actuator4.SetPosition(position);

        // Set remote actuator positions
        ActuatorMsg msg;
        msg.actuatorId = 1;
        msg.actuatorPosition = position;

        // SendActuatorMsg calls block until remote receives and processes the message
        bool success1 = NetworkMgr::Instance().SendActuatorMsgWait(msg);
        msg.actuatorId = 2;
        bool success2 = NetworkMgr::Instance().SendActuatorMsgWait(msg);

        if (!success1 || !success2)
            std::cout << "Remote actuator failed!" << std::endl;

        // Explicitly start timer for next time. Prevent excessive timer messages in the thread 
        // queue if "Wait" calls above block a long time due to communications down.
        m_actuatorTimer.Start(std::chrono::milliseconds(1000), true);
    }

    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        if (error != dmq::DelegateError::SUCCESS)
            std::cout << "ClientApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    void SendStatusHandler(uint16_t seqNum, dmq::DelegateRemoteId id, TransportMonitor::Status status)
    {
        if (status != TransportMonitor::Status::SUCCESS)
            std::cout << "ClientApp Timeout: " << id << " " << seqNum << std::endl;
    }

    Thread m_thread;

    Timer m_pollTimer;
    Timer m_actuatorTimer;

    Actuator m_actuator3;
    Actuator m_actuator4;
    Sensor m_sensor3;
    Sensor m_sensor4;
};

#endif