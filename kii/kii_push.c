#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "kii.h"
#include "kii_mqtt.h"
#include "kii_core.h"
#include "kii_json.h"

#define KII_PUSH_PING_ENABLE 1
#define KII_PUSH_INSTALLATIONID_SIZE 64
#define KII_PUSH_TOPIC_HEADER_SIZE 8

#if(KII_PUSH_PING_ENABLE)
#define KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE 30
#else
#define KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE 0
#endif

typedef enum
{
    KIIPUSH_ENDPOINT_READY = 0,
    KIIPUSH_ENDPOINT_UNAVAILABLE = 1,
    KIIPUSH_ENDPOINT_ERROR = 2
} kiiPush_endpointState_e;

#define KIIPUSH_TASK_STK_SIZE 8
static unsigned int mKiiPush_taskStk[KIIPUSH_TASK_STK_SIZE];
#if(KII_PUSH_PING_ENABLE)
#define KIIPUSH_PINGREQ_TASK_STK_SIZE 8
static unsigned int mKiiPush_pingReqTaskStk[KIIPUSH_PINGREQ_TASK_STK_SIZE];
#endif

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
    kii_json_t kii_json;

    core_err = kii_core_install_thing_push(&kii->kii_core, development);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code)
    {
        goto exit;
    }

    buf = kii->kii_core.response_body;
    buf_size = kii->kii_core.http_context.buffer_size -
        (kii->kii_core.response_body - kii->kii_core.http_context.buffer);
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

    memset(&kii_json, 0, sizeof(kii_json));

    parse_result = kii_json_read_object(&kii_json, buf, buf_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get json value: %d\n",
                        parse_result));
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

static kiiPush_endpointState_e kiiPush_retrieveEndpoint(kii_t* kii, const char* installation_id, kii_mqtt_endpoint_t* endpoint)
{
    char* buf = NULL;
    size_t buf_size = 0;
    kiiPush_endpointState_e ret = KIIPUSH_ENDPOINT_ERROR;
    kii_json_parse_result_t parse_result = KII_JSON_PARSE_INVALID_INPUT;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[8];
    kii_json_t kii_json;

    core_err = kii_core_get_mqtt_endpoint(&kii->kii_core, installation_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code == 503)
    {
        ret = KIIPUSH_ENDPOINT_UNAVAILABLE;
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code)
    {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }

    buf = kii->kii_core.response_body;
    buf_size = kii->kii_core.http_context.buffer_size -
        (kii->kii_core.response_body - kii->kii_core.http_context.buffer);
    if (buf == NULL) {
        ret = KIIPUSH_ENDPOINT_ERROR;
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

    memset(&kii_json, 0, sizeof(kii_json));

    parse_result = kii_json_read_object(&kii_json, buf, buf_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        ret = KIIPUSH_ENDPOINT_ERROR;
        goto exit;
    }
    endpoint->port_tcp = fields[4].field_copy.int_value;
    endpoint->port_ssl = fields[5].field_copy.int_value;
    endpoint->ttl = fields[6].field_copy.long_value;
    ret = KIIPUSH_ENDPOINT_READY;

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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
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

static void* kiiPush_recvMsgTask(void* sdata)
{
    int remainingLen;
    int byteLen;
    int topicLen;
    int totalLen;
    char* p;
    int bytes = 0;
    size_t rcvdCounter = 0;
    KII_PUSH_RECEIVED_CB callback;
    kiiPush_endpointState_e endpointState;
    kii_t* kii;
    kii_mqtt_endpoint_t endpoint;
    char installation_id[KII_PUSH_INSTALLATIONID_SIZE + 1];

    memset(installation_id, 0x00, sizeof(installation_id));
    memset(&endpoint, 0x00, sizeof(kii_mqtt_endpoint_t));

    kii = (kii_t*) sdata;
    callback = kii->push_received_cb;
    for(;;)
    {
        if(kii->_mqtt_endpoint_ready == 0)
        {
            if(kiiPush_install(kii, KII_FALSE, installation_id,
                    sizeof(installation_id) / sizeof(installation_id[0])) != 0)
            {
                kii->delay_ms_cb(1000);
                continue;
            }

            do
            {
                kii->delay_ms_cb(1000);
                endpointState = kiiPush_retrieveEndpoint(kii, installation_id, &endpoint);
            }
            while(endpointState == KIIPUSH_ENDPOINT_UNAVAILABLE);

            if(endpointState != KIIPUSH_ENDPOINT_READY)
            {
                continue;
            }
            M_KII_LOG(kii->kii_core.logger_cb("installationID:%s\r\n", installation_id));
            M_KII_LOG(kii->kii_core.logger_cb("mqttTopic:%s\r\n", endpoint.topic));
            M_KII_LOG(kii->kii_core.logger_cb("host:%s\r\n", endpoint.host));
            M_KII_LOG(kii->kii_core.logger_cb("username:%s\r\n", endpoint.username));
            M_KII_LOG(kii->kii_core.logger_cb("password:%s\r\n", endpoint.password));
            if(kiiMQTT_connect(kii, &endpoint, KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE) < 0)
            {
                continue;
            }
            else if(kiiMQTT_subscribe(kii, endpoint.topic, QOS0) < 0)
            {
                continue;
            }
            else
            {
                kii->_mqtt_endpoint_ready = 1;
            }
        }
        else
        {
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
                        kii->_mqtt_endpoint_ready = 0;
                        continue;
                    }
                    if(byteLen > 0)
                    {
                        totalLen =
                            remainingLen + byteLen + 1; /* fixed head byte1+remaining length bytes + remaining bytes*/
                    }
                    else
                    {
                        M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt decode error\r\n"));
                        kii->_mqtt_endpoint_ready = 0;
                        continue;
                    }
                    if(totalLen > kii->mqtt_buffer_size)
                    {
                        M_KII_LOG(kii->kii_core.logger_cb("kii-error: mqtt buffer overflow\r\n"));
                        kii->_mqtt_endpoint_ready = 0;
                        continue;
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
                            kii->_mqtt_endpoint_ready = 0;
                            continue;
                        }
                    }
                    else
                    {
                        M_KII_LOG(kii->kii_core.logger_cb("kii_error: mqtt receive data error\r\n"));
                        kii->_mqtt_endpoint_ready = 0;
                        continue;
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
                kii->_mqtt_endpoint_ready = 0;
            }
        }
    }
    return NULL;
}

#if(KII_PUSH_PING_ENABLE)
static void* kiiPush_pingReqTask(void* sdata)
{
    kii_t* kii;

    kii = (kii_t*)sdata;
    for(;;)
    {
        if(kii->_mqtt_endpoint_ready == 1)
        {
            kiiMQTT_pingReq(kii);
        }
        kii->delay_ms_cb(KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE * 1000);
    }
    return NULL;
}
#endif

int kii_push_start_routine(kii_t* kii, unsigned int recvMsgtaskPrio, unsigned int pingReqTaskPrio, KII_PUSH_RECEIVED_CB callback)
{
    kii->push_received_cb = callback;
    kii->task_create_cb(NULL,
            kiiPush_recvMsgTask,
            (void*)kii,
            (void*)mKiiPush_taskStk,
            KIIPUSH_TASK_STK_SIZE * sizeof(unsigned char),
            recvMsgtaskPrio);
#if(KII_PUSH_PING_ENABLE)
    kii->task_create_cb(NULL,
            kiiPush_pingReqTask,
            (void*)kii,
            (void*)mKiiPush_pingReqTaskStk,
            KIIPUSH_PINGREQ_TASK_STK_SIZE * sizeof(unsigned char),
            pingReqTaskPrio);
#endif
    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
