
#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using MQTT library. 
/// https://github.com/eclipse-paho/paho.mqtt.c

// Ensure network byte order functions are available
#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
#endif

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include "MQTTClient.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

//#define MQTT_ADDRESS     "tcp://test.mosquitto.org:1883"
#define MQTT_ADDRESS     "tcp://broker.hivemq.com:1883"
#define MQTT_TOPIC       "Delegate_MQTT"
#define MQTT_QOS         1
#define MQTT_TIMEOUT     10000L

#define MQTT_PUB_CLIENTID    "DelegatePub"
#define MQTT_SUB_CLIENTID    "DelegateSub"

static void debugTrace(enum MQTTCLIENT_TRACE_LEVELS level, char* message) {
    //printf("TRACE [%d]: %s\n", level, message);
}

/// @brief MQTT transport example.
class MqttTransport : public ITransport
{
public:
    enum class Type
    {
        PUB,
        SUB
    };

    MqttTransport() = default;

    ~MqttTransport()
    {
        Close();
    }

    int Create(Type type)
    {
        int rc = EXIT_FAILURE;
        printf("Using server at %s\n", MQTT_ADDRESS);

        MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
        m_type = type;
        if (m_type == Type::PUB)
        {
            if ((rc = MQTTClient_create(&m_client, MQTT_ADDRESS, MQTT_PUB_CLIENTID,
                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to create client, return code %d\n", rc);
                return rc;
            }

            conn_opts.keepAliveInterval = 20;
            conn_opts.cleansession = 1;
            if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to connect, return code %d\n", rc);
                return rc;
            }
        }
        else if (m_type == Type::SUB)
        {
            if ((rc = MQTTClient_create(&m_client, MQTT_ADDRESS, MQTT_SUB_CLIENTID,
                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to create client, return code %d\n", rc);
                return rc;
            }

            // Register callbacks to populate internal queue
            if ((rc = MQTTClient_setCallbacks(m_client, this, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to set callbacks, return code %d\n", rc);
                MQTTClient_destroy(&m_client);
                return rc;
            }

            conn_opts.keepAliveInterval = 20;
            conn_opts.cleansession = 1;
            if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to connect, return code %d\n", rc);
                MQTTClient_destroy(&m_client);
                return rc;
            }

            if ((rc = MQTTClient_subscribe(m_client, MQTT_TOPIC, MQTT_QOS)) != MQTTCLIENT_SUCCESS)
            {
                // printf("Failed to subscribe, return code %d\n", rc);
                return rc;
            }
        }
        return MQTTCLIENT_SUCCESS;
    }

    void Close()
    {
        if (m_client)
        {
            if (m_type == Type::SUB)
            {
                MQTTClient_unsubscribe(m_client, MQTT_TOPIC);
            }
            MQTTClient_disconnect(m_client, 1000);
            MQTTClient_destroy(&m_client);
            m_client = nullptr;
        }

        // Wake up any blocked Receive
        {
            std::lock_guard<std::mutex> lock(m_queueMtx);
            m_stop = true;
        }
        m_queueCv.notify_all();
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail())
            return -1;

        // Create a local copy to modify the length
        DmqHeader headerCopy = header;
        std::string payload = os.str();
        if (payload.length() > UINT16_MAX) {
            std::cerr << "Payload too large." << std::endl;
            return -1;
        }
        headerCopy.SetLength(static_cast<uint16_t>(payload.length()));

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Serialize Header (Network Byte Order)
        uint16_t marker = htons(headerCopy.GetMarker());
        uint16_t id = htons(headerCopy.GetId());
        uint16_t seqNum = htons(headerCopy.GetSeqNum());
        uint16_t len = htons(headerCopy.GetLength());

        ss.write(reinterpret_cast<const char*>(&marker), sizeof(marker));
        ss.write(reinterpret_cast<const char*>(&id), sizeof(id));
        ss.write(reinterpret_cast<const char*>(&seqNum), sizeof(seqNum));
        ss.write(reinterpret_cast<const char*>(&len), sizeof(len));

        // Append Payload
        ss.write(payload.data(), payload.size());

        std::string fullPacket = ss.str();

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
        
        pubmsg.payload = (void*)fullPacket.data();
        pubmsg.payloadlen = (int)fullPacket.size();
        pubmsg.qos = MQTT_QOS;
        pubmsg.retained = 0;

        int rc;
        if ((rc = MQTTClient_publishMessage(m_client, MQTT_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
        {
            // printf("Failed to publish message, return code %d\n", rc);
            return EXIT_FAILURE;
        }
        else
        {
            if (headerCopy.GetId() != dmq::ACK_REMOTE_ID && m_transportMonitor)
            {
                m_transportMonitor->Add(headerCopy.GetSeqNum(), headerCopy.GetId());
            }
            return EXIT_SUCCESS;
        }
    }

    virtual int Receive(xstringstream& is, DmqHeader& header) override
    {
        std::unique_lock<std::mutex> lock(m_queueMtx);
        
        // Wait for data or stop signal
        // We use a 1s timeout to allow periodic checks, though CV wakeups are efficient
        if (m_queueCv.wait_for(lock, std::chrono::milliseconds(1000), [this] { return !m_rxQueue.empty() || m_stop; }))
        {
            if (m_stop) return -1;

            if (m_rxQueue.empty()) return -1;

            // Pop message
            std::vector<char> msg = std::move(m_rxQueue.front());
            m_rxQueue.pop();
            lock.unlock(); // Release lock before processing

            if (msg.size() < DmqHeader::HEADER_SIZE) return -1;

            // Deserialize
            xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);
            headerStream.write(msg.data(), DmqHeader::HEADER_SIZE);
            headerStream.seekg(0);

            uint16_t val;
            headerStream.read((char*)&val, 2); header.SetMarker(ntohs(val));
            if (header.GetMarker() != DmqHeader::MARKER) return -1;

            headerStream.read((char*)&val, 2); header.SetId(ntohs(val));
            headerStream.read((char*)&val, 2); header.SetSeqNum(ntohs(val));
            headerStream.read((char*)&val, 2); header.SetLength(ntohs(val));

            // Write payload
            if (msg.size() > DmqHeader::HEADER_SIZE) {
                is.write(msg.data() + DmqHeader::HEADER_SIZE, msg.size() - DmqHeader::HEADER_SIZE);
            }

            // ACK Logic
            if (header.GetId() == dmq::ACK_REMOTE_ID) {
                if (m_transportMonitor) m_transportMonitor->Remove(header.GetSeqNum());
            }
            else if (m_transportMonitor) {
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(header.GetSeqNum());
                Send(ss_ack, ack);
            }
            return 0;
        }

        return -1; // Timeout
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        m_transportMonitor = transportMonitor;
    }

private:
    static void delivered(void* context, MQTTClient_deliveryToken dt)
    {
        // printf("Message with token value %d delivery confirmed\n", dt);
    }

    // Callback running on Paho Thread
    static int msgarrvd(void* context, char* topicName, int topicLen, MQTTClient_message* message)
    {
        MqttTransport* instance = (MqttTransport*)context;
        if (instance)
        {
            // Copy data to vector and push to queue
            std::vector<char> data;
            data.resize(message->payloadlen);
            std::memcpy(data.data(), message->payload, message->payloadlen);

            {
                std::lock_guard<std::mutex> lock(instance->m_queueMtx);
                instance->m_rxQueue.push(std::move(data));
            }
            instance->m_queueCv.notify_one();
        }

        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    static void connlost(void* context, char* cause)
    {
        // printf("\nConnection lost\n");
    }

    MQTTClient m_client = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
    Type m_type = Type::PUB;

    // Thread-safe Queue for bridging Paho callback to ITransport::Receive
    std::queue<std::vector<char>> m_rxQueue;
    std::mutex m_queueMtx;
    std::condition_variable m_queueCv;
    bool m_stop = false;
};

#endif // MQTT_TRANSPORT_H