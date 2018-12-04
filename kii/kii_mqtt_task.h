#ifndef __KII_MQTT_TASK_H__
#define __KII_MQTT_TASK_H__

#include "kii.h"

typedef enum { QOS0, QOS1, QOS2 } kii_mqtt_qos;

typedef struct {
    char byte1;
    unsigned long remaining_length;
} kii_mqtt_fixed_header;

void* mqtt_start_task(void* sdata);

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
