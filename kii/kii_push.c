#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

#include "kii.h"
#include "khc.h"
#include "kii_mqtt_task.h"
#include "jkii_wrapper.h"
#include "kii_push_impl.h"
#include "kii_impl.h"


kii_code_t kii_subscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    _reset_khc(kii);
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
    _reset_khc(kii);
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
    _reset_khc(kii);
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
    _reset_khc(kii);
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
    _reset_khc(kii);
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
    _reset_khc(kii);
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
    _reset_khc(kii);
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

    jkii_parse_err_t parse_result = JKII_ERR_INVALID_INPUT;
    jkii_field_t fields[2];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "installationID";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_installation_id->id;
    fields[0].field_copy_buff_size = sizeof(out_installation_id->id);
    fields[1].name = NULL;

    parse_result = _jkii_read_object(kii, buff, buff_size, fields);
    if (parse_result != JKII_ERR_OK) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }

exit:
    return res;
}

/* jkii reads a JSON integer as an int if it fits in one and as a long if not,
 * so these check that a value fits where kii_mqtt_endpoint_t keeps it: a port
 * in 16 bits, the TTL in an unsigned long. */

static int _is_port(int value)
{
    return 0 <= value && value <= 65535;
}

static int _fits_unsigned_long(long value)
{
    return value >= 0;
}

kii_code_t kii_get_mqtt_endpoint(
        kii_t* kii,
        const char* installation_id,
        kii_mqtt_endpoint_t* endpoint)
{
    _reset_khc(kii);
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

    jkii_parse_err_t parse_result = JKII_ERR_INVALID_INPUT;
    jkii_field_t fields[8];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "username";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = endpoint->username;
    fields[0].field_copy_buff_size = 
        sizeof(endpoint->username) / sizeof(endpoint->username[0]);
    fields[1].name = "password";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = endpoint->password;
    fields[1].field_copy_buff_size =
        sizeof(endpoint->password) / sizeof(endpoint->password[0]);
    fields[2].name = "host";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[2].field_copy.string = endpoint->host;
    fields[2].field_copy_buff_size =
        sizeof(endpoint->host) / sizeof(endpoint->host[0]);
    fields[3].name = "mqttTopic";
    fields[3].type = JKII_FIELD_TYPE_STRING;
    fields[3].field_copy.string = endpoint->topic;
    fields[3].field_copy_buff_size =
        sizeof(endpoint->topic) / sizeof(endpoint->topic[0]);
    fields[4].name = "portTCP";
    fields[4].type = JKII_FIELD_TYPE_INTEGER;
    fields[5].name = "portSSL";
    fields[5].type = JKII_FIELD_TYPE_INTEGER;
    fields[6].name = "X-MQTT-TTL";
    fields[6].type = JKII_FIELD_TYPE_LONG;
    fields[7].name = NULL;

    parse_result = _jkii_read_object(kii, buff, buff_size, fields);
    // The TTL is obsolete, and nothing uses it: an endpoint without one is
    // an endpoint all the same, with a TTL of 0. All the rest has to be
    // there, and a TTL that is has to be one.
    int ttl_absent = 0;
    if (parse_result == JKII_ERR_PARTIAL &&
            fields[6].result == JKII_FIELD_ERR_NOT_FOUND) {
        ttl_absent = 1;
        for (int i = 0; i < 6; ++i) {
            if (fields[i].result != JKII_FIELD_ERR_OK) {
                ttl_absent = 0;
            }
        }
    }
    if (parse_result != JKII_ERR_OK && !ttl_absent) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }
    if (_is_port(fields[4].field_copy.int_value) == 0 ||
            _is_port(fields[5].field_copy.int_value) == 0 ||
            (!ttl_absent && _fits_unsigned_long(fields[6].field_copy.long_value) == 0)) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }
    endpoint->port_tcp = (unsigned int)fields[4].field_copy.int_value;
    endpoint->port_ssl = (unsigned int)fields[5].field_copy.int_value;
    endpoint->ttl = ttl_absent ? 0 : (unsigned long)fields[6].field_copy.long_value;

exit:
    return res;
}

/* What the task uses without checking, so that one left out is found when
 * starting it, rather than as a crash in the task. */
static kii_bool_t _push_task_can_run(const kii_t* kii)
{
    const khc* http = &kii->_khc;
    return (kii->_cb_delay_ms != NULL &&
            http->_cb_sock_connect != NULL &&
            http->_cb_sock_send != NULL &&
            http->_cb_sock_recv != NULL &&
            http->_cb_sock_close != NULL &&
            kii->_cb_mqtt_sock_connect != NULL &&
            kii->_cb_mqtt_sock_send != NULL &&
            kii->_cb_mqtt_sock_recv != NULL &&
            kii->_cb_mqtt_sock_close_cb != NULL &&
            kii->_rw_buff != NULL &&
            kii->_mqtt_buffer != NULL) ? KII_TRUE : KII_FALSE;
}

kii_code_t kii_start_push_task(kii_t* kii, unsigned int keep_alive_interval, KII_PUSH_RECEIVED_CB callback, void* userdata)
{
    // A second task would share the first's sockets and buffers.
    if (kii->_task_running == KII_TRUE) {
        return KII_ERR_FAIL;
    }
    // CONNECT carries the keep-alive in 16 bits. A longer one would reach the
    // broker cut down to its low 16 bits while the task pinged on the whole.
    if (keep_alive_interval > 65535) {
        return KII_ERR_FAIL;
    }
    if (kii->_cb_task_create == NULL) {
        return KII_ERR_CREATE_TASK;
    }
    if (callback == NULL || _push_task_can_run(kii) != KII_TRUE) {
        return KII_ERR_FAIL;
    }
    kii->_keep_alive_interval = keep_alive_interval;
    kii->_cb_push_received = callback;
    kii->_push_data = userdata;
    // Before creating it, as it may have finished by the time that returns.
    kii->_task_running = KII_TRUE;
    kii_task_code_t res = kii->_cb_task_create(KII_TASK_NAME_MQTT,
            mqtt_start_task,
            (void*)kii,
            kii->_task_create_data);
    if (res != KII_TASKC_OK) {
        kii->_task_running = KII_FALSE;
        return KII_ERR_CREATE_TASK;
    }
    return KII_ERR_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
