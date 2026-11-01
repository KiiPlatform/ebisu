#include "kii_ti_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "jkii_wrapper.h"
#include <string.h>

kii_code_t _onboard(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    _reset_khc(kii);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/onboardings", kii->_app_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_auth_basic_app(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, "application/vnd.kii.OnboardingWithVendorThingIDByThing+json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"vendorThingID\":\"", vendor_thing_id, "");
    content_len = _kii_body_append(kii, content_len,
            "\",\"thingPassword\":\"", password, "\"");
    if (thing_type != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"thingType\":\"", thing_type, "\"");
    }
    if (firmware_version != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"firmwareVersion\":\"", firmware_version, "\"");
    }
    if (layout_position != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"layoutPosition\":\"", layout_position, "\"");
    }
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    if (thing_properties != NULL) {
        // A JSON object, not a string: embedded as given, unlike the values
        // above, which are escaped to go between quotes.
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"thingProperties\":%s",
                thing_properties);
        if (content_len >= kii->_rw_buff_size) {
            _req_headers_free_all(kii);
            return KII_ERR_TOO_LARGE_DATA;
        }
    }

    content_len += snprintf(
            &kii->_rw_buff[content_len],
            kii->_rw_buff_size - content_len,
            "}");
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    ret = _req_send(kii);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    // Onboarding issues no refresh token: one held belongs to the tokens this
    // replaces.
    kii->_author.refresh_token[0] = '\0';

    // Where MQTT is enabled, the response also carries the endpoint of the
    // installation onboarding made.
    kii_mqtt_endpoint_t endpoint;
    kii_installation_id_t installation_id;
    memset(&endpoint, 0, sizeof(endpoint));
    memset(&installation_id, 0, sizeof(installation_id));
    kii->_onboarded_thing_id[0] = '\0';

    jkii_field_t fields[10];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "accessToken";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
        sizeof(kii->_author.access_token[0]);
    fields[1].name = "thingID";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->_author.author_id) /
        sizeof(kii->_author.author_id[0]);
    fields[2].path = "/mqttEndpoint/installationID";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[2].field_copy.string = installation_id.id;
    fields[2].field_copy_buff_size = sizeof(installation_id.id);
    fields[3].path = "/mqttEndpoint/username";
    fields[3].type = JKII_FIELD_TYPE_STRING;
    fields[3].field_copy.string = endpoint.username;
    fields[3].field_copy_buff_size = sizeof(endpoint.username);
    fields[4].path = "/mqttEndpoint/password";
    fields[4].type = JKII_FIELD_TYPE_STRING;
    fields[4].field_copy.string = endpoint.password;
    fields[4].field_copy_buff_size = sizeof(endpoint.password);
    fields[5].path = "/mqttEndpoint/mqttTopic";
    fields[5].type = JKII_FIELD_TYPE_STRING;
    fields[5].field_copy.string = endpoint.topic;
    fields[5].field_copy_buff_size = sizeof(endpoint.topic);
    fields[6].path = "/mqttEndpoint/host";
    fields[6].type = JKII_FIELD_TYPE_STRING;
    fields[6].field_copy.string = endpoint.host;
    fields[6].field_copy_buff_size = sizeof(endpoint.host);
    fields[7].path = "/mqttEndpoint/portTCP";
    fields[7].type = JKII_FIELD_TYPE_INTEGER;
    fields[8].path = "/mqttEndpoint/portSSL";
    fields[8].type = JKII_FIELD_TYPE_INTEGER;
    fields[9].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if ((result != JKII_ERR_OK && result != JKII_ERR_PARTIAL) ||
            fields[0].result != JKII_FIELD_ERR_OK ||
            fields[1].result != JKII_FIELD_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }

    // Without the whole endpoint, the push task installs push and fetches it.
    if (result == JKII_ERR_OK &&
            0 < fields[7].field_copy.int_value && fields[7].field_copy.int_value <= 65535 &&
            0 < fields[8].field_copy.int_value && fields[8].field_copy.int_value <= 65535) {
        endpoint.port_tcp = (uint16_t)fields[7].field_copy.int_value;
        endpoint.port_ssl = (uint16_t)fields[8].field_copy.int_value;
        kii->_onboarded_endpoint = endpoint;
        kii->_onboarded_installation_id = installation_id;
        memcpy(kii->_onboarded_thing_id, kii->_author.author_id,
                sizeof(kii->_onboarded_thing_id));
    }

    return KII_ERR_OK;
}

kii_code_t _put_firmware_version(
        kii_t* kii,
        const char* firmware_version)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    _reset_khc(kii);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/things/%s/firmware-version", kii->_app_id, kii->_author.author_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, "application/vnd.kii.ThingFirmwareVersionUpdateRequest+json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"firmwareVersion\":\"", firmware_version, "\"}");
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    ret = _req_send(kii);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    return KII_ERR_OK;
}

kii_code_t _get_firmware_version(
        kii_t* kii,
        kii_ti_firmware_version_t* out_version)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    _reset_khc(kii);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "GET");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/things/%s/firmware-version", kii->_app_id, kii->_author.author_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    ret = _req_send(kii);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    jkii_field_t fields[2];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "firmwareVersion";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_version->firmware_version;
    fields[0].field_copy_buff_size = sizeof(out_version->firmware_version) /
        sizeof(out_version->firmware_version[0]);
    fields[1].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }

    return KII_ERR_OK;
}

kii_code_t _upload_state(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* last_path_segment,
        const char* method,
        const char* content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_khc(kii);
    _reset_buff(kii);

    if (opt_normalizer_host != NULL) {
        khc_set_host(&kii->_khc, opt_normalizer_host);
    } else {
        khc_set_host(&kii->_khc, kii->_app_host);
    }
    khc_set_method(&kii->_khc, method);

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/targets/thing:%s/%s", kii->_app_id, kii->_author.author_id, last_path_segment);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, content_type);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (opt_content_encoding != NULL) {
        ret = _set_content_encoding(kii, opt_content_encoding);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }

    khc_set_cb_read(&kii->_khc, state_read_cb, state_read_cb_data);
    kii->_req_streamed = KII_TRUE;
    ret = _req_send(kii);
    kii->_req_streamed = KII_FALSE;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    return KII_ERR_OK;
}

