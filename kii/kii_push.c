#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include "kii.h"
#include "khc.h"
#include "kii_mqtt.h"
#include "kii_json_utils.h"
#include "kii_push_impl.h"
#include "kii_impl.h"

kii_code_t kii_subscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
   khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
   khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

    kii_code_t res = _put_topic(kii, topic);
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

kii_code_t kii_delete_topic(kii_t* kii, const kii_topic_t* topic)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
        kii_installation_id_t* out_installation_id)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

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
    fields[0].field_copy.string = out_installation_id->id;
    fields[0].field_copy_buff_size = sizeof(out_installation_id->id);
    fields[1].name = NULL;

    parse_result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (parse_result != KII_JSON_PARSE_SUCCESS) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }

exit:
    return res;
}

kii_code_t kii_get_mqtt_endpoint(
    kii_t* kii,
    const char* installation_id,
    kii_mqtt_endpoint_t* endpoint)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

    kii_code_t res = _get_mqtt_endpoint(kii, installation_id);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code)
    {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

    // TODO: get buffer and its length.
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        res = KII_ERR_FAIL;
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
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }
    endpoint->port_tcp = fields[4].field_copy.int_value;
    endpoint->port_ssl = fields[5].field_copy.int_value;
    endpoint->ttl = fields[6].field_copy.long_value;

exit:
    return res;
}

kii_code_t kii_start_push_routine(kii_t* kii, unsigned int keep_alive_interval, KII_PUSH_RECEIVED_CB callback)
{
    kii->_keep_alive_interval = keep_alive_interval;
    kii->push_received_cb = callback;
    kii->task_create_cb(KII_TASK_NAME_RECV_MSG,
        _mqtt_start_recvmsg_task,
        (void*)kii);
    if (keep_alive_interval > 0) {
        kii->task_create_cb(KII_TASK_NAME_PING_REQ,
            _mqtt_start_pinreq_task,
            (void*)kii);
    }
    return KII_ERR_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
