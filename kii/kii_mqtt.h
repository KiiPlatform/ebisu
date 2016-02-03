#ifndef KII_MQTT_H
#define KII_MQTT_H

#include "kii_core.h"
#include "kii.h"

enum QoS { QOS0, QOS1, QOS2 };

typedef struct kii_mqtt_endpoint_t {
    char username[64];
    char password[128];
    char topic[64];
    char host[64];
    unsigned int port_tcp;
    unsigned int port_ssl;
    unsigned long ttl;
} kii_mqtt_endpoint_t;

int kiiMQTT_encode(char* buf, int length);
int kiiMQTT_decode(char* buf, int* value);
int kiiMQTT_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval);
int kiiMQTT_subscribe(kii_t* kii, const char* topic, enum QoS qos);
int kiiMQTT_pingReq(kii_t* kii);

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
