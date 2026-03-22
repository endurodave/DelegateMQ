#include "DelegateMQ.h"
#include <iostream>
#include <queue>

#if defined(DMQ_DATABUS)

// Simple loopback transport for testing
class DataBusLoopbackTransport : public ITransport {
public:
    struct Packet {
        DmqHeader header;
        std::vector<char> data;
    };

    virtual int Send(xostringstream& os, const DmqHeader& header) override {
        std::string s = os.str();
        std::vector<char> data(s.begin(), s.end());
        m_queue.push({header, data});
        return 0;
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override {
        if (m_queue.empty()) return -1;
        Packet p = m_queue.front();
        m_queue.pop();
        header = p.header;
        is.write(p.data.data(), p.data.size());
        return 0;
    }

private:
    std::queue<Packet> m_queue;
};

int DataBusRemoteTestMain() {
    std::cout << "Starting DataBusRemoteTest..." << std::endl;
    dmq::DataBus::ResetForTesting();

    DataBusLoopbackTransport transport;
    Serializer<void(int)> serializer;

    // Node B setup (Subscriber)
    auto nodeB = std::make_shared<dmq::Participant>(transport);
    
    // In a real system, nodeB's DataBus would be separate. 
    // Here we simulate it by having Node B's participant call back into a local variable.
    int receivedValue = 0;
    nodeB->RegisterHandler<int>(100, serializer, [&](int val) {
        receivedValue = val;
    });

    // Node A setup (Publisher)
    auto nodeA_participant = std::make_shared<dmq::Participant>(transport);
    nodeA_participant->AddRemoteTopic("remote/topic", 100);
    
    dmq::DataBus::AddParticipant(nodeA_participant);
    dmq::DataBus::RegisterSerializer<int>("remote/topic", serializer);

    // Node A publishes
    dmq::DataBus::Publish<int>("remote/topic", 42);

    // Simulate Node B processing incoming data
    nodeB->ProcessIncoming();

    ASSERT_TRUE(receivedValue == 42);
    std::cout << "DataBusRemoteTest PASSED!" << std::endl;

    return 0;
}

#else

int DataBusRemoteTestMain() {
    return 0;
}

#endif
