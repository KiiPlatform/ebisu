#include "kii.h"
#include <string.h>

#define KII_SDK_INFO "sn=te;sv=1.2.4"

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";

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
    return 0;
}

int kii_api_call(
    kii_t *kii,
    const char *http_method,
    const char *resource_path,
    const void *http_body,
    size_t body_size,
    const char *content_type,
    char *header,
    ...)
{
    // TODO: reimplement it. Equivalent to kii_core_api_call().
    return -1;
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

int _kii_set_content_length(kii_t* kii, size_t content_length) {
    kii->_rw_buff_req_size = content_length;
    return 0;
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

void _reset_rw_buff(kii_t* kii) {
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_rw_buff_req_size = 0;
}