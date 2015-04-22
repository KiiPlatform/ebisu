#ifndef KII_MQTT_H
#define KII_MQTT_H

#include "kii-core/kii.h"
#include "kii-core/kii_mqtt.h"

enum QoS { QOS0, QOS1, QOS2 };

int KiiMQTT_encode(char* buf, int length);
int KiiMQTT_decode(char* buf, int* value);
int KiiMQTT_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval);
int KiiMQTT_subscribe(kii_t* kii, const char* topic, enum QoS qos);
int KiiMQTT_pingReq(kii_t* kii);

#endif
