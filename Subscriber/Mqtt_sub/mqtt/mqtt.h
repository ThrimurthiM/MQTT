/*
- * COPYRIGHT (C) 2021 Electosh Solution Pvt. Ltd - All Rights Reserved
 *
 *      %name      : mqtt.h %
 *      Author     : Thrimurthi M
 *      Modified By: Thrimurthi M
 */

#ifndef mqtt_h
#define mqtt_h

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

typedef enum
{
    MQTT_ERROR = -1,
    MQTT_OK    = 0,
    MQTT_CONNECTION_TIMEOUT = 1
}MqttResult_t;

typedef struct mqtt_broker_handle mqtt_broker_handle_t;

typedef enum {QoS0, QoS1, QoS2} QoS;

mqtt_broker_handle_t* mqtt_connect(const char* client, const char * server_ip, uint32_t port);
MqttResult_t mqtt_disconnect(mqtt_broker_handle_t *broker);

MqttResult_t mqtt_publish(mqtt_broker_handle_t *broker, const char *topic, const char *msg, QoS qos);

MqttResult_t mqtt_subscribe(mqtt_broker_handle_t *broker, const char *topic, QoS qos);
MqttResult_t mqtt_display_message(mqtt_broker_handle_t *broker, int (*print)(int));
void mqtt_Sleep(uint32_t milliSeconds);


#endif
