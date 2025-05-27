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
    /// start message to ServerApp). Two types of async message sending illustrated. 
    /// @return `true` if all async commands sent successfully to ServerApp succeed. 
    /// `false` if the any send fails.
    bool Start()
    {
        bool sendCommandMsgSuccess = false;
        bool sendCommandMsgComplete = false;
        std::mutex mtx;
        std::condition_variable cv;

        // Callback to capture NetworkMgr::SendCommandMsg() success or error.
        ErrorCallback errorCb = [&sendCommandMsgSuccess, &sendCommandMsgComplete, &cv, &mtx](dmq::DelegateRemoteId id, dmq::DelegateError err, dmq::DelegateErrorAux aux) {
            // SendCommandMsg() ID?
            if (id == ids::COMMAND_MSG_ID) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    sendCommandMsgComplete = true;
                    if (err == dmq::DelegateError::SUCCESS)  // Send success? 
                        sendCommandMsgSuccess = true;
                }
                cv.notify_one();
            }
        };

        // Register for callback 
        NetworkMgr::ErrorCb += MakeDelegate(errorCb);

        CommandMsg command;
        command.action = CommandMsg::Action::START;
        command.pollTime = 250;

        // Async send start remote data collection message (non-blocking). Send success/failure
        // captured by errorCb callback lambda. 
        NetworkMgr::Instance().SendCommandMsg(command);

        // Async send actuator message (non-blocking). Send success/failure captured
        // using std::future. 
        ActuatorMsg msg;
        msg.actuatorId = 1;
        std::future<bool> futureRetVal = NetworkMgr::Instance().SendActuatorMsgFuture(msg);

        // Wait for async send command callback to be triggered
        std::unique_lock<std::mutex> lock(mtx);
        while (!sendCommandMsgComplete)
            cv.wait(lock);

        // Send complete; unregister from callback
        NetworkMgr::ErrorCb -= MakeDelegate(errorCb);

        // Start local data collection
        m_pollTimer.Expired = MakeDelegate(this, &ClientApp::PollData, m_thread);
        m_pollTimer.Start(std::chrono::milliseconds(500));

        // Start actuator updates
        m_actuatorTimer.Expired = MakeDelegate(this, &ClientApp::ActuatorUpdate, m_thread);
        m_actuatorTimer.Start(std::chrono::milliseconds(1000), true);

        // Wait here for the SendActuatorMsgFuture() return value
        bool sendActuatorMsgSuccess = futureRetVal.get();

        // Return true if both async messages succeed
        return (sendCommandMsgSuccess && sendActuatorMsgSuccess);
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

    // Update the local and remote actuator positions. Blocking call that only 
    // returns if all actuators updates succeed or a failure occurs.
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
        if (error != dmq::DelegateError::SUCCESS && id == ids::COMMAND_MSG_ID)
            std::cout << "ClientApp Error: " << id << " " << (int)error << " " << aux << std::endl;
    }

    void SendStatusHandler(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)
    {
        if (status != TransportMonitor::Status::SUCCESS && id == ids::COMMAND_MSG_ID)
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