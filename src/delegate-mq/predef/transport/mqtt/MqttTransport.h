#ifndef MQTT_TRANSPORT_H
#define MQTT_TRANSPORT_H

/// @file 
/// @see https://github.com/endurodave/DelegateMQ
/// David Lafreniere, 2025.
/// 
/// Transport callable argument data to/from a remote using MQTT library. 
/// https://github.com/eclipse-paho/paho.mqtt.c

#include "predef/transport/ITransport.h"
#include "predef/transport/ITransportMonitor.h"
#include "predef/transport/DmqHeader.h"
#include "MQTTClient.h"

#define MQTT_ADDRESS     "tcp://mqtt.eclipseprojects.io:1883"
#define MQTT_TOPIC       "Delegate_MQTT"
#define MQTT_QOS         1
#define MQTT_TIMEOUT     10000L

#define MQTT_PUB_CLIENTID    "DelegatePub"
#define MQTT_SUB_CLIENTID    "DelegateSub"

class IMqttReceiveHandler
{
public:
    virtual void Receive() = 0;
};

void debugTrace(enum MQTTCLIENT_TRACE_LEVELS level, char* message) {
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

    int Create(Type type)
    {
        int rc = EXIT_FAILURE;
        printf("Using server at %s\n", MQTT_ADDRESS);

        MQTTClient_setTraceCallback(debugTrace);

        MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
        m_type = type;
        if (m_type == Type::PUB)
        {
            if ((rc = MQTTClient_create(&m_client, MQTT_ADDRESS, MQTT_PUB_CLIENTID,
                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to create client, return code %d\n", rc);
                goto exit;
            }

            conn_opts.keepAliveInterval = 20;
            conn_opts.cleansession = 1;
            if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to connect, return code %d\n", rc);
                goto exit;
            }
            goto exit;
        }
        else if (m_type == Type::SUB)
        {
            if ((rc = MQTTClient_create(&m_client, MQTT_ADDRESS, MQTT_SUB_CLIENTID,
                MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to create client, return code %d\n", rc);
                rc = EXIT_FAILURE;
                goto exit;
            }

            if ((rc = MQTTClient_setCallbacks(m_client, this, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to set callbacks, return code %d\n", rc);
                rc = EXIT_FAILURE;
                MQTTClient_destroy(&m_client);
                goto exit;
            }

            conn_opts.keepAliveInterval = 20;
            conn_opts.cleansession = 1;
            if ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to connect, return code %d\n", rc);
                rc = EXIT_FAILURE;
                MQTTClient_destroy(&m_client);
                goto exit;
            }

            if ((rc = MQTTClient_subscribe(m_client, MQTT_TOPIC, MQTT_QOS)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to subscribe, return code %d\n", rc);
                rc = EXIT_FAILURE;
            }
        }
    exit:
        return rc;
    }

    void Close()
    {
        int rc = EXIT_FAILURE;
        if (m_type == Type::SUB)
        {
            if ((rc = MQTTClient_unsubscribe(m_client, MQTT_TOPIC)) != MQTTCLIENT_SUCCESS)
            {
                printf("Failed to unsubscribe, return code %d\n", rc);
                rc = EXIT_FAILURE;
            }
        }

        if ((rc = MQTTClient_disconnect(m_client, 10000)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to disconnect, return code %d\n", rc);
            rc = EXIT_FAILURE;
        }

        MQTTClient_destroy(&m_client);
    }

    virtual int Send(xostringstream& os, const DmqHeader& header) override
    {
        if (os.bad() || os.fail())
            return -1;

        xostringstream ss(std::ios::in | std::ios::out | std::ios::binary);

        // Write each header value using the getters from DmqHeader
        auto marker = header.GetMarker();
        ss << "[" << marker << " ";

        auto id = header.GetId();
        ss << id << " ";

        auto seqNum = header.GetSeqNum();
        ss << seqNum << "] ";

        // Insert delegate arguments from the stream (os)
        ss << os.str();

        size_t payloadLen = ss.str().length();

        // Allocate memory for the payload
        void* payload = malloc(payloadLen);  // Allocate memory to hold the payload
        if (payload == NULL) {
            std::cerr << "Failed to allocate memory for payload." << std::endl;
            return -1;  // Return error code if malloc fails
        }

        // Copy the content of the stringstream into the payload
        std::memcpy(payload, ss.str().c_str(), payloadLen);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        MQTTClient_deliveryToken token;
        int rc;
        pubmsg.payload = payload;
        pubmsg.payloadlen = (int)payloadLen;
        pubmsg.qos = MQTT_QOS;
        pubmsg.retained = 0;
        if ((rc = MQTTClient_publishMessage(m_client, MQTT_TOPIC, &pubmsg, &token)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to publish message, return code %d\n", rc);
            free(payload);
            return EXIT_FAILURE;
        }
        else
        {
            if (id != dmq::ACK_REMOTE_ID)
            {
                // Add sequence number to monitor
                if (m_transportMonitor)
                    m_transportMonitor->Add(seqNum, id);
            }
            free(payload);
            return EXIT_SUCCESS;
        }
    }

    virtual xstringstream Receive(DmqHeader& header) override
    {
        xstringstream headerStream(std::ios::in | std::ios::out | std::ios::binary);

        if (!m_payload)
            return headerStream;

        // Find the position of the opening square bracket
        size_t headerStart = 0;
        while (m_payload[headerStart] != '[' && headerStart < m_payloadLen) {
            ++headerStart;
        }

        if (headerStart == m_payloadLen) {
            std::cerr << "Invalid header format: Missing opening '['" << std::endl;
            return headerStream;
        }

        // Find the position of the closing square bracket
        size_t headerEnd = headerStart;
        while (m_payload[headerEnd] != ']' && headerEnd < m_payloadLen) {
            ++headerEnd;
        }

        if (headerEnd == m_payloadLen) {
            std::cerr << "Invalid header format: Missing closing ']'" << std::endl;
            return headerStream;
        }

        // Calculate the header size (everything between '[' and ']')
        size_t headerSize = headerEnd - headerStart + 1;

        // Now extract the header between '[' and ']'
        std::string headerStr(m_payload + headerStart + 1, headerSize - 2);

        std::stringstream headerParser(headerStr);

        uint16_t marker = 0, id = 0, seqNum = 0;
        headerParser >> marker >> id >> seqNum;

        header.SetMarker(marker);
        header.SetId(id);
        header.SetSeqNum(seqNum);

        // Check for the correct sync marker
        if (header.GetMarker() != DmqHeader::MARKER) {
            std::cerr << "Invalid sync marker!" << std::endl;
            return headerStream;
        }

        xstringstream argStream(std::ios::in | std::ios::out | std::ios::binary);

        // The remaining data after the closing bracket is the JSON part
        size_t jsonStart = headerEnd + 1; // After the closing header bracket ']'
        argStream.write(m_payload + jsonStart, m_payloadLen - jsonStart);

        if (id == dmq::ACK_REMOTE_ID)
        {
            // Receiver ack'ed message. Remove sequence number from monitor.
            if (m_transportMonitor)
                m_transportMonitor->Remove(seqNum);
        }
        else
        {          
            if (m_transportMonitor)
            {
                // Send header with received seqNum as the ack message
                xostringstream ss_ack;
                DmqHeader ack;
                ack.SetId(dmq::ACK_REMOTE_ID);
                ack.SetSeqNum(seqNum);
                Send(ss_ack, ack);
            }
        }

        // argStream contains the serialized remote argument data
        return argStream;
    }

    void SetReceiveHandler(IMqttReceiveHandler* handler)
    {
        m_receiveHandler = handler;
    }

    void SetTransportMonitor(ITransportMonitor* transportMonitor)
    {
        m_transportMonitor = transportMonitor;
    }

private:
    static void delivered(void* context, MQTTClient_deliveryToken dt)
    {
        printf("Message with token value %d delivery confirmed\n", dt);
        MqttTransport* instance = (MqttTransport*)context;
        if (instance)
        {
            instance->deliveredtoken = dt;
        }
    }

    static int msgarrvd(void* context, char* topicName, int topicLen, MQTTClient_message* message)
    {
        printf("Message arrived\n");
        printf("     topic: %s\n", topicName);
        printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);

        MqttTransport* instance = (MqttTransport*)context;
        if (instance && instance->m_receiveHandler)
        {
            instance->m_payload = (char*)message->payload;
            instance->m_payloadLen = message->payloadlen;

            // Handling incoming message
            instance->m_receiveHandler->Receive();

            instance->m_payload = nullptr;
            instance->m_payloadLen = 0;
        }

        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    static void connlost(void* context, char* cause)
    {
        printf("\nConnection lost\n");
        if (cause)
            printf("     cause: %s\n", cause);
    }

    volatile MQTTClient_deliveryToken deliveredtoken;

    MQTTClient m_client = nullptr;

    IMqttReceiveHandler* m_receiveHandler = nullptr;
    ITransportMonitor* m_transportMonitor = nullptr;
    Type m_type = Type::PUB;

    char* m_payload = nullptr;
    int m_payloadLen = 0;
};

#endif
