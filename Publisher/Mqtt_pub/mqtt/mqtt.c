#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Winsock2.h> // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h> // needed for ip_mreq definition for multicast

#include <Windows.h>
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>


/* For version 3 of the MQTT protocol */
#include "mqtt.h"

#define PROTOCOL_NAME       "MQIsdp"    // 
#define PROTOCOL_VERSION    3U          // version 3.0 of MQTT
#define CLEAN_SESSION       (1U<<1)
#define KEEPALIVE           30U         // specified in seconds
#define MESSAGE_ID          1U // not used by QoS 0 - value must be > 0

/* Macros for accessing the MSB and LSB of a uint16_t */
#define MSB(A) ((uint8_t)((A & 0xFF00) >> 8))
#define LSB(A) ((uint8_t)(A & 0x00FF))


#define SET_MESSAGE(M) ((uint8_t)(M << 4))
#define GET_MESSAGE(M) ((uint8_t)(M >> 4))

typedef enum {
    CONNECT = 1,
    CONNACK,
    PUBLISH,
    PUBACK,
    PUBREC,
    PUBREL,
    PUBCOMP,
    SUBSCRIBE,
    SUBACK,
    UNSUBSCRIBE,
    UNSUBACK,
    PINGREQ,
    PINGRESP,
    DISCONNECT
} connect_msg_t;

typedef enum {
    Connection_Accepted,
    Connection_Refused_unacceptable_protocol_version,
    Connection_Refused_identifier_rejected,
    Connection_Refused_server_unavailable,
    Connection_Refused_bad_username_or_password,
    Connection_Refused_not_authorized
} connack_msg_t;


struct mqtt_broker_handle
{
    int                 socket;
    struct sockaddr_in  socket_address;
    uint16_t            port;
    char                hostname[16];  // based on xxx.xxx.xxx.xxx format
    char                clientid[24];  // max 23 charaters long
    bool                connected;
    size_t              topic;
    uint16_t            pubMsgID;
    uint16_t            subMsgID;
};

typedef struct fixed_header_t
{
    uint16_t     retain : 1;
    uint16_t     Qos : 2;
    uint16_t     DUP : 1;
    uint16_t     connect_msg_t : 4;
    uint16_t     remaining_length : 8;
}fixed_header_t;



mqtt_broker_handle_t* mqtt_connect(const char* client, const char * server_ip, uint32_t port)
{

#ifdef _WIN32
    //
    // Initialize Windows Socket API with given VERSION.
    //
    WSADATA wsaData;
    if (WSAStartup(0x0101, &wsaData))
    {
        return 0;
    }
#endif


    mqtt_broker_handle_t *broker = (mqtt_broker_handle_t *)calloc(sizeof(mqtt_broker_handle_t), 1);
    if (broker != 0) {
        // check connection strings are within bounds
        if ((strlen(client) + 1 > sizeof(broker->clientid)) || (strlen(server_ip) + 1 > sizeof(broker->hostname))) {
            fprintf(stderr, "failed to connect: client or server exceed limits\n");
            free(broker);
            return 0;  // strings too large
        }

        broker->port = port;
        strcpy(broker->hostname, server_ip);
        strcpy(broker->clientid, client);

        if ((broker->socket = (int)socket(PF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "failed to connect: could not create socket\n");
            free(broker);
            return 0;
        }

        // create the stuff we need to connect
        broker->connected = false;
        broker->socket_address.sin_family = AF_INET;
        broker->socket_address.sin_port = htons(broker->port); // converts the unsigned short from host byte order to network byte order
        broker->socket_address.sin_addr.s_addr = inet_addr(broker->hostname);

        // connect
#if 0
        if ((connect(broker->socket, (struct sockaddr *)&broker->socket_address, sizeof(broker->socket_address))) < 0) {
            fprintf(stderr, "failed to connect: to server socket\n");
            free(broker);
            return 0;
        }
#else
        int retVal = connect(broker->socket, (struct sockaddr *)&broker->socket_address, sizeof(broker->socket_address));
        if (retVal < 0)
        {
            fprintf(stderr, "failed to connect: to server socket\n");
            free(broker);
            return 0;
        }
#endif
        // variable header
        uint8_t var_header[] = {
            0,                         // MSB(strlen(PROTOCOL_NAME)) but 0 as messages must be < 127
            (uint8_t)strlen(PROTOCOL_NAME),     // LSB(strlen(PROTOCOL_NAME)) is redundant 
            'M','Q','I','s','d','p',
            PROTOCOL_VERSION,
            CLEAN_SESSION,
            MSB(KEEPALIVE),
            LSB(KEEPALIVE)
        };

        // set up payload for connect message
       // uint8_t payload[2+strlen(broker->clientid)];
       // uint8_t payload[2 + 24];
        uint32_t sizeofpayload = (uint32_t)(2 + strlen(broker->clientid));
        uint8_t *payload = (uint8_t*)calloc(sizeofpayload, sizeof(uint8_t));
        payload[0] = 0;
        payload[1] = (uint8_t)strlen(broker->clientid);
        memcpy(&payload[2], broker->clientid, strlen(broker->clientid));

        // fixed header: 2 bytes, big endian
        uint8_t fixed_header[] = { SET_MESSAGE(CONNECT), (uint8_t)(sizeof(var_header) + sizeofpayload) };
        //      fixed_header_t  fixed_header = { .QoS = 0, .connect_msg_t = CONNECT, .remaining_length = sizeof(var_header)+strlen(broker->clientid) };

        //uint8_t packet[sizeof(fixed_header) + sizeof(var_header) + sizeof(payload)];
        uint32_t sizeofpacket = sizeof(fixed_header) + sizeof(var_header) + sizeofpayload;
        uint8_t *packet = calloc(sizeofpacket, sizeof(uint8_t));
        memset(packet, 0, sizeofpacket);
        memcpy(packet, fixed_header, sizeof(fixed_header));
        memcpy(packet + sizeof(fixed_header), var_header, sizeof(var_header));
        memcpy(packet + sizeof(fixed_header) + sizeof(var_header), payload, sizeofpayload);

        // send Connect message
        if (send(broker->socket, packet, sizeofpacket, 0) < (int32_t)sizeofpacket) {
            close(broker->socket);
            free(broker);
            free(packet);
            free(payload);
            return 0;
        }
        free(packet);
        free(payload);

        uint8_t buffer[4];
        long sz = recv(broker->socket, buffer, sizeof(buffer), 0);  // wait for CONNACK
        //        printf("buffer size is %ld\n",sz);
        //        printf("%2x:%2x:%2x:%2x\n",(uint8_t)buffer[0],(uint8_t)buffer[1],(uint8_t)buffer[2],(uint8_t)buffer[3]);
        if ((GET_MESSAGE(buffer[0]) == CONNACK) && ((sz - 2) == buffer[1]) && (buffer[3] == Connection_Accepted))
        {
            printf("Connected to MQTT Server at %s:%4d\n", server_ip, port);
        }
        else
        {
            fprintf(stderr, "failed to connect with error: %d\n", buffer[3]);
            close(broker->socket);
            free(broker);
            return 0;
        }
        // set connected flag
        broker->connected = true;
    }
    return broker;
}

MqttResult_t mqtt_subscribe(mqtt_broker_handle_t *broker, const char *topic, QoS qos)
{
    if (!broker->connected) {
        return MQTT_ERROR;
    }

    uint8_t var_header[] = { MSB(MESSAGE_ID), LSB(MESSAGE_ID) };	// appended to end of PUBLISH message
    uint32_t topicLen = (uint32_t)strlen(topic);
    // utf topic
    //uint8_t utf_topic[2+strlen(topic)+1]; // 2 for message size + 1 for QoS
    //uint8_t utf_topic[2 + 4 + 1]; // 2 for message size + 1 for QoS
    uint32_t sizeofutf_topic = 2 + topicLen + 1;
    uint8_t * utf_topic = calloc(sizeofutf_topic, sizeof(uint8_t));
    // set up topic payload
    utf_topic[0] = 0;                       // MSB(strlen(topic));
    utf_topic[1] = LSB(strlen(topic));
    memcpy((char *)&utf_topic[2], topic, topicLen);
    utf_topic[sizeofutf_topic - 1] = qos;

    uint8_t fixed_header[] = { SET_MESSAGE(SUBSCRIBE), (uint8_t)(sizeof(var_header) + sizeofutf_topic) };
    //    fixed_header_t  fixed_header = { .QoS = 0, .connect_msg_t = SUBSCRIBE, .remaining_length = sizeof(var_header)+strlen(utf_topic) };

    //uint8_t packet[sizeof(fixed_header) + sizeof(var_header) + sizeofMessage];
    uint32_t sizeofPacket = sizeof(fixed_header) + sizeof(var_header) + sizeofutf_topic;
    uint8_t * packet = calloc(sizeofPacket, sizeof(uint8_t));

    memset(packet, 0, sizeofPacket);
    memcpy(packet, &fixed_header, sizeof(fixed_header));
    memcpy(packet + sizeof(fixed_header), var_header, sizeof(var_header));
    memcpy(packet + sizeof(fixed_header) + sizeof(var_header), utf_topic, sizeofutf_topic);

    if (send(broker->socket, packet, sizeofPacket, 0) < (int32_t)sizeofPacket) {
        puts("failed to send subscribe message");
        free(packet);
        free(utf_topic);
        return MQTT_ERROR;
    }
    free(packet);
    free(utf_topic);

    uint8_t buffer[5];
    long sz = recv(broker->socket, buffer, sizeof(buffer), 0);  // wait for SUBACK

    //    printf("buffer size is %ld\n",sz);
    //    printf("%2x:%2x:%2x:%2x:%2x\n",(uint8_t)buffer[0],(uint8_t)buffer[1],(uint8_t)buffer[2],(uint8_t)buffer[3],(uint8_t)buffer[4]);

    if ((GET_MESSAGE(buffer[0]) == SUBACK) && ((sz - 2) == buffer[1]) && (buffer[2] == MSB(MESSAGE_ID)) && (buffer[3] == LSB(MESSAGE_ID))) {
        printf("Subscribed to MQTT Service %s with QoS %d\n", topic, buffer[4]);
    }
    else
    {
        puts("failed to subscribe");
        return MQTT_ERROR;
    }
    broker->topic = strlen(topic);
    struct timeval tv;

    tv.tv_sec = 30;  /* 30 Secs Timeout */
    tv.tv_usec = 0;  // Not init'ing this can cause strange errors

    setsockopt(broker->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
    return MQTT_OK;
}

int SetSocketTimeout(int connectSocket, int seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds * 1000;


    // tv.tv_sec = seconds / 1000;
     //tv.tv_usec = (seconds % 1000) * 1000;

    return setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv);
}

MqttResult_t mqtt_display_message(mqtt_broker_handle_t *broker, int(*print)(int))
{
    uint8_t buffer[128];
    // SetSocketTimeout(broker->socket, 1000000000000000);
    SetSocketTimeout(broker->socket, 60);

    while (1) {
        // wait for next message
        long sz = recv(broker->socket, buffer, sizeof(buffer), 0);
        //printf("message size is %ld\n",sz);        
        // if more than ack - i.e. data > 0
        if (sz == 0)
        {
            /* Socket has been disconnected */
           //printf("\nSocket EOF\n");
            printf("\n Connection Time Out");
            broker->socket = 0;
            return MQTT_CONNECTION_TIMEOUT;
        }

        if (sz < 0)
        {
            // printf("\nSocket recv returned %ld, errno %d %s\n", sz, errno, strerror(errno));
            printf("\n Connection Time Out");
            // close(broker->socket); /* Close socket if we get an error */
            broker->socket = 0;
            return MQTT_ERROR;
        }

        if (sz > 0) {
            //printf("message size is %ld\n",sz);
            if (GET_MESSAGE(buffer[0]) == PUBLISH) {
                //printf("Got PUBLISH message with size %d\n", (uint8_t)buffer[1]);
                uint32_t topicSize = (buffer[2] << 8) + buffer[3];
                //printf("topic size is %d\n", topicSize);
                //for(int loop = 0; loop < topicSize; ++loop) {
                //    putchar(buffer[4+loop]);
                //}
                //putchar('\n');
                unsigned long i = 4 + topicSize;
                if (((buffer[0] >> 1) & 0x03) > QoS0) {
                    uint32_t messageId = (buffer[4 + topicSize] << 8) + buffer[4 + topicSize + 1];
                    //printf("Message ID is %d\n", messageId);
                    i += 2; // 2 extra for msgID
                    // if QoS1 the send PUBACK with message ID
                    uint8_t puback[4] = { SET_MESSAGE(PUBACK), 2, buffer[4 + topicSize], buffer[4 + topicSize + 1] };
                    if (send(broker->socket, puback, sizeof(puback), 0) < sizeof(puback)) {
                        puts("failed to PUBACK");
                        return MQTT_ERROR;
                    }
                }
                for (; (long)i < sz; ++i) {
                    print(buffer[i]);
                }
                print('\n');
                return MQTT_OK;
            }
        }
    }
    return MQTT_ERROR;
}


MqttResult_t mqtt_publish(mqtt_broker_handle_t *broker, const char *topic, const char *msg, QoS qos)
{
    if (!broker->connected) {
        return MQTT_ERROR;
    }
    if (qos > QoS2) {
        return MQTT_ERROR;
    }

    if (qos == QoS0) {
        // utf topic
       // uint8_t utf_topic[2+strlen(topic)]; // 2 for message size QoS-0 does not have msg ID
        //uint8_t utf_topic[2 + 50];
        uint32_t topicLen = (uint32_t)strlen(topic);
        uint32_t sizeofutf_topic = 2 + topicLen;
        uint8_t * utf_topic = calloc(sizeofutf_topic, sizeof(uint8_t));
        // set up topic payload
        utf_topic[0] = 0;                       // MSB(strlen(topic));
        utf_topic[1] = LSB(strlen(topic));
        memcpy((char *)&utf_topic[2], topic, strlen(topic));

        uint8_t fixed_header[] = { SET_MESSAGE(PUBLISH) | (qos << 1), (uint8_t)(sizeofutf_topic + strlen(msg)) };
        //    fixed_header_t  fixed_header = { .QoS = 0, .connect_msg_t = PUBLISH, .remaining_length = sizeof(utf_topic)+strlen(msg) };

            //uint8_t packet[sizeof(fixed_header)+sizeof(utf_topic)+strlen(msg)];
        //uint8_t packet[sizeof(fixed_header) + sizeof(utf_topic) + 100];

        uint32_t sizeofPacket = (uint32_t)(sizeof(fixed_header) + sizeofutf_topic + strlen(msg));
        uint8_t * packet = calloc(sizeofPacket, sizeof(uint8_t));

        memset(packet, 0, sizeofPacket);
        memcpy(packet, &fixed_header, sizeof(fixed_header));
        memcpy(packet + sizeof(fixed_header), utf_topic, sizeof(utf_topic));
        memcpy(packet + sizeof(fixed_header) + sizeof(utf_topic), msg, strlen(msg));

        if (send(broker->socket, packet, sizeofPacket, 0) <(int32_t)sizeofPacket)
        {
            free(packet);
            free(utf_topic);
            return MQTT_ERROR;
        }
        free(packet);
        free(utf_topic);
    }
    else {
        broker->pubMsgID++;
        // utf topic
       // uint8_t utf_topic[2+strlen(topic)+2]; // 2 extra for message size > QoS0 for msg ID
      //  uint8_t utf_topic[2 + 50 + 2];
        uint32_t topicLen = (uint32_t)strlen(topic);
        uint32_t sizeofutf_topic = 2 + topicLen + 2;
        uint8_t * utf_topic = calloc(sizeofutf_topic, sizeof(uint8_t));

        // set up topic payload
        utf_topic[0] = 0;                       // MSB(strlen(topic));
        utf_topic[1] = LSB(strlen(topic));
        memcpy((char *)&utf_topic[2], topic, strlen(topic));
        utf_topic[sizeofutf_topic - 2] = MSB(broker->pubMsgID);
        utf_topic[sizeofutf_topic - 1] = LSB(broker->pubMsgID);

        uint8_t fixed_header[] = { SET_MESSAGE(PUBLISH) | (qos << 1), (uint8_t)(sizeofutf_topic + strlen(msg)) };

        // uint8_t packet[sizeof(fixed_header)+sizeof(utf_topic)+strlen(msg)];
        //uint8_t packet[sizeof(fixed_header) + sizeof(utf_topic) + 100];
        uint32_t sizeofPacket = (uint32_t)(sizeof(fixed_header) + sizeofutf_topic + strlen(msg));
        uint8_t * packet = calloc(sizeofPacket, sizeof(uint8_t));

        memset(packet, 0, sizeofPacket);
        memcpy(packet, &fixed_header, sizeof(fixed_header));
        memcpy(packet + sizeof(fixed_header), utf_topic, sizeofutf_topic);
        memcpy(packet + sizeof(fixed_header) + sizeofutf_topic, msg, strlen(msg));

        if (send(broker->socket, packet, sizeofPacket, 0) < (int32_t)sizeofPacket)
        {
            free(packet);
            free(utf_topic);
            return MQTT_ERROR;
        }
        free(packet);
        free(utf_topic);

        if (qos == QoS1) {
            // expect PUBACK with MessageID
            uint8_t buffer[4];
            long sz = recv(broker->socket, buffer, sizeof(buffer), 0);  // wait for SUBACK

            //    printf("buffer size is %ld\n",sz);
            //    printf("%2x:%2x:%2x:%2x:%2x\n",(uint8_t)buffer[0],(uint8_t)buffer[1],(uint8_t)buffer[2],(uint8_t)buffer[3],(uint8_t)buffer[4]);

            if ((GET_MESSAGE(buffer[0]) == PUBACK) && ((sz - 2) == buffer[1]) && (buffer[2] == MSB(broker->pubMsgID)) && (buffer[3] == LSB(broker->pubMsgID))) {
                printf("Published to MQTT Service %s with QoS1\n", topic);
            }
            else
            {
                puts("failed to publisg at QoS1");
                return MQTT_ERROR;
            }
        }

    }

    return MQTT_OK;
}

MqttResult_t mqtt_disconnect(mqtt_broker_handle_t *broker)
{
    uint8_t fixed_header[] = { SET_MESSAGE(DISCONNECT), 0 };
    if (send(broker->socket, fixed_header, sizeof(fixed_header), 0) < sizeof(fixed_header))
    {
        puts("failed to disconnect");
        return MQTT_ERROR;
    }
    return MQTT_OK;
}

void mqtt_Sleep(uint32_t milliSeconds)
{
#ifdef _WIN32
    Sleep(milliSeconds); //sleep in milli Seconds
#elif linux
    usleep(milliSeconds * 1000);  //sleep in milli Seconds
#endif
}

