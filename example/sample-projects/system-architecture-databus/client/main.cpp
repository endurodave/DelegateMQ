#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>

int main() {
    std::cout << "Starting System Architecture CLIENT (Subscriber)..." << std::endl;

#ifdef _WIN32
    // 1. Initialize Winsock (Windows only). WinsockContext defined in WinsockConnect.h.
    WinsockContext winsock;
#endif

    // 2. Initialize Transport (Listen on localhost:8000)
    // UdpTransport is automatically defined by including DelegateMQ.h with DMQ_TRANSPORT_WIN32_UDP
    UdpTransport transport;
    if (transport.Create(UdpTransport::Type::SUB, "127.0.0.1", 8000) != 0) {
        std::cerr << "Failed to create UDP transport" << std::endl;
        return -1;
    }

    // 3. Setup Remote Participant
    auto server = std::make_shared<dmq::Participant>(transport);
    
    // Register how to handle incoming remote DataMsg
    Serializer<void(DataMsg)> serializer;
    server->RegisterHandler<DataMsg>(SystemTopic::DataMsgId, serializer, [](DataMsg msg) {
        // Republish to local bus so local components can subscribe by name
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, std::move(msg));
    });

    // 4. Local Subscription
    // This component only cares about the local bus topic name
    auto conn = dmq::DataBus::Subscribe<DataMsg>(SystemTopic::DataMsg, [](DataMsg msg) {
        std::cout << "--- Received DataMsg ---" << std::endl;
        for (const auto& act : msg.actuators) {
            std::cout << "  Actuator " << act.id << ": pos=" << act.position << ", v=" << act.voltage << std::endl;
        }
        for (const auto& sen : msg.sensors) {
            std::cout << "  Sensor " << sen.id << ": supply=" << sen.supplyV << ", reading=" << sen.readingV << std::endl;
        }
    });

    // 5. Background thread to process incoming network data
    std::atomic<bool> running{true};
    std::thread receiveThread([&]() {
        while (running) {
            server->ProcessIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for a while
    std::cout << "Client running. Waiting for data..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(60));

    running = false;
    receiveThread.join();

    transport.Close();
    return 0;
}
