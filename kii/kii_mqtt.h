#ifndef KII_MQTT_H
#define KII_MQTT_H

#include "kii.h"

enum QoS { QOS0, QOS1, QOS2 };

typedef enum
{
    KII_MQTT_PREPARING_ENDPOINT = 0,
    KII_MQTT_SUBSCRIBING_TOPIC = 1,
    KII_MQTT_READY = 2
} kii_mqtt_state;

int _mqtt_encode(char* buf, int length);
int _mqtt_decode(char* buf, int* value);
int _mqtt_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval);
int _mqtt_subscribe(kii_t* kii, const char* topic, enum QoS qos);
int _mqtt_pingreq(kii_t* kii);

int _mqtt_recvmsg(
    kii_t* kii,
    kii_mqtt_endpoint_t* endpoint);
void* _mqtt_start_recvmsg_task(void* sdata);
void* _mqtt_start_pinreq_task(void* sdata);

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
