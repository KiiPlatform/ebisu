#ifndef __KII_MQTT_TASK_H__
#define __KII_MQTT_TASK_H__

#include "kii.h"

typedef enum
{
    KII_MQTT_ST_INSTALL_PUSH,
    KII_MQTT_ST_GET_ENDPOINT,
    KII_MQTT_ST_SOCK_CONNECT,
    KII_MQTT_ST_SEND_CONNECT,
    KII_MQTT_ST_RECV_CONNACK,
    KII_MQTT_ST_SEND_SUBSCRIBE,
    KII_MQTT_ST_RECV_SUBACK,
    KII_MQTT_ST_RECV_READY,
    KII_MQTT_ST_RECV_MSG,
    KII_MQTT_ST_SEND_PINGREQ,
    KII_MQTT_ST_RECONNECT,
    KII_MQTT_ST_ERR_EXIT
} kii_mqtt_task_state;

typedef struct {
    char byte1;
    unsigned long remaining_length;
} kii_mqtt_fixed_header;

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
