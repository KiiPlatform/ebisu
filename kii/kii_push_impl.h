#ifndef __kii_push_impl__
#define __kii_push_impl__

#include "kii.h"

#define KII_PUSH_INSTALLATIONID_SIZE 64
#define KII_PUSH_TOPIC_HEADER_SIZE 8

typedef enum
{
    KII_MQTT_PREPARING_ENDPOINT = 0,
    KII_MQTT_SUBSCRIBING_TOPIC = 1,
    KII_MQTT_READY = 2
} kii_mqtt_state;

kii_code_t _install_push(
        kii_t* kii,
        kii_bool_t development);

kii_code_t _get_mqtt_endpoint(
        kii_t* kii,
        const char* installation_id);

kii_code_t _subscribe_bucket(
        kii_t* kii,
        const kii_bucket_t* bucket);

kii_code_t _unsubscribe_bucket(
        kii_t* kii,
        const kii_bucket_t* bucket);

kii_code_t _subscribe_topic(
        kii_t* kii,
        const kii_topic_t* topic);

kii_code_t _unsubscribe_topic(
        kii_t* kii,
        const kii_topic_t* topic);

kii_code_t _put_topic(
        kii_t* kii,
        const kii_topic_t* topic);

kii_code_t _delete_topic(
        kii_t* kii,
        const kii_topic_t* topic);
#endif