#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdint.h>

#include "kii.h"
#include "khc.h"
#include "kii_mqtt_task.h"
#include "kii_push_impl.h"
#include "kii_impl.h"
#include <ebisu_mem.h>


kii_code_t kii_subscribe_bucket(kii_t* kii, const kii_bucket_t* bucket)
{
    _reset_khc(kii);
    _reset_buff(kii);

    kii_code_t res = _subscribe_bucket(kii, bucket);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }
exit:
    return res;
}

// The outcome of a push message request sent with result res.
static kii_code_t _push_message_result(kii_t* kii, kii_code_t res)
{
    if (res != KII_ERR_OK) {
        return res;
    }
    // Kii's guide documents 201, its API reference 200.
    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }
    return KII_ERR_OK;
}

kii_code_t kii_send_push_message(
        kii_t* kii,
        const kii_topic_t* topic,
        const char* message)
{
    _reset_khc(kii);
    _reset_buff(kii);

    return _push_message_result(kii, _send_push_message(kii, topic, message));
}

kii_code_t kii_send_push_message_to_thing(
        kii_t* kii,
        const char* thing_id,
        const char* message)
{
    _reset_khc(kii);
    _reset_buff(kii);

    return _push_message_result(kii, _send_push_message_to_thing(kii, thing_id, message));
}

kii_code_t kii_install_push(
        kii_t* kii,
        bool development,
        kii_installation_id_t* out_installation_id)
{
    _reset_khc(kii);
    _reset_buff(kii);

    kii_code_t res = _install_push(kii, development);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    parse_result = jkii_parse(buff, buff_size, fields);
    if (parse_result != JKII_ERR_OK) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }

exit:
    return res;
}

/* jkii reads JSON integers as int32_t and int64_t, so these check that a
 * value fits where kii_mqtt_endpoint_t keeps it: a port in a uint16_t, the TTL
 * in a uint32_t. */

static int _is_port(int32_t value)
{
    return 0 <= value && value <= 65535;
}

static int _fits_uint32(int64_t value)
{
    return 0 <= value && value <= (int64_t)UINT32_MAX;
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

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code)
    {
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

    parse_result = jkii_parse(buff, buff_size, fields);
    if (parse_result != JKII_ERR_OK) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }
    if (_is_port(fields[4].field_copy.int_value) == 0 ||
            _is_port(fields[5].field_copy.int_value) == 0 ||
            _fits_uint32(fields[6].field_copy.long_value) == 0) {
        res = KII_ERR_PARSE_JSON;
        goto exit;
    }
    endpoint->port_tcp = (uint16_t)fields[4].field_copy.int_value;
    endpoint->port_ssl = (uint16_t)fields[5].field_copy.int_value;
    endpoint->ttl = (uint32_t)fields[6].field_copy.long_value;

exit:
    // The response carried the MQTT credentials.
    _kii_clear_buffers(kii);
    return res;
}

/* What the task uses without checking, so that one left out is found when
 * starting it, rather than as a crash in the task. */
static bool _socket_can_run(const ebisu_socket_t* sock)
{
    return (sock != NULL && sock->ops != NULL &&
            sock->ops->connect != NULL &&
            sock->ops->send != NULL &&
            sock->ops->recv != NULL &&
            sock->ops->close != NULL);
}

/* Whether the task can run. One with an endpoint of the application's makes
 * no requests of Kii Cloud, so needs neither the HTTP socket nor the buffer. */
static bool _push_task_can_run(const kii_t* kii, bool endpoint_given)
{
    return (kii->_system->ops->delay_ms != NULL &&
            (endpoint_given || _socket_can_run(kii->_khc._socket)) &&
            _socket_can_run(kii->_mqtt_socket) &&
            (endpoint_given || kii->_rw_buff != NULL) &&
            kii->_mqtt_buffer != NULL);
}

static kii_code_t _start_task(kii_t* kii, uint32_t keep_alive_interval,
        const kii_mqtt_endpoint_t* endpoint)
{
    // A second task would share the first's sockets and buffers.
    if (kii->_push_task_running) {
        return KII_ERR_FAIL;
    }
    // CONNECT carries the keep-alive in 16 bits. A longer one would reach the
    // broker cut down to its low 16 bits while the task pinged on the whole.
    // The parameter is wider than that so that such a value gets here whole.
    if (keep_alive_interval > 65535) {
        return KII_ERR_FAIL;
    }
    if (kii->_system == NULL || kii->_system->ops->task_create == NULL) {
        return KII_ERR_CREATE_TASK;
    }
    if (kii->_cb_mqtt_message == NULL || !_push_task_can_run(kii, endpoint != NULL)) {
        return KII_ERR_FAIL;
    }
    kii->_mqtt_endpoint_given = endpoint != NULL;
    if (endpoint != NULL) {
        kii->_mqtt_endpoint = *endpoint;
    }
    kii->_keep_alive_interval = (uint16_t)keep_alive_interval;
    // Before creating it, as it may have finished by the time that returns.
    kii->_push_task_running = true;
    _kii_mqtt_publish_open(kii, true);
    ebisu_task_code_t res = kii->_system->ops->task_create(kii->_system,
            KII_TASK_NAME_MQTT,
            mqtt_start_task,
            (void*)kii);
    if (res != EBISU_TASKC_OK) {
        _kii_mqtt_publish_open(kii, false);
        kii->_push_task_running = false;
        return KII_ERR_CREATE_TASK;
    }
    return KII_ERR_OK;
}

kii_code_t kii_start_push_task(kii_t* kii, uint32_t keep_alive_interval)
{
    return _start_task(kii, keep_alive_interval, NULL);
}

/* Whether each string of endpoint is terminated within its array. */
static bool _endpoint_terminated(const kii_mqtt_endpoint_t* endpoint)
{
    return memchr(endpoint->username, '\0', sizeof(endpoint->username)) != NULL &&
        memchr(endpoint->password, '\0', sizeof(endpoint->password)) != NULL &&
        memchr(endpoint->topic, '\0', sizeof(endpoint->topic)) != NULL &&
        memchr(endpoint->host, '\0', sizeof(endpoint->host)) != NULL;
}

kii_code_t kii_start_mqtt_task_with_endpoint(
        kii_t* kii,
        const kii_mqtt_endpoint_t* endpoint,
        uint32_t keep_alive_interval)
{
    if (endpoint == NULL) {
        return KII_ERR_FAIL;
    }
    if (!_endpoint_terminated(endpoint)) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    if (endpoint->host[0] == '\0' || endpoint->topic[0] == '\0') {
        return KII_ERR_FAIL;
    }
    return _start_task(kii, keep_alive_interval, endpoint);
}

bool _kii_mqtt_endpoint_terminated(const kii_mqtt_endpoint_t* endpoint)
{
    return _endpoint_terminated(endpoint);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
