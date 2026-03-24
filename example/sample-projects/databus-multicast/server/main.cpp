// main.cpp
// DataBus Multicast Server (Publisher) Example.
// 
// This sample demonstrates a distributed publisher using dmq::DataBus 
// over UDP Multicast for one-to-many "Best Effort" distribution.

#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>

#if defined(_WIN32) || defined(_WIN64)
#include "predef/transport/win32-udp/MulticastTransport.h"
#else
#include "predef/transport/linux-udp/MulticastTransport.h"
#endif

#ifdef DMQ_DATABUS_SPY
#include "SpyBridge.h"
#include <sstream>
#endif

int main() {
    std::cout << "Starting DataBus Multicast SERVER (Publisher)..." << std::endl;

    NetworkContext winsock;
    // Auto-detect physical IP for multicast interface
    std::string localIP = NetworkContext::GetLocalAddress();

    std::cout << "Local Interface: " << localIP << std::endl;

#ifdef DMQ_DATABUS_SPY
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
    if (transport.Create(MulticastTransport::Type::PUB, "239.1.1.1", 8000, localIP.c_str()) != 0) {
        std::cerr << "Failed to create Multicast transport" << std::endl;
        return -1;
    }

    // 2. Setup Remote Participant
    auto group = std::make_shared<dmq::Participant>(transport);
    group->AddRemoteTopic(SystemTopic::DataMsg, SystemTopic::DataMsgId);
    dmq::DataBus::AddParticipant(group);

    // 3. Register Serializer
    Serializer<void(DataMsg)> serializer;
    dmq::DataBus::RegisterSerializer<DataMsg>(SystemTopic::DataMsg, serializer);

    // 4. Main loop: Publish data to the multicast group every second
    int iteration = 0;
    while (iteration < 120) {
        DataMsg msg;
        Actuator act; act.id = 1; act.position = 10.0f + (float)iteration;
        msg.actuators.push_back(act);

        Sensor sen; sen.id = 101; sen.readingV = 2.5f + ((float)iteration * 0.1f);
        msg.sensors.push_back(sen);

        std::cout << "Multicast Publishing DataMsg, iteration: " << iteration << std::endl;
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, msg);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        iteration++;
    }

    transport.Close();

#ifdef DMQ_DATABUS_SPY
    SpyBridge::Stop();
#endif

    return 0;
}



