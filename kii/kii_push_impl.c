#include <string.h>
#include "kii_push_impl.h"
#include "kii_mqtt.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "kii.h"

kii_code_t _install_push(
        kii_t* kii,
        kii_bool_t development)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size,
        "/api/apps/%s/installations", kii->_app_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request Headers
    kii_code_t res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_content_type(kii, "application/vnd.kii.InstallationCreationRequest+json");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    // Request body
    const char* flag = "false";
    if (development == KII_TRUE) {
        flag = "true";
    }
    int body_len = snprintf(kii->_rw_buff, kii->_rw_buff_size,
        "{\"deviceType\":\"MQTT\", \"development\": \"%s\"}",
        flag);
    if (body_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    res =_set_content_length(kii, body_len);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _get_mqtt_endpoint(
        kii_t* kii,
        const char* installation_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "GET");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size,
        "/api/apps/%s/installations/%s/mqtt-endpoint",
        kii->_app_id, installation_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request Headers
    kii_code_t res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _subscribe_bucket(
        kii_t* kii,
        const kii_bucket_t* bucket)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    kii_code_t res = _set_bucket_subscription_path(kii, bucket);
    if (res != KII_ERR_OK) {
        return res;
    }
    // Request Headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _unsubscribe_bucket(
        kii_t* kii,
        const kii_bucket_t* bucket)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "DELETE");

    kii_code_t res = _set_bucket_subscription_path(kii, bucket);
    if (res != KII_ERR_OK) {
        return res;
    }
    // Request Headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _subscribe_topic(
        kii_t* kii,
        const kii_topic_t* topic)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    kii_code_t res = _set_topic_subscription_path(kii, topic);
    if (res != KII_ERR_OK) {
        return res;
    }
    // Request headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    // No body.
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(code);
}

kii_code_t _unsubscribe_topic(
        kii_t* kii,
        const kii_topic_t* topic)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "DELETE");

    kii_code_t res = _set_topic_subscription_path(kii, topic);
    if (res != KII_ERR_OK) {
        return res;
    }
    // Request headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    // No body.
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(code);
}

kii_code_t _put_topic(
        kii_t* kii,
        const kii_topic_t* topic)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    kii_code_t res = _set_topic_path(kii, topic);
    if (res != KII_ERR_OK) {
        return res;
    }

    // Request headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    // No body.
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(code);
}

kii_code_t _delete_topic(
        kii_t* kii,
        const kii_topic_t* topic)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "DELETE");

    kii_code_t res = _set_topic_path(kii, topic);
    if (res != KII_ERR_OK) {
        return res;
    }

    // Request headers
    res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_auth_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    // No body.
    res = _set_req_body(kii, "");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(code);
}
