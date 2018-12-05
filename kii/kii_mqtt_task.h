#ifndef __KII_MQTT_TASK_H__
#define __KII_MQTT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "kii.h"

typedef enum { QOS0, QOS1, QOS2 } kii_mqtt_qos;

typedef struct {
    char byte1;
    unsigned long remaining_length;
} kii_mqtt_fixed_header;

typedef struct {
    kii_t* kii;
    kii_installation_id_t ins_id;
    kii_mqtt_endpoint_t endpoint;
    kii_mqtt_task_info info;
    unsigned int elapsed_time_ms;
    unsigned long remaining_message_size;
} mqtt_state_t;

void _init_mqtt_state(kii_t* kii, mqtt_state_t* state);

typedef void (*MQTT_STATE_HANDLER)(mqtt_state_t* state);

void _mqtt_state_install_push(mqtt_state_t* state);
void _mqtt_state_get_endpoint(mqtt_state_t* state);
void _mqtt_state_sock_connect(mqtt_state_t* state);
void _mqtt_state_send_connect(mqtt_state_t* state);
void _mqtt_state_recv_connack(mqtt_state_t* state);
void _mqtt_state_send_subscribe(mqtt_state_t* state);
void _mqtt_state_recv_suback(mqtt_state_t* state);
void _mqtt_state_recv_ready(mqtt_state_t* state);
void _mqtt_state_recv_msg(mqtt_state_t* state);
void _mqtt_state_send_pingreq(mqtt_state_t* state);
void _mqtt_state_reconnect(mqtt_state_t* state);

void* mqtt_start_task(void* sdata);

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
