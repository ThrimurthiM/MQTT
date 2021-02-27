/*
- * COPYRIGHT (C) 2021 Electosh Solution Pvt. Ltd - All Rights Reserved
 *
 *      %name      : main.c %
 *      Author     : Thrimurthi M
 *      Modified By: Thrimurthi M
 */

#include "mqtt.h"

#define TIME_OUT_IN_SEC   ( 1 * 60 )

const char *client_name = "default_sub"; 	// -c
const char *ip_addr = "35.158.189.129";		// -h 
uint32_t    port = 1883;			// -p
const char *topic = "test";	// -t
static mqtt_broker_handle_t *broker;

/* Static Function */
static MqttResult_t connectMqtt();
static MqttResult_t ReceiveMqttData();
static void parse_options(int argc, char** argv);

int main(int argc, char** argv)
{
    puts("MQTT Subscribe Test Code v0.1 \n");

    if (argc > 1)
    {
        parse_options(argc, argv);
    }

    while (1)
    {
        MqttResult_t retVal;
        retVal = connectMqtt();
        if (retVal == MQTT_OK)
        {
            printf("\n Connected.... Waiting for Publisher Data\n");

            retVal = ReceiveMqttData();
            if (retVal != MQTT_ERROR)
                break;
            printf("\n Disconneted...\n\n\n");
            mqtt_Sleep(100);
        }
        else
        {
            printf("Failed to Connect\n");
        }
    }
}

static MqttResult_t connectMqtt()
{
    //  mqtt_broker_handle_t *broker = mqtt_connect("default_sub","127.0.0.1", 1883);
    MqttResult_t retVal = MQTT_ERROR;
    broker = mqtt_connect(client_name, ip_addr, port);
    if (broker != NULL)
    {
        retVal = mqtt_subscribe(broker, topic, QoS0);
    }
    return retVal;
}

static MqttResult_t ReceiveMqttData()
{
    MqttResult_t retVal = MQTT_ERROR;
    while (1)
    {
        retVal = mqtt_display_message(broker, &putchar);
        if (retVal == MQTT_ERROR)
        {
            break;
        }
    }
    return retVal;
}

static void parse_options(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (strcmp("-c", argv[i]) == 0) {
            printf("client:%s ", argv[++i]);
            client_name = argv[i];
        }
        if (strcmp("-h", argv[i]) == 0) {
            printf("host:%s ", argv[++i]);
            ip_addr = argv[i];
        }
        if (strcmp("-p", argv[i]) == 0) {
            printf("port:%s ", argv[++i]);
            port = atoi(argv[i]);
        }
        if (strcmp("-t", argv[i]) == 0) {
            printf("topic:%s ", argv[++i]);
            topic = argv[i];
        }
    }
    puts("");
}
