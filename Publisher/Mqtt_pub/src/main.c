#include "mqtt.h"

const char *client_name = "default_pub"; 	// -c
const char *ip_addr = "35.156.182.231";		// -i
uint32_t    port = 1883;			// -p
const char *topic = "thrimurthi_test";	// -t
uint32_t    count = 10;			// -n


int main(int argc, char** argv)
{
    puts("MQTT PUB Test Code");

    while (1)
    {
        mqtt_broker_handle_t *broker = mqtt_connect(client_name, ip_addr, port);


        if (broker == NULL)
        {
            puts("Failed to connect");
        }
        else
        {
            char msg[128];
            while (1)
            {
                printf("\n Enter Message to Transmit:");
                scanf("%[^\n]s", msg);
                getchar();

                if (mqtt_publish(broker, topic, msg, QoS1) == MQTT_ERROR)
                {
                    printf("publish failed\n");
                    break;
                }
                else
                {
                    printf("Sent messages: %s \n", msg);
                }
                mqtt_Sleep(1000);
            }
            mqtt_disconnect(broker);
        }
    }
    getchar();
}

