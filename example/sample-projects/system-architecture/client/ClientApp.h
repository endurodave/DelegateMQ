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

        return success;
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
        NetworkMgr::TimeoutCb += MakeDelegate(this, &ClientApp::TimeoutHandler, m_thread);
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

    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        if (error != dmq::DelegateError::SUCCESS)
            std::cout << "ClientApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    void TimeoutHandler(uint16_t seqNum, dmq::DelegateRemoteId id)
    {
        std::cout << "ClientApp Timeout: " << id << " " << seqNum << std::endl;
    }

    Thread m_thread;

    Timer m_pollTimer;

    Actuator m_actuator3;
    Actuator m_actuator4;
    Sensor m_sensor3;
    Sensor m_sensor4;
};

#endif