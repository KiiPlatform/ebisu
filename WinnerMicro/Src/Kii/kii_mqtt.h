#ifndef KII_MQTT_H
#define KII_MQTT_H

enum QoS { QOS0, QOS1, QOS2 };


int KiiMQTT_encode(char* buf, int length);
int KiiMQTT_decode(char* buf, int *value);
int KiiMQTT_connect(unsigned short keepAliveInterval);
int KiiMQTT_subscribe(enum QoS qos);
int KiiMQTT_pingReq(void);

#endif

