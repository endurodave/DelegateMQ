// main.cpp
// DataBus System Architecture Client (Subscriber) Example.
// 
// This sample demonstrates a distributed subscriber using dmq::DataBus over UDP.
// - Receives simulated sensor/actuator data (DataMsg) from the server.
// - Periodically sends commands (CommandMsg) to the server to toggle the polling rate.
// - Uses the DelegateMQ Reliability Layer (RetryMonitor/TransportMonitor) to detect connection loss.
// 
// NOTE: To monitor this application with the DelegateMQ Spy Console:
// 1. Build with -DDMQ_DATABUS_TOOLS=ON.
// 2. Start dmq-spy.exe before running this application.

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#ifdef DMQ_DATABUS_TOOLS
#include "SpyBridge.h"
#endif

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <sstream>
#include <csignal>

static std::atomic<bool> g_running(true);

static void SignalHandler(int) { g_running = false; }

int main() {
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "Starting System Architecture CLIENT (Subscriber)..." << std::endl;

#ifdef _WIN32
    // 1. Initialize Winsock (Windows only). NetworkContext defined in NetworkConnect.h.
    NetworkContext winsock;
#endif

#ifdef DMQ_DATABUS_TOOLS
    // Start Spy Bridge to export DataBus traffic to the Spy Console
    SpyBridge::Start("127.0.0.1", 9999);

    // Register stringifier so Spy Console can display the DataMsg content
    dmq::DataBus::RegisterStringifier<DataMsg>(SystemTopic::DataMsg, [](const DataMsg& msg) {
        std::ostringstream ss;
        ss << "Actuators: " << msg.actuators.size() << ", Sensors: " << msg.sensors.size();
        return ss.str();
    });

    dmq::DataBus::RegisterStringifier<CommandMsg>(SystemTopic::CommandMsg, [](const CommandMsg& msg) {
        return "Command: PollingRate=" + std::to_string(msg.pollingRateMs) + "ms";
    });
#endif

    // 2. Initialize Transports
    // Listen for Data from Server on localhost:8000
    UdpTransport transportData;
    if (transportData.Create(UdpTransport::Type::SUB, "127.0.0.1", 8000) != 0) {
        std::cerr << "Failed to create Data transport" << std::endl;
        return -1;
    }

    // Send Commands to Server on localhost:8001
    UdpTransport transportCmd;
    if (transportCmd.Create(UdpTransport::Type::PUB, "127.0.0.1", 8001) != 0) {
        std::cerr << "Failed to create Command transport" << std::endl;
        return -1;
    }

    // 3. Setup Reliability Layer (for outgoing commands)
    TransportMonitor monitor(std::chrono::seconds(1));
    RetryMonitor retry(transportCmd, monitor, 0); 
    ReliableTransport reliableTransport(transportCmd, retry);

    // Configure transports to work together for ACKs
    transportCmd.SetTransportMonitor(&monitor);
    transportData.SetTransportMonitor(&monitor);
    transportData.SetSendTransport(&transportCmd); // Allow SUB to send ACKs via PUB

    // 4. Setup Participant for Commands (Outgoing via reliable transport)
    auto commandParticipant = std::make_shared<dmq::Participant>(reliableTransport);
    commandParticipant->AddRemoteTopic(SystemTopic::CommandMsg, SystemTopic::CommandMsgId);
    dmq::DataBus::AddParticipant(commandParticipant);

    // 5. Setup Participant for Data (Incoming via standard transport)
    auto dataParticipant = std::make_shared<dmq::Participant>(transportData);
    Serializer<void(DataMsg)> dataSerializer;
    dataParticipant->RegisterHandler<DataMsg>(SystemTopic::DataMsgId, dataSerializer, [](DataMsg msg) {
        // Republish to local bus so local components can subscribe by name
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, std::move(msg));
    });

    // 6. Register Command Serializer
    Serializer<void(CommandMsg)> commandSerializer;
    dmq::DataBus::RegisterSerializer<CommandMsg>(SystemTopic::CommandMsg, commandSerializer);

    // 7. Monitor reliability signal to detect server response status
    auto statusConn = monitor.OnSendStatus.Connect(dmq::MakeDelegate([](dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
        if (status == TransportMonitor::Status::TIMEOUT) {
            std::cerr << "!!! ALERT: Server not responding to command (RemoteID: " << id << " Seq: " << seq << ")" << std::endl;
        } else {
            std::cout << "Command ACK received (Seq: " << seq << ")" << std::endl;
        }
    }));

    // 8. Local Subscription to DataBus
    auto conn = dmq::DataBus::Subscribe<DataMsg>(SystemTopic::DataMsg, [](DataMsg msg) {
        std::cout << "Client received DataMsg: " << msg.actuators.size() << " actuators, " << msg.sensors.size() << " sensors" << std::endl;
    });

    // 9. Background thread to process incoming network data and reliability timeouts
    std::atomic<bool> running{true};
    std::thread receiveThread([&]() {
        while (running) {
            commandParticipant->ProcessIncoming(); // Process ACKs for commands
            dataParticipant->ProcessIncoming();    // Process incoming DataMsg
            monitor.Process();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // 10. Main loop: Toggle polling rate every 5 seconds
    int rateToggle = 0;
    auto lastCommandTime = std::chrono::steady_clock::now();

    std::cout << "Client running. Sending speed commands every 5s... Press Ctrl+C to quit." << std::endl;

    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCommandTime).count() >= 5) {
            CommandMsg cmd;
            cmd.pollingRateMs = (rateToggle % 2 == 0) ? 500 : 1000;
            std::cout << "Client sending command: set rate to " << cmd.pollingRateMs << "ms" << std::endl;
            dmq::DataBus::Publish<CommandMsg>(SystemTopic::CommandMsg, cmd);

            rateToggle++;
            lastCommandTime = now;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nShutting down..." << std::endl;
    running = false;
    receiveThread.join();

    transportData.Close();
    transportCmd.Close();

#ifdef DMQ_DATABUS_TOOLS
    SpyBridge::Stop();
#endif

    return 0;
}



