#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "kii.h"
#include "kii_mqtt.h"
#include "kii_core.h"
#include "kii_json_utils.h"

#define KII_PUSH_INSTALLATIONID_SIZE 64
#define KII_PUSH_TOPIC_HEADER_SIZE 8

typedef enum
{
    KIIPUSH_RETRIEVE_ENDPOINT_SUCCESS = 0,
    KIIPUSH_RETRIEVE_ENDPOINT_RETRY = 1,
    KIIPUSH_RETRIEVE_ENDPOINT_ERROR = 2
} kiiPush_retrieveEndpointResult;

typedef enum
{
    KIIPUSH_PREPARING_ENDPOINT = 0,
    KIIPUSH_SUBSCRIBING_TOPIC = 1,
    KIIPUSH_READY = 2
} kiiPush_state;

static int kiiPush_install(
        kii_t* kii,
        kii_bool_t development,
        char* installation_id,
        size_t installation_id_len)
{
    char* buf = NULL;
    size_t buf_size = 0;
    int ret = -1;
    kii_json_parse_result_t parse_result = KII_JSON_PARSE_INVALID_INPUT;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[2];

    core_err = kii_core_install_thing_push(&kii->kii_core, development);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code)
    {
        goto exit;
    }

    buf = kii->kii_core.response_body;
    buf_size = strlen(kii->kii_core.response_body);
    if (buf == NULL) {
        ret = -1;
        goto exit;
    }

    memset(fields, 0, sizeof(fields));
    fields[0].name = "installationID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = installation_id;
    fields[0].field_copy_buff_size = installation_id_len;
    fields[1].name = NULL;

    parse_result = prv_kii_json_read_object(kii, buf, buf_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get json value: %d\n",
                        parse_result));
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

static kiiPush_retrieveEndpointResult kiiPush_retrieveEndpoint(kii_t* kii, const char* installation_id, kii_mqtt_endpoint_t* endpoint)
{
    char* buf = NULL;
    size_t buf_size = 0;
    kiiPush_retrieveEndpointResult ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
    kii_json_parse_result_t parse_result = KII_JSON_PARSE_INVALID_INPUT;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[8];

    core_err = kii_core_get_mqtt_endpoint(&kii->kii_core, installation_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code == 503)
    {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_RETRY;
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code)
    {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
        goto exit;
    }

    buf = kii->kii_core.response_body;
    buf_size = strlen(kii->kii_core.response_body);
    if (buf == NULL) {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
        goto exit;
    }

    memset(fields, 0, sizeof(fields));
    fields[0].name = "username";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = endpoint->username;
    fields[0].field_copy_buff_size = 
        sizeof(endpoint->username) / sizeof(endpoint->username[0]);
    fields[1].name = "password";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = endpoint->password;
    fields[1].field_copy_buff_size =
        sizeof(endpoint->password) / sizeof(endpoint->password[0]);
    fields[2].name = "host";
    fields[2].type = KII_JSON_FIELD_TYPE_STRING;
    fields[2].field_copy.string = endpoint->host;
    fields[2].field_copy_buff_size =
        sizeof(endpoint->host) / sizeof(endpoint->host[0]);
    fields[3].name = "mqttTopic";
    fields[3].type = KII_JSON_FIELD_TYPE_STRING;
    fields[3].field_copy.string = endpoint->topic;
    fields[3].field_copy_buff_size =
        sizeof(endpoint->topic) / sizeof(endpoint->topic[0]);
    fields[4].name = "portTCP";
    fields[4].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[5].name = "portSSL";
    fields[5].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[6].name = "X-MQTT-TTL";
    fields[6].type = KII_JSON_FIELD_TYPE_LONG;
    fields[7].name = NULL;

    parse_result = prv_kii_json_read_object(kii, buf, buf_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
        goto exit;
    }
    endpoint->port_tcp = fields[4].field_copy.int_value;
    endpoint->port_ssl = fields[5].field_copy.int_value;
    endpoint->ttl = fields[6].field_copy.long_value;
    ret = KIIPUSH_RETRIEVE_ENDPOINT_SUCCESS;

exit:
    return ret;
}

int kii_push_subscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    int ret = -1;

    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_subscribe_bucket(&kii->kii_core, bucket);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->kii_core.response_code == 204 || kii->kii_core.response_code == 409) {
        ret = 0;
    }
exit:
    return ret;
}

int kii_push_unsubscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    int ret = -1;

    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_unsubscribe_bucket(&kii->kii_core, bucket);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
	ret = 0;
exit:
    return ret;
}

int kii_push_subscribe_topic(kii_t* kii, const kii_topic_t* topic)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_subscribe_topic(&kii->kii_core, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->kii_core.response_code == 204 || kii->kii_core.response_code == 409) {
        ret = 0;
    }
exit:
    return ret;
}

int kii_push_unsubscribe_topic(kii_t* kii, const kii_topic_t* topic)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_unsubscribe_topic(&kii->kii_core, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
	ret = 0;
exit:
    return ret;
}

int kii_push_create_topic(kii_t* kii, const kii_topic_t* topic)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_create_topic(&kii->kii_core, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if (kii->kii_core.response_code == 204 || kii->kii_core.response_code == 409) {
        ret = 0;
    }
exit:
    return ret;
}

int kii_push_delete_topic(kii_t* kii, const kii_topic_t* topic)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_delete_topic(&kii->kii_core, topic);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

static int kiiPush_receivePushNotification(
        kii_t* kii,
        kii_mqtt_endpoint_t* endpoint)
{
    int remainingLen;
    int byteLen;
    int topicLen;
    size_t totalLen;
    char* p;
    size_t bytes = 0;
    size_t rcvdCounter = 0;
    KII_PUSH_RECEIVED_CB callback;

    callback = kii->push_received_cb;
    memset(kii->mqtt_buffer, 0, kii->mqtt_buffer_size);
    M_KII_LOG(kii->kii_core.logger_cb("readPointer: %d\r\n", kii->mqtt_buffer + bytes));

    rcvdCounter = 0;
    kii->mqtt_socket_recv_cb(&kii->mqtt_socket_context, kii->mqtt_buffer, 2, &rcvdCounter);
    if(rcvdCounter == 2)
    {
        if((kii->mqtt_buffer[0] & 0xf0) == 0x30)
        {
            rcvdCounter = 0;
            kii->mqtt_socket_recv_cb(&kii->mqtt_socket_context, kii->mqtt_buffer+2, KII_PUSH_TOPIC_HEADER_SIZE, &rcvdCounter);
            if(rcvdCounter == KII_PUSH_TOPIC_HEADER_SIZE)
            {
                byteLen = kiiMQTT_decode(&kii->mqtt_buffer[1], &remainingLen);
            }
            else
            {
                M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt decode error\r\n"));
                return -1;
            }
            if(byteLen > 0)
            {
                totalLen =
                  remainingLen + byteLen + 1; /* fixed head byte1+remaining length bytes + remaining bytes*/
            }
            else
            {
                M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt decode error\r\n"));
                return -1;
            }
            if(totalLen > kii->mqtt_buffer_size)
            {
                M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt buffer overflow\r\n"));
                return -1;
            }
            M_KII_LOG(kii->kii_core.logger_cb("decode byteLen=%d, remainingLen=%d\r\n", byteLen, remainingLen));
            bytes = rcvdCounter + 2;
            M_KII_LOG(kii->kii_core.logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
            while(bytes < totalLen)
            {
                M_KII_LOG(kii->kii_core.logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
                M_KII_LOG(kii->kii_core.logger_cb("lengthToLead: %d\r\n", totalLen - bytes));
                M_KII_LOG(kii->kii_core.logger_cb("readPointer: %d\r\n", kii->mqtt_buffer + bytes));
                /*kii->socket_recv_cb(&(kii->socket_context), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);*/
                rcvdCounter = 0;
                kii->mqtt_socket_recv_cb(&(kii->mqtt_socket_context), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);
                M_KII_LOG(kii->kii_core.logger_cb("totalLen: %d, bytes: %d\r\n", totalLen, bytes));
                if(rcvdCounter > 0)
                {
                    bytes += rcvdCounter;
                    M_KII_LOG(kii->kii_core.logger_cb("success read. totalLen: %d, bytes: %d\r\n", totalLen, bytes));
                }
                else
                {
                    bytes = -1;
                    M_KII_LOG(kii->kii_core.logger_cb("failed to read. totalLen: %d, bytes: %d\r\n", totalLen, bytes));
                    break;
                }
            }
            M_KII_LOG(kii->kii_core.logger_cb("bytes:%d, totalLen:%d\r\n", bytes, totalLen));
            if(bytes >= totalLen)
            {
                p = kii->mqtt_buffer;
                p++; /* skip fixed header byte1*/
                p += byteLen; /* skip remaining length bytes*/
                topicLen = p[0] * 256 + p[1]; /* get topic length*/
                p = p + 2; /* skip 2 topic length bytes*/
                p = p + topicLen; /* skip topic*/
                if((remainingLen - 2 - topicLen) > 0)
                {
                    M_KII_LOG(kii->kii_core.logger_cb("Successfully Recieved Push %s\n", p));
                    callback(kii, p, remainingLen - 2 - topicLen);
                }
                else
                {
                    M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt topic length error\r\n"));
                    return -1;
                }
            }
            else
            {
                M_KII_LOG(kii->kii_core.logger_cb("kii_error: mqtt receive data error\r\n"));
                return -1;
            }
        }
#if(KII_PUSH_PING_ENABLE)
        else if((kii->mqtt_buffer[0] & 0xf0) == 0xd0)
        {
            M_KII_LOG(kii->kii_core.logger_cb("ping resp\r\n"));
        }
#endif
    }
    else
    {
        M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt receive data error\r\n"));
        return -1;
    }
    return 0;
}

static void* kiiPush_recvMsgTask(void* sdata)
{
    kii_t* kii;
    kii_mqtt_endpoint_t endpoint;
    kiiPush_state pushState = KIIPUSH_PREPARING_ENDPOINT;

    memset(&endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));

    kii = (kii_t*) sdata;
    for(;;)
    {
        switch(pushState)
        {
            case KIIPUSH_PREPARING_ENDPOINT:
                {
                    char installation_id[KII_PUSH_INSTALLATIONID_SIZE + 1];
                    kiiPush_retrieveEndpointResult result;

                    if(kiiPush_install(kii, KII_FALSE, installation_id,
                                    sizeof(installation_id) /
                                        sizeof(installation_id[0])) != 0)
                    {
                        M_KII_LOG(kii->kii_core.logger_cb(
                                "kii-error: mqtt installation error\r\n"));
                        kii->delay_ms_cb(1000);
                        continue;
                    }

                    do
                    {
                        kii->delay_ms_cb(1000);
                        result = kiiPush_retrieveEndpoint(kii, installation_id,
                                &endpoint);
                    }
                    while(result == KIIPUSH_RETRIEVE_ENDPOINT_RETRY);

                    if(result != KIIPUSH_RETRIEVE_ENDPOINT_SUCCESS)
                    {
                        M_KII_LOG(kii->kii_core.logger_cb(
                                "kii-error: mqtt retrive error\r\n"));
                        kii->delay_ms_cb(1000);
                        continue;
                    }

                    pushState = KIIPUSH_SUBSCRIBING_TOPIC;

                    M_KII_LOG(kii->kii_core.logger_cb("installationID:%s\r\n",
                                    installation_id));
                    M_KII_LOG(kii->kii_core.logger_cb("mqttTopic:%s\r\n",
                                    endpoint.topic));
                    M_KII_LOG(kii->kii_core.logger_cb("host:%s\r\n",
                                    endpoint.host));
                    M_KII_LOG(kii->kii_core.logger_cb("username:%s\r\n",
                                    endpoint.username));
                    M_KII_LOG(kii->kii_core.logger_cb("password:%s\r\n",
                                    endpoint.password));
                }
                break;
            case KIIPUSH_SUBSCRIBING_TOPIC:
                if (kiiMQTT_connect(kii, &endpoint,
                                KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS) != 0)
                {
                    M_KII_LOG(kii->kii_core.logger_cb(
                            "kii-error: mqtt connect error\r\n"));
                    pushState = KIIPUSH_PREPARING_ENDPOINT;
                    continue;
                }
                kii->_mqtt_connected = 1;

                if(kiiMQTT_subscribe(kii, endpoint.topic, QOS0) < 0)
                {
                    M_KII_LOG(kii->kii_core.logger_cb(
                            "kii-error: mqtt subscribe error\r\n"));
                    kii->_mqtt_connected = 0;
                    pushState = KIIPUSH_PREPARING_ENDPOINT;
                    continue;
                }

                pushState = KIIPUSH_READY;
                break;
            case KIIPUSH_READY:
                if(kiiPush_receivePushNotification(kii, &endpoint) != 0)
                {
                    // Receiving notificaiton is failed. Retry subscribing.
                    kii->_mqtt_connected = 0;
                    pushState = KIIPUSH_SUBSCRIBING_TOPIC;
                }
                break;
        }
    }
    return NULL;
}

#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
static void* kiiPush_pingReqTask(void* sdata)
{
    kii_t* kii;

    kii = (kii_t*)sdata;
    for(;;)
    {
        if(kii->_mqtt_connected == 1)
        {
            kiiMQTT_pingReq(kii);
        }
        kii->delay_ms_cb(KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS * 1000);
    }
    return NULL;
}
#endif

int kii_push_start_routine(kii_t* kii, KII_PUSH_RECEIVED_CB callback)
{
    kii->push_received_cb = callback;
    kii->task_create_cb(KII_TASK_NAME_RECV_MSG,
            kiiPush_recvMsgTask,
            (void*)kii);
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
    kii->task_create_cb(KII_TASK_NAME_PING_REQ,
            kiiPush_pingReqTask,
            (void*)kii);
#endif
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
