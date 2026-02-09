/**
 * @file network_tests.cpp
 * @brief STM32 Server Application for Network Integration Tests.
 * @see https://github.com/endurodave/DelegateMQ
 * @author David Lafreniere, 2026.
 *
 * @details
 * This file implements the "ServerApp" logic for the DelegateMQ STM32 demo. 
 * It acts as the remote endpoint that communicates with the PC-based ClientApp.
 *
 * **Key Responsibilities:**
 * 1. **Command Handling:** Receives START/STOP commands from the Client to begin/end 
 * telemetry streaming.
 * 2. **Actuator Control:** Listens for `ActuatorMsg` from the Client to toggle simulated 
 * hardware actuators (and local LEDs).
 * 3. **Telemetry Streaming:** Uses a FreeRTOS software timer to periodically poll 
 * local sensors and actuators, packaging the data into `DataMsg` packets sent 
 * back to the Client.
 * 4. **Alarm Simulation:** Periodically generates simulated `AlarmMsg` events to test 
 * asynchronous alarm propagation to the Client.
 *
 * **Architecture:**
 * - **Singleton:** The `ServerApp` class is a singleton that manages its own FreeRTOS thread.
 * - **Asynchronous:** Uses `DelegateMQ` signals and slots to handle incoming network 
 * packets on the application thread, preventing network thread starvation.
 */

#include "DelegateMQ.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "../../common/NetworkMgr.h"
#include "../../common/AlarmMgr.h"
#include <stdio.h>

// External reference to the UART handle
extern UART_HandleTypeDef huart6;

using namespace dmq;

// ============================================================================
// SERVER APP (Ported from ServerApp.h)
// ============================================================================
class ServerApp
{
public:
    static ServerApp& Instance()
    {
        static ServerApp instance;
        return instance;
    }

    // Receive and handle client command message
    void CommandMsgRecv(CommandMsg& command)
    {
        if (command.action == CommandMsg::Action::START)
        {
             // Connect and store handle
            m_pollTimerConn = m_pollTimer.OnExpired->Connect(MakeDelegate(this, &ServerApp::PollData, m_thread));
            m_pollTimer.Start(std::chrono::milliseconds(command.pollTime));
            BSP_LED_On(LED4); // Green LED ON = Polling

            printf("[ServerApp] Starting Poll Timer (%lu ms)\n", command.pollTime);
        }
        else if (command.action == CommandMsg::Action::STOP)
        {
            m_pollTimer.Stop();
            m_pollTimerConn.Disconnect();
            BSP_LED_Off(LED4); // Green LED OFF

            printf("[ServerApp] Stopping Poll Timer\n");
        }
    }

    // Receive and handle client actuator message
    void ActuatorMsgRecv(ActuatorMsg& command)
    {
        printf("[ServerApp] Actuator Cmd: ID=%d Pos=%d\n", command.actuatorId, command.actuatorPosition);

        if (command.actuatorId == 1)
            m_actuator1.SetPosition(command.actuatorPosition);
        else if (command.actuatorId == 2)
            m_actuator2.SetPosition(command.actuatorPosition);
        else
            printf("[ServerApp] Unknown actuator ID\n");
    }

private:
    ServerApp() :
        m_thread("ServerApp"),
        m_actuator1(1),
        m_actuator2(2),
        m_sensor1(1),
        m_sensor2(2)
    {
        // 1. Create the App Thread
        m_thread.CreateThread();

        // 2. Register for incoming client commands via NetworkMgr Signals
        // Note: NetworkMgr must be initialized before this runs
        m_onCommandConn = NetworkMgr::OnCommand->Connect(MakeDelegate(this, &ServerApp::CommandMsgRecv, m_thread));
        m_onActuatorConn = NetworkMgr::OnActuator->Connect(MakeDelegate(this, &ServerApp::ActuatorMsgRecv, m_thread));
        m_onNetworkErrorConn = NetworkMgr::OnNetworkError->Connect(MakeDelegate(this, &ServerApp::ErrorHandler, m_thread));
        m_onSendStatusConn = NetworkMgr::OnSendStatus->Connect(MakeDelegate(this, &ServerApp::SendStatusHandler, m_thread));
    }

    ~ServerApp()
    {
        m_thread.ExitThread();
    }

    void PollData()
    {
        static int errCnt = 0;

        // Periodically simulate an alarm
        if (errCnt++ % 10 == 0)
        {
            printf("[ServerApp] Generating Simulated Alarm...\n");
            AlarmMsg msg;
            msg.alarm = AlarmMsg::Alarm::ACTUATOR_ERROR;
            msg.source = AlarmMsg::Source::SERVER;
            AlarmNote note;
            note.note = "Server simulated alarm!";

            AlarmMgr::Instance().SetAlarm(msg, note);
        }

        // Collect sensor and actuator data
        DataMsg dataMsg;
        dataMsg.actuators.push_back(m_actuator1.GetState());
        dataMsg.actuators.push_back(m_actuator2.GetState());
        dataMsg.sensors.push_back(m_sensor1.GetSensorData());
        dataMsg.sensors.push_back(m_sensor2.GetSensorData());

        // Send data to remote client
        // printf("[ServerApp] Sending Sensor Data...\n"); // Optional verbose logging
        NetworkMgr::Instance().SendDataMsg(dataMsg);

        BSP_LED_Toggle(LED6); // Blue Toggle = Data Sent
    }

    void ErrorHandler(dmq::DelegateRemoteId id, dmq::DelegateError error, dmq::DelegateErrorAux aux)
    {
        if (error != dmq::DelegateError::SUCCESS)
            printf("[ServerApp] Error: ID=%d Err=%d Aux=%d\n", id, (int)error, aux);
    }

    void SendStatusHandler(dmq::DelegateRemoteId id, uint16_t seqNum, TransportMonitor::Status status)
    {
        if (status != TransportMonitor::Status::SUCCESS)
            printf("[ServerApp] Timeout/Fail: ID=%d Seq=%d\n", id, seqNum);
    }

    Thread m_thread;
    Timer m_pollTimer;

    // RAII Connections
    dmq::ScopedConnection m_pollTimerConn;
    dmq::ScopedConnection m_onCommandConn;
    dmq::ScopedConnection m_onActuatorConn;
    dmq::ScopedConnection m_onNetworkErrorConn;
    dmq::ScopedConnection m_onSendStatusConn;

    Actuator m_actuator1;
    Actuator m_actuator2;
    Sensor m_sensor1;
    Sensor m_sensor2;
};

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
void StartNetworkTests() {
    // Disable stdout buffering so logs appear immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n=== STARTING STM32 SERVER APP ===\n");

    // 1. Initialize Network Engine (Create Transport/resources, but DON'T start thread yet)
    printf("[Init] NetworkMgr Create...\n");
    int err = NetworkMgr::Instance().Create();
    if (err) {
        printf("[FAIL] NetworkMgr Create Error: %d\n", err);
        return;
    }

    // 2. Start Server Application (Registers Delegates!)
    // MOVED UP: Must happen BEFORE NetworkMgr::Start()
    printf("[Init] ServerApp Instance...\n");
    ServerApp::Instance();

    // 3. Start Network Thread (Now safe to receive data)
    printf("[Init] NetworkMgr Start...\n");
    NetworkMgr::Instance().Start();

    printf("[Ready] Server is running. Waiting for Client commands...\n");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
