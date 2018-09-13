#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "kii.h"
#include "khc.h"
#include "kii_mqtt.h"
#include "kii_json_utils.h"
#include "kii_push_impl.h"

kii_code_t kii_subscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    kii_code_t res = _subscribe_bucket(kii, bucket);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if (resp_code != 204 && resp_code != 409) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}

kii_code_t kii_unsubscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{

    kii_code_t res = _unsubscribe_bucket(kii, bucket);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}

kii_code_t kii_subscribe_topic(kii_t* kii, const kii_topic_t* topic)
{

    kii_code_t res = _subscribe_topic(kii, topic);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if (resp_code != 204 && resp_code != 409) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}

kii_code_t kii_unsubscribe_topic(kii_t* kii, const kii_topic_t* topic)
{

    kii_code_t res = _unsubscribe_topic(kii, topic);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if (resp_code != 204 && resp_code != 409) {
        res = KII_ERR_RESP_STATUS;
    }

exit:
    return res;
}

kii_code_t kii_put_topic(kii_t* kii, const kii_topic_t* topic)
{

    kii_code_t res = _put_topic(kii, topic);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if (resp_code == 204 || resp_code == 409) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}

kii_code_t kii_delete_topic(kii_t* kii, const kii_topic_t* topic)
{

    kii_code_t res = _delete_topic(kii, topic);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }
exit:
    return res;
}

kii_code_t kii_install_push(
        kii_t* kii,
        kii_bool_t development,
        char* installation_id,
        size_t installation_id_len)
{

    kii_code_t res = _install_push(kii, development);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        res = KII_ERR_FAIL;
        goto exit;
    }

    kii_json_parse_result_t parse_result = KII_JSON_PARSE_INVALID_INPUT;
    kii_json_field_t fields[2];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "installationID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = installation_id;
    fields[0].field_copy_buff_size = installation_id_len;
    fields[1].name = NULL;

    parse_result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }

exit:
    return res;
}

kiiPush_retrieveEndpointResult kiiPush_retrieveEndpoint(kii_t* kii, const char* installation_id, kii_mqtt_endpoint_t* endpoint)
{
    kiiPush_retrieveEndpointResult ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;

    kii_code_t res = _get_mqtt_endpoint(kii, installation_id);
    if (res != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code == 503)
    {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_RETRY;
        goto exit;
    }
    if(resp_code < 200 || 300 <= resp_code)
    {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
        goto exit;
    }

    // TODO: get buffer and its length.
    char* buff = NULL;
    size_t buff_size = 0;
    if (buff == NULL) {
        ret = KIIPUSH_RETRIEVE_ENDPOINT_ERROR;
        goto exit;
    }

    kii_json_parse_result_t parse_result = KII_JSON_PARSE_INVALID_INPUT;
    kii_json_field_t fields[8];
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

    parse_result = prv_kii_json_read_object(kii, buff, buff_size, fields);
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
    kii->mqtt_sock_recv_cb(&kii->mqtt_sock_recv_ctx, kii->mqtt_buffer, 2, &rcvdCounter);
    if(rcvdCounter == 2)
    {
        if((kii->mqtt_buffer[0] & 0xf0) == 0x30)
        {
            rcvdCounter = 0;
            kii->mqtt_sock_recv_cb(&kii->mqtt_sock_recv_ctx, kii->mqtt_buffer+2, KII_PUSH_TOPIC_HEADER_SIZE, &rcvdCounter);
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
                kii->mqtt_sock_recv_cb(&(kii->mqtt_sock_recv_ctx), kii->mqtt_buffer + bytes, totalLen - bytes, &rcvdCounter);
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

                    if(kii_install_push(kii, KII_FALSE, installation_id,
                                    sizeof(installation_id) /
                                        sizeof(installation_id[0])) != KII_ERR_OK)
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
                    /* Receiving notificaiton is failed. Retry subscribing. */
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
