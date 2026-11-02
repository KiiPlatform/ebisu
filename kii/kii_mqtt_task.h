#ifndef __KII_MQTT_TASK_H__
#define __KII_MQTT_TASK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "kii.h"

typedef struct {
    char byte1;
    unsigned long remaining_length;
} kii_mqtt_fixed_header;

typedef struct {
    kii_t* kii;
    kii_installation_id_t ins_id;
    kii_mqtt_endpoint_t endpoint;
    kii_mqtt_task_info info;
    /** \private
     * The time since the last packet that counts for the keep-alive, in
     * milliseconds, as estimated by adding up the task's waits and receive
     * timeouts. Used when the system has no monotonic clock.
     */
    uint32_t elapsed_time_ms;
    /** \private
     * When that time started, by the system's monotonic clock, if it has one.
     */
    uint64_t elapsed_since_ms;
    unsigned long remaining_message_size;
    /** \private
     * The QoS of the PUBLISH whose fixed header was read last, from its flags.
     */
    unsigned int message_qos;
    /** \private
     * Whether that PUBLISH came while waiting for SUBACK, which is waited
     * for again once it is read.
     */
    kii_bool_t awaiting_suback;
    /** \private
     * Whether the PINGRESP for the last PINGREQ has arrived.
     *
     * A broken connection used to be noticed only when sending failed, which on
     * a half-open TCP connection can take many retransmission timeouts. Missing
     * PINGRESP detects it within one keep-alive interval instead.
     */
    kii_bool_t pingresp_received;
    /** \private
     * Whether the MQTT socket is connected: from a successful connect until
     * the close callback is called, so that the task closes it on its way
     * out, and only then.
     */
    kii_bool_t sock_connected;
    /** \private
     * Whether the broker accepted CONNECT on that connection: from a CONNACK
     * that accepts it until the connection is closed, so that the task says
     * DISCONNECT before closing it on its way out.
     */
    kii_bool_t mqtt_connected;
    /** \private
     * How long the wait before the next attempt to reach the cloud is based
     * on, in milliseconds. It doubles with each failure until a connection is
     * subscribed.
     */
    uint32_t retry_wait_ms;
    /** \private
     * What is left of the wait, in milliseconds. The task checks whether to
     * continue during it.
     */
    uint32_t wait_left_ms;
    /** \private
     * Attempts in a row to connect to the broker that have failed.
     */
    uint32_t connect_failures;
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
