// main.cpp
// DataBus System Architecture Server (Publisher) Example.
// 
// This sample demonstrates a distributed publisher using dmq::DataBus over UDP.
// - Publishes simulated sensor/actuator data (DataMsg) to the client.
// - Subscribes to incoming commands (CommandMsg) to dynamically adjust the polling rate.
// - Uses the DelegateMQ Reliability Layer (RetryMonitor/TransportMonitor) to track delivery.
// 
// NOTE: To monitor this application with the DelegateMQ Spy Console:
// 1. Build with -DDMQ_DATABUS_SPY=ON.
// 2. Start dmq-spy.exe before running this application.

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"

#ifdef DMQ_DATABUS_SPY
#include "SpyBridge.h"
#endif

#include <iostream>
#include <vector>
#include <thread>
#include <sstream>
#include <atomic>

// Global polling rate modified by CommandMsg
std::atomic<int> g_pollingRateMs{ 1000 };

int main() {
    std::cout << "Starting System Architecture SERVER (Publisher)..." << std::endl;

#ifdef _WIN32
    // 1. Initialize Winsock (Windows only). WinsockContext defined in WinsockConnect.h.
    WinsockContext winsock;
#endif

#ifdef DMQ_DATABUS_SPY
    // Start Spy Bridge to export DataBus traffic to the Spy Console
    SpyBridge::Start("127.0.0.1", 9999);

    // Register stringifier so Spy Console can display the DataMsg content
    dmq::DataBus::RegisterStringifier<DataMsg>(SystemTopic::DataMsg, [](const DataMsg& msg) {
        std::ostringstream ss;
        ss << "Actuators: " << msg.actuators.size() << ", Sensors: " << msg.sensors.size();
        return ss.str();
    });

    dmq::DataBus::RegisterStringifier<CommandMsg>(SystemTopic::CommandMsg, [](const CommandMsg& msg) {
        return "Set Polling Rate: " + std::to_string(msg.pollingRateMs) + "ms";
    });
#endif

    // 2. Initialize Transports
    // Send to Client on localhost:8000
    UdpTransport transportData;
    if (transportData.Create(UdpTransport::Type::PUB, "127.0.0.1", 8000) != 0) {
        std::cerr << "Failed to create Data transport" << std::endl;
        return -1;
    }

    // Listen for Commands on localhost:8001
    UdpTransport transportCmd;
    if (transportCmd.Create(UdpTransport::Type::SUB, "127.0.0.1", 8001) != 0) {
        std::cerr << "Failed to create Command transport" << std::endl;
        return -1;
    }

    // 3. Setup Reliability Layer
    // Use a 1s timeout and 0 retries for immediate feedback
    TransportMonitor monitor(std::chrono::seconds(1));
    RetryMonitor retry(transportData, monitor, 0);
    ReliableTransport reliableTransport(transportData, retry);

    // Configure transports to work together for ACKs
    transportData.SetTransportMonitor(&monitor);
    transportCmd.SetTransportMonitor(&monitor);
    transportCmd.SetSendTransport(&transportData); // Allow SUB to send ACKs via PUB

    // 4. Setup Participant for Data (Outgoing via reliable transport)
    auto dataParticipant = std::make_shared<dmq::Participant>(reliableTransport);
    dataParticipant->AddRemoteTopic(SystemTopic::DataMsg, SystemTopic::DataMsgId);
    dmq::DataBus::AddParticipant(dataParticipant);

    // 5. Setup Participant for Commands (Incoming via standard transport)
    auto commandParticipant = std::make_shared<dmq::Participant>(transportCmd);
    Serializer<void(CommandMsg)> commandSerializer;
    commandParticipant->RegisterHandler<CommandMsg>(SystemTopic::CommandMsgId, commandSerializer, [](CommandMsg msg) {
        // Republish to local bus so local components can subscribe and monitor sees it
        dmq::DataBus::Publish<CommandMsg>(SystemTopic::CommandMsg, std::move(msg));
    });

    // 6. Register Data Serializer
    Serializer<void(DataMsg)> dataSerializer;
    dmq::DataBus::RegisterSerializer<DataMsg>(SystemTopic::DataMsg, dataSerializer);

    // 7. Handle incoming CommandMsg (now arrives via local bus)
    auto commandConn = dmq::DataBus::Subscribe<CommandMsg>(SystemTopic::CommandMsg, [](const CommandMsg& msg) {
        std::cout << "Server received CommandMsg: Changing polling rate to " << msg.pollingRateMs << "ms" << std::endl;
        g_pollingRateMs = msg.pollingRateMs;
    });

    // 8. Monitor reliability signal to detect client response status
    using StatusSignal = std::function<void(dmq::DelegateRemoteId, uint16_t, TransportMonitor::Status)>;
    auto statusConn = monitor.OnSendStatus.Connect(dmq::MakeDelegate(StatusSignal([](dmq::DelegateRemoteId id, uint16_t seq, TransportMonitor::Status status) {
        if (status == TransportMonitor::Status::TIMEOUT) {
            std::cerr << "!!! ALERT: Client not acknowledging data (RemoteID: " << id << " Seq: " << seq << ")" << std::endl;
        }
    })));

    // 9. Background thread to process reliability timeouts and incoming network data
    std::atomic<bool> running{ true };
    std::thread netThread([&]() {
        while (running) {
            // Process incoming ACKs on data transport
            dataParticipant->ProcessIncoming();
            // Process incoming Commands on command transport
            commandParticipant->ProcessIncoming();
            // Process reliability timeouts
            monitor.Process();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // 10. Main loop: Publish data at dynamic rate
    int iteration = 0;
    while (iteration < 120) {
        DataMsg msg;
        Actuator act; act.id = 1; act.position = 10.0f + (float)iteration;
        msg.actuators.push_back(act);

        Sensor sen; sen.id = 101; sen.readingV = 2.5f + ((float)iteration * 0.1f);
        msg.sensors.push_back(sen);

        std::cout << "Publishing DataMsg (Rate: " << g_pollingRateMs << "ms), iteration: " << iteration << std::endl;
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, msg);

        std::this_thread::sleep_for(std::chrono::milliseconds(g_pollingRateMs));
        iteration++;
    }

    running = false;
    netThread.join();
    transportData.Close();
    transportCmd.Close();

#ifdef DMQ_DATABUS_SPY
    SpyBridge::Stop();
#endif

    return 0;
}
