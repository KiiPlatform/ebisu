#ifndef KII_MQTT_H
#define KII_MQTT_H

#include "kii-core/kii.h"
#include "kii-core/kii_mqtt.h"

enum QoS { QOS0, QOS1, QOS2 };

int kiiMQTT_encode(char* buf, int length);
int kiiMQTT_decode(char* buf, int* value);
int kiiMQTT_connect(kii_t* kii, kii_mqtt_endpoint_t* endpoint, unsigned short keepAliveInterval);
int kiiMQTT_subscribe(kii_t* kii, const char* topic, enum QoS qos);
int kiiMQTT_pingReq(kii_t* kii);

#endif
