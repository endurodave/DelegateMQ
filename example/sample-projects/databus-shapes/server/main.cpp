// main.cpp
// DataBus Shapes Demo Server (Publisher).
// 
// Simulates moving shapes and publishes their positions via Multicast.

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#include "predef/transport/win32-udp/MulticastTransport.h"
#include "predef/util/WinsockConnect.h"
#else
#include "predef/transport/linux-udp/MulticastTransport.h"
#endif

#ifdef DMQ_DATABUS_SPY
#include "SpyBridge.h"
#include <sstream>
#endif

int main() {
    std::cout << "Starting DataBus Shapes Demo SERVER (Publisher)..." << std::endl;

#ifdef _WIN32
    WinsockContext winsock;
    std::string localIP = WinsockContext::GetLocalAddress();
#else
    std::string localIP = "0.0.0.0";
#endif

    std::cout << "Local Interface: " << localIP << std::endl;

#ifdef DMQ_DATABUS_SPY
    // Start Spy Bridge to export DataBus traffic to the Spy Console via Multicast
    // Note: Use port 9999 to isolate spy traffic from app traffic on 8000.
    SpyBridge::StartMulticast("239.1.1.1", 9999, localIP);

    dmq::DataBus::RegisterStringifier<ShapeMsg>(SystemTopic::Square, [](const ShapeMsg& m) {
        return "Square X=" + std::to_string(m.x) + " Y=" + std::to_string(m.y);
    });
    dmq::DataBus::RegisterStringifier<ShapeMsg>(SystemTopic::Circle, [](const ShapeMsg& m) {
        return "Circle X=" + std::to_string(m.x) + " Y=" + std::to_string(m.y);
    });
#endif

    // 1. Initialize Multicast Transport (Group: 239.1.1.1, Port: 8000)
    MulticastTransport transport;
    if (transport.Create(MulticastTransport::Type::PUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return -1;
    }

    // 2. Setup Remote Participant for the group
    auto group = std::make_shared<dmq::Participant>(transport);
    group->AddRemoteTopic(SystemTopic::Square, SystemTopic::SquareId);
    group->AddRemoteTopic(SystemTopic::Circle, SystemTopic::CircleId);
    group->AddRemoteTopic(SystemTopic::Triangle, SystemTopic::TriangleId);
    dmq::DataBus::AddParticipant(group);

    // 3. Register Serializers
    Serializer<void(ShapeMsg)> serializer;
    dmq::DataBus::RegisterSerializer<ShapeMsg>(SystemTopic::Square, serializer);
    dmq::DataBus::RegisterSerializer<ShapeMsg>(SystemTopic::Circle, serializer);
    dmq::DataBus::RegisterSerializer<ShapeMsg>(SystemTopic::Triangle, serializer);

    // 4. Animation loop
    float angle = 0;
    while (true) {
        // Move Square in a rectangle
        ShapeMsg square;
        square.x = 20 + (int)(20 * std::cos(angle));
        square.y = 20 + (int)(10 * std::sin(angle));
        square.color = 0; // Blue
        dmq::DataBus::Publish<ShapeMsg>(SystemTopic::Square, square);

        // Move Circle in a large circle
        ShapeMsg circle;
        circle.x = 50 + (int)(30 * std::sin(angle * 0.5f));
        circle.y = 25 + (int)(15 * std::cos(angle * 0.5f));
        circle.color = 1; // Red
        dmq::DataBus::Publish<ShapeMsg>(SystemTopic::Circle, circle);

        angle += 0.1f;
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    transport.Close();
#ifdef DMQ_DATABUS_SPY
    SpyBridge::Stop();
#endif
    return 0;
}
