// main.cpp
// DataBus Multicast Client (Subscriber) Example.
// 
// This sample demonstrates a distributed subscriber using dmq::DataBus 
// over UDP Multicast. Any number of clients can join the group to 
// receive the same data stream.

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <csignal>

#if defined(_WIN32) || defined(_WIN64)
#include "predef/transport/win32-udp/MulticastTransport.h"
#else
#include "predef/transport/linux-udp/MulticastTransport.h"
#endif

#ifdef DMQ_DATABUS_TOOLS
#include "SpyBridge.h"
#include <sstream>
#endif

static std::atomic<bool> g_running(true);

static void SignalHandler(int) { g_running = false; }

int main(int argc, char* argv[]) {
    int duration = 0;
    if (argc > 1) duration = atoi(argv[1]);

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "Starting DataBus Multicast CLIENT (Subscriber)..." << std::endl;

    NetworkContext winsock;
    // Auto-detect physical IP for multicast interface
    std::string localIP = NetworkContext::GetLocalAddress();

    std::cout << "Local Interface: " << localIP << std::endl;

#ifdef DMQ_DATABUS_TOOLS
    // Start Spy Bridge to export DataBus traffic to the Spy Console via Multicast
    SpyBridge::StartMulticast("239.1.1.1", 9999, localIP);

    // Register stringifier so Spy Console can display the DataMsg content
    dmq::DataBus::RegisterStringifier<DataMsg>(SystemTopic::DataMsg, [](const DataMsg& msg) {
        std::ostringstream ss;
        ss << "Multicast Data: " << msg.actuators.size() << " actuators, " << msg.sensors.size() << " sensors";
        return ss.str();
    });
#endif

    // 1. Initialize Multicast Transport (Group: 239.1.1.1, Port: 8000)
    MulticastTransport transport;
    if (transport.Create(MulticastTransport::Type::SUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return -1;
    }

    // 2. Setup Remote Participant
    auto group = std::make_shared<dmq::Participant>(transport);
    
    // 3. Register Serializer and Handler for incoming data
    static Serializer<void(DataMsg)> serializer;
    group->RegisterHandler<DataMsg>(SystemTopic::DataMsgId, serializer, [](DataMsg msg) {
        // Republish to local bus so local components can subscribe by name
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, std::move(msg));
    });

    // 4. Subscribe to local DataBus to print received messages
    auto conn = dmq::DataBus::Subscribe<DataMsg>(SystemTopic::DataMsg, [](DataMsg msg) {
        std::cout << "Received Multicast Data: " << msg.actuators.size() << " actuators, " 
                  << msg.sensors.size() << " sensors" << std::endl;
    });

    std::atomic<bool> running{ true };
    std::thread receiveThread([&]() {
        while (running) {
            group->ProcessIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    if (duration > 0) {
        std::thread([duration]() {
            std::this_thread::sleep_for(std::chrono::seconds(duration));
            g_running = false;
        }).detach();
    } else {
        std::cout << "Client joined multicast group 239.1.1.1:8000. Waiting for data... Press Ctrl+C to quit." << std::endl;
    }

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nShutting down..." << std::endl;
    running = false;
    receiveThread.join();
    transport.Close();

#ifdef DMQ_DATABUS_TOOLS
    SpyBridge::Stop();
#endif

    return 0;
}



