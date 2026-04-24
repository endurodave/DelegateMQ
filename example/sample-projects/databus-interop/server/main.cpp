/// @file main.cpp
/// @see https://github.com/DelegateMQ/DelegateMQ
/// David Lafreniere, 2025.
///
/// DataBus Interop Server.
///
/// Publishes DataMsg (actuator + sensor readings) over UDP and subscribes
/// to CommandMsg (polling rate adjustment) from any interop client.
///
/// Pairs with:
///   ../python-client/main.py     — Python client
///   ../csharp-client/Program.cs  — C# client
///
/// Transport:  UDP  (port 8000 PUB, port 8001 SUB)
/// Serializer: MessagePack

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include "extras/util/NetworkConnect.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>

static std::atomic<bool> g_running(true);
static void SignalHandler(int) { g_running = false; }

std::atomic<int> g_pollingRateMs{ 1000 };

int main(int argc, char* argv[])
{
    int duration = 0;
    if (argc > 1) duration = atoi(argv[1]);

    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "Starting DataBus Interop SERVER..." << std::endl;

#ifdef _WIN32
    dmq::util::NetworkContext winsock;
#endif

    // 1. Initialize transports
    // PUB on 8000: server sends DataMsg to clients
    dmq::transport::UdpTransport transportData;
    if (transportData.Create(dmq::transport::UdpTransport::Type::PUB, "127.0.0.1", 8000) != 0) {
        std::cerr << "Failed to create Data transport (port 8000)" << std::endl;
        return -1;
    }

    // SUB on 8001: server receives CommandMsg from clients
    dmq::transport::UdpTransport transportCmd;
    if (transportCmd.Create(dmq::transport::UdpTransport::Type::SUB, "127.0.0.1", 8001) != 0) {
        std::cerr << "Failed to create Command transport (port 8001)" << std::endl;
        return -1;
    }

    // 2. Reliability layer — tracks ACKs for outgoing DataMsg
    dmq::util::TransportMonitor monitor(std::chrono::seconds(1));
    dmq::util::RetryMonitor retry(transportData, monitor, 0);
    dmq::util::ReliableTransport reliableTransport(transportData, retry);

    // Cross-wire: SUB socket sends ACKs back via PUB socket
    transportData.SetTransportMonitor(&monitor);
    transportCmd.SetTransportMonitor(&monitor);
    transportCmd.SetSendTransport(&transportData);

    // 3. DataBus participant for outgoing DataMsg (via reliable transport)
    auto dataParticipant = std::make_shared<dmq::databus::Participant>(reliableTransport);
    dataParticipant->AddRemoteTopic(SystemTopic::DataMsg, SystemTopic::DataMsgId);
    dmq::databus::DataBus::AddParticipant(dataParticipant);

    // 4. DataBus participant for incoming CommandMsg
    auto commandParticipant = std::make_shared<dmq::databus::Participant>(transportCmd);
    dmq::serialization::msgpack::Serializer<void(CommandMsg)> commandSerializer;
    dmq::databus::DataBus::AddIncomingTopic<CommandMsg>(SystemTopic::CommandMsg, SystemTopic::CommandMsgId, *commandParticipant, commandSerializer);

    // 5. Register DataMsg serializer
    dmq::serialization::msgpack::Serializer<void(DataMsg)> dataSerializer;
    dmq::databus::DataBus::RegisterSerializer<DataMsg>(SystemTopic::DataMsg, dataSerializer);

    // 6. Subscribe to CommandMsg on local DataBus
    auto cmdConn = dmq::databus::DataBus::Subscribe<CommandMsg>(
        SystemTopic::CommandMsg, [](const CommandMsg& msg) {
            std::cout << "Received CommandMsg: pollingRateMs=" << msg.pollingRateMs << std::endl;
            g_pollingRateMs = msg.pollingRateMs;
        });

    // 7. Background network thread — polls participants and drives ACK timeouts
    std::atomic<bool> netRunning{ true };
    std::thread netThread([&]() {
        while (netRunning) {
            dataParticipant->ProcessIncoming();     // process incoming ACKs
            commandParticipant->ProcessIncoming();  // process incoming CommandMsg
            monitor.Process();                      // drive ACK/retry timeouts
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    if (duration > 0) {
        std::thread([duration]() {
            std::this_thread::sleep_for(std::chrono::seconds(duration));
            g_running = false;
        }).detach();
    } else {
        std::cout << "Press 'q' + Enter (or Ctrl+C) to quit." << std::endl;
        std::thread([]() {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!line.empty() && (line[0] == 'q' || line[0] == 'Q')) {
                    g_running = false;
                    break;
                }
            }
            g_running = false;  // also exit on EOF (pipe closed / stdin redirected)
        }).detach();
    }

    // 8. Main loop — publish DataMsg at the current polling rate
    int iteration = 0;
    while (g_running) {
        DataMsg msg;

        Actuator act;
        act.id       = 1;
        act.position = 10.0f + static_cast<float>(iteration) * 0.1f;
        act.voltage  = 3.3f;
        msg.actuators.push_back(act);

        Sensor sen;
        sen.id       = 101;
        sen.supplyV  = 5.0f;
        sen.readingV = 2.5f + static_cast<float>(iteration) * 0.01f;
        msg.sensors.push_back(sen);

        std::cout << "Publishing DataMsg (rate=" << g_pollingRateMs
                  << "ms) iteration=" << iteration << std::endl;
        dmq::databus::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, msg);

        std::this_thread::sleep_for(std::chrono::milliseconds(g_pollingRateMs));
        iteration++;
    }

    std::cout << "\nShutting down..." << std::endl;
    netRunning = false;
    netThread.join();
    transportData.Close();
    transportCmd.Close();

    return 0;
}
