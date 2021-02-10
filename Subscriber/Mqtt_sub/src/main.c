#include "mqtt.h"

#define TIME_OUT_IN_SEC   ( 1 * 60 )

const char *client_name = "default_sub"; 	// -c
const char *ip_addr = "35.156.182.231";		// -i //35.156.182.231, 52.29.60.198
uint32_t    port = 1883;			// -p
const char *topic = "thrimurthi_test";	// -t
static mqtt_broker_handle_t *broker;

/* Static Function */
static MqttResult_t connectMqtt();
static MqttResult_t ReceiveMqttData();

int main(int argc, char** argv)
{
    puts("MQTT SUB Test Code");
    while (1)
    {
        MqttResult_t retVal;
        retVal = connectMqtt();
        if (retVal == MQTT_OK)
        {
            printf("\n Connected.... Waiting for PUB Data\n");

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


