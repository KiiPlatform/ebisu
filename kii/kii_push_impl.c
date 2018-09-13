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
    // TODO: reimplement it.
    return KII_ERR_FAIL;
}

kii_code_t _get_mqtt_endpoint(
        kii_t* kii,
        const char* installation_id)
{
    // TODO: reimplement it.
    return KII_ERR_FAIL;
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
