/*
- * COPYRIGHT (C) 2021 Electosh Solution Pvt. Ltd - All Rights Reserved
 *
 *      %name      : main.c %
 *      Author     : Thrimurthi M
 *      Modified By: Thrimurthi M
 */

#include "mqtt.h"

const char *client_name = "default_pub"; 	// -c
const char *ip_addr = "35.158.189.129";		// -h
uint32_t    port = 1883;			// -p
const char *topic = "test";	// -t
char        msg[128] = { 0 };           // -m 

static void parse_options(int argc, char** argv);

int main(int argc, char** argv)
{
    puts("MQTT Publish Test Code v0.1 \n");

    if (argc > 1)
    {
        parse_options(argc, argv);
    }

    mqtt_broker_handle_t *broker = mqtt_connect(client_name, ip_addr, port);


    if (broker == NULL)
    {
        puts("Failed to connect");
    }
    else
    {
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
            mqtt_Sleep(200); // Sleep Some time
        }
        mqtt_disconnect(broker);
    }
}

static void parse_options(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp("-c", argv[i]) == 0)
        {
            printf("client:%s ", argv[++i]);
            client_name = argv[i];
        }
        if (strcmp("-h", argv[i]) == 0)
        {
            printf("host:%s ", argv[++i]);
            ip_addr = argv[i];
        }
        if (strcmp("-p", argv[i]) == 0)
        {
            printf("port:%s ", argv[++i]);
            port = atoi(argv[i]);
        }
        if (strcmp("-t", argv[i]) == 0)
        {
            printf("topic:%s ", argv[++i]);
            topic = argv[i];
        }
    }
    puts("");
}

