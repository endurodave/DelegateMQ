#include "DelegateMQ.h"
#include "SystemMessages.h"
#include "SystemIds.h"
#include <iostream>
#include <vector>
#include <thread>

int main() {
    std::cout << "Starting System Architecture SERVER (Publisher)..." << std::endl;

#ifdef _WIN32
    // 1. Initialize Winsock (Windows only). WinsockContext defined in WinsockConnect.h.
    WinsockContext winsock;
#endif

    // 2. Initialize Transport (Send to Client on localhost:8000)
    // UdpTransport is automatically defined by including DelegateMQ.h with DMQ_TRANSPORT_WIN32_UDP
    UdpTransport transport;
    if (transport.Create(UdpTransport::Type::PUB, "127.0.0.1", 8000) != 0) {
        std::cerr << "Failed to create UDP transport" << std::endl;
        return -1;
    }

    // 3. Setup Remote Participant
    auto client = std::make_shared<dmq::Participant>(transport);
    client->AddRemoteTopic(SystemTopic::DataMsg, SystemTopic::DataMsgId);
    
    dmq::DataBus::AddParticipant(client);

    // 4. Register Serializer for DataMsg
    Serializer<void(DataMsg)> serializer;
    dmq::DataBus::RegisterSerializer<DataMsg>(SystemTopic::DataMsg, serializer);

    // 5. Main loop: Publish data every second
    int iteration = 0;
    while (iteration < 60) {
        DataMsg msg;
        
        // Simulate data
        Actuator act;
        act.id = 1;
        act.position = 10.0f + (float)iteration;
        act.voltage = 12.0f;
        msg.actuators.push_back(act);

        Sensor sen;
        sen.id = 101;
        sen.supplyV = 5.0f;
        sen.readingV = 2.5f + ((float)iteration * 0.1f);
        msg.sensors.push_back(sen);

        std::cout << "Publishing DataMsg, iteration: " << iteration << std::endl;
        dmq::DataBus::Publish<DataMsg>(SystemTopic::DataMsg, msg);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        iteration++;
    }

    transport.Close();
    return 0;
}
