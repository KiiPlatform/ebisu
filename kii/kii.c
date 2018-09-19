#include "kii.h"
#include "kii_impl.h"
#include <string.h>

#define KII_SDK_INFO "sn=te;sv=1.2.4"

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";
const char _CONTENT_LENGTH_ZERO[] = "Content-Length: 0";

size_t _cb_write_buff(char *buffer, size_t size, size_t count, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    if (kii->_rw_buff_written == 0) {
        memset(kii->_rw_buff, '\0', kii->_rw_buff_size);
    }
    size_t remain = kii->_rw_buff_size - kii->_rw_buff_written;
    size_t req_size = size * count;
    if (remain < req_size)
    {
        // Insufficient buffer size.
        return 0;
    }
    memcpy(kii->_rw_buff + kii->_rw_buff_written, buffer, req_size);
    kii->_rw_buff_written += req_size;
    return req_size;
}

size_t _cb_read_buff(char *buffer, size_t size, size_t count, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    size_t remain = kii->_rw_buff_req_size - kii->_rw_buff_read;
    if (remain <= 0)
    {
        return 0;
    }
    size_t to_read = (size * count > remain) ? (remain) : (size * count);
    memcpy(buffer, kii->_rw_buff + kii->_rw_buff_read, to_read);
    kii->_rw_buff_read += to_read;
    return to_read;
}

size_t _cb_write_header(char *buffer, size_t size, size_t count, void *userdata)
{
    // TODO: implement it later for getting Etag, etc.
    char* etag_buff = ((kii_t*)userdata)->_etag;
    _parse_etag(buffer, size * count, etag_buff, 64);
    return size * count;
}

int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id)
{
    memset(kii, 0x00, sizeof(kii_t));
    strncpy(kii->_app_id, app_id, sizeof(kii->_app_id) * sizeof(char));
    char* host;
    if(strcmp(site, "CN3") == 0)
    {
        host = "api-cn3.kii.com";
    }
    else if(strcmp(site, "JP") == 0)
    {
        host = "api-jp.kii.com";
    }
    else if(strcmp(site, "US") == 0)
    {
        host = "api.kii.com";
    }
    else if(strcmp(site, "SG") == 0)
    {
        host = "api-sg.kii.com";
    }
    else if (strcmp(site, "EU") == 0)
    {
        host = "api-eu.kii.com";
    }
    else
    {
        /* Let's enable to set custom host */
        host = (char*)site;
    }
    strncpy(kii->_app_host, host, sizeof(kii->_app_host) * sizeof(char));
    kii->_sdk_info = KII_SDK_INFO;
    khc_set_zero(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    khc_set_cb_header(&kii->_khc, _cb_write_header, kii);
    kii->_etag[0] = '\0';
    return 0;
}

int kii_set_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    return 0;
}

int kii_set_http_cb_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    khc_set_cb_sock_connect(&kii->_khc, cb, userdata);
    return 0;
}

int kii_set_http_cb_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    khc_set_cb_sock_send(&kii->_khc, cb, userdata);
    return 0;
}

int kii_set_http_cb_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    khc_set_cb_sock_recv(&kii->_khc, cb, userdata);
    return 0;
}

int kii_set_http_cb_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    khc_set_cb_sock_close(&kii->_khc, cb, userdata);
    return 0;
}

int kii_set_mqtt_cb_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    kii->mqtt_sock_connect_cb = cb;
    kii->mqtt_sock_connect_ctx = userdata;
    return 0;
}

int kii_set_mqtt_cb_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    kii->mqtt_sock_send_cb = cb;
    kii->mqtt_sock_send_ctx = userdata;
    return 0;
}

int kii_set_mqtt_cb_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    kii->mqtt_sock_recv_cb = cb;
    kii->mqtt_sock_recv_ctx = userdata;
    return 0;
}

int kii_set_mqtt_cb_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    kii->mqtt_sock_close_cb = cb;
    kii->mqtt_sock_close_ctx = userdata;
    return 0;
}

kii_code_t kii_set_json_parser_resource(kii_t* kii, kii_json_resource_t* resource) {
    kii->_json_resource = resource;
    return KII_ERR_OK;
}

kii_code_t kii_set_json_parser_resource_cb(
    kii_t* kii,
    KII_JSON_RESOURCE_ALLOC_CB alloc_cb,
    KII_JSON_RESOURCE_FREE_CB free_cb)
{
    kii->_json_alloc_cb = alloc_cb;
    kii->_json_free_cb = free_cb;
    return KII_ERR_OK;
}

const char* kii_get_etag(kii_t* kii) {
    return kii->_etag;
}

int kii_get_resp_status(kii_t* kii) {
    return khc_get_status_code(&kii->_khc);
}

kii_code_t _convert_code(khc_code khc_c) {
    switch(khc_c) {
        case KHC_ERR_OK:
            return KII_ERR_OK;
        case KHC_ERR_SOCK_CONNECT:
            return KII_ERR_SOCK_CONNECT;
        case KHC_ERR_SOCK_CLOSE:
            return KII_ERR_SOCK_CONNECT;
        case KHC_ERR_SOCK_SEND:
            return KII_ERR_SOCK_SEND;
        case KHC_ERR_SOCK_RECV:
            return KII_ERR_SOCK_RECV;
        case KHC_ERR_HEADER_CALLBACK:
            return KII_ERR_HEADER_CALLBACK;
        case KHC_ERR_WRITE_CALLBACK:
            return KII_ERR_WRITE_CALLBACK;
        case KHC_ERR_ALLOCATION:
            return KII_ERR_ALLOCATION;
        case KHC_ERR_TOO_LARGE_DATA:
            return KII_ERR_TOO_LARGE_DATA;
        case KHC_ERR_FAIL:
            return KII_ERR_FAIL;
    }
    return KII_ERR_FAIL;
}

void _reset_buff(kii_t* kii) {
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_rw_buff_req_size = 0;
    kii->_etag[0] = '\0';
}

void _req_headers_free_all(kii_t* kii) {
    khc_slist_free_all(kii->_req_headers);
    kii->_req_headers = NULL;
}

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len) {
    char header_cpy[header_len + 1];
    memcpy(header_cpy, header, header_len);
    header_cpy[header_len] = '\0';

    const char etag_lower[] = "etag";
    const char etag_upper[] = "ETAG";
    size_t key_len = strlen(etag_lower);
    int state = 0;
    int j = 0;
    for (int i = 0; i < header_len; ++i) {
        char c = header_cpy[i];

        if (state == 0) {
            if (c == etag_lower[i] || c == etag_upper[i]) {
                if (i == key_len - 1) {
                    state = 1;
                }
                continue;
            } else {
                // Not Etag.
                return -1;
            }
        } else if (state == 1) { // Skip WP before :
            if (c == ' ' || c == '\t') {
                continue;
            } else if ( c == ':') {
                state = 2;
                continue;
            } else {
                // Inalid Format.
                return -2;
            }
        } else if (state == 2) { // Skip WP after :
            if (c == ' ' || c == '\t') {
                continue;
            } else {
                state = 3;
                buff[0] = c;
                j++;
                continue;
            }
        } else if (state == 3) { // Extract value
            if (c == ' ' || c == '\t' || c == '\r') {
                break;
            } else {
                if (j < buff_len - 1) {
                    buff[j] = c;
                    ++j;
                    continue;
                } else {
                    // Etag too large.
                    return -3;
                }
            }
        }
    }
    buff[j] = '\0';
    return j;
}