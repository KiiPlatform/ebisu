#include "kii.h"
#include "kii_impl.h"
#include <string.h>

#define KII_SDK_INFO "sn=te;sv=1.2.4"

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";
const char _CONTENT_LENGTH_ZERO[] = "Content-Length: 0";

size_t _cb_write_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    if (kii->_rw_buff_written == 0) {
        memset(kii->_rw_buff, '\0', kii->_rw_buff_size);
    }
    size_t remain = kii->_rw_buff_size - kii->_rw_buff_written;
    size_t req_size = size;
    if (remain < req_size)
    {
        // Insufficient buffer size.
        return 0;
    }
    memcpy(kii->_rw_buff + kii->_rw_buff_written, buffer, req_size);
    kii->_rw_buff_written += req_size;
    return req_size;
}

size_t _cb_read_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    size_t remain = kii->_rw_buff_req_size - kii->_rw_buff_read;
    if (remain <= 0)
    {
        return 0;
    }
    size_t to_read = (size > remain) ? (remain) : (size);
    memcpy(buffer, kii->_rw_buff + kii->_rw_buff_read, to_read);
    kii->_rw_buff_read += to_read;
    return to_read;
}

size_t _cb_write_header(char *buffer, size_t size, void *userdata)
{
    // TODO: implement it later for getting Etag, etc.
    char* etag_buff = ((kii_t*)userdata)->_etag;
    _parse_etag(buffer, size, etag_buff, 64);
    return size;
}

void kii_init(kii_t* kii)
{
    memset(kii, 0x00, sizeof(kii_t));
    kii->_sdk_info = KII_SDK_INFO;
    khc_init(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    khc_set_cb_header(&kii->_khc, _cb_write_header, kii);
    kii->_etag[0] = '\0';
    kii->_cb_slist_alloc = khc_cb_slist_alloc;
    kii->_cb_slist_free = khc_cb_slist_free;
    kii->_slist_alloc_data = NULL;
    kii->_slist_free_data = NULL;
    kii->_sdk_info = KII_SDK_INFO;
    kii->_cb_task_create = NULL;
    kii->_task_create_data = NULL;
    kii->_cb_task_continue = NULL;
    kii->_task_continue_data = NULL;
    kii->_cb_task_exit = NULL;
    kii->_task_exit_data = NULL;
    kii->_cb_delay_ms = NULL;
    kii->_delay_ms_data = NULL;
}

void kii_set_site(
        kii_t* kii,
        const char* site)
{
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
}

void kii_set_app_id(
        kii_t* kii,
        const char* app_id)
{
    strncpy(kii->_app_id, app_id, sizeof(kii->_app_id) * sizeof(char));
}

void kii_set_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
}

void kii_set_stream_buff(kii_t* kii, char* buff, size_t buff_size) {
    khc_set_stream_buff(&kii->_khc, buff, buff_size);
}

void kii_set_resp_header_buff(kii_t* kii, char* buff, size_t buff_size) {
    khc_set_resp_header_buff(&kii->_khc, buff, buff_size);
}

void kii_set_cb_http_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    khc_set_cb_sock_connect(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    khc_set_cb_sock_send(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    khc_set_cb_sock_recv(&kii->_khc, cb, userdata);
}

void kii_set_cb_http_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    khc_set_cb_sock_close(&kii->_khc, cb, userdata);
}

void kii_set_mqtt_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_mqtt_buffer = buff;
    kii->_mqtt_buffer_size = buff_size;
}

void kii_set_cb_mqtt_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata) {
    kii->_cb_mqtt_sock_connect = cb;
    kii->_mqtt_sock_connect_ctx = userdata;
}

void kii_set_cb_mqtt_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata) {
    kii->_cb_mqtt_sock_send = cb;
    kii->_mqtt_sock_send_ctx = userdata;
}

void kii_set_cb_mqtt_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata) {
    kii->_cb_mqtt_sock_recv = cb;
    kii->_mqtt_sock_recv_ctx = userdata;
}

void kii_set_cb_mqtt_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata) {
    kii->_cb_mqtt_sock_close_cb = cb;
    kii->_mqtt_sock_close_ctx = userdata;
}

void kii_set_mqtt_to_sock_recv(kii_t* kii, unsigned int to_sock_recv_sec) {
    kii->_mqtt_to_recv_sec = to_sock_recv_sec;
}

void kii_set_mqtt_to_sock_send(kii_t* kii, unsigned int to_sock_send_sec) {
    kii->_mqtt_to_send_sec = to_sock_send_sec;
}

void kii_set_cb_task_create(kii_t* kii, KII_CB_TASK_CREATE cb, void* userdata) {
    kii->_cb_task_create = cb;
    kii->_task_create_data = userdata;
}

void kii_set_cb_task_continue(kii_t* kii, KII_CB_TASK_CONTINUE cb, void* userdata) {
    kii->_cb_task_continue = cb;
    kii->_task_continue_data = userdata;
}

void kii_set_cb_task_exit(kii_t* kii, KII_CB_TASK_EXIT cb, void* userdata) {
    kii->_cb_task_exit = cb;
    kii->_task_exit_data = userdata;
}

void kii_set_cb_delay_ms(kii_t* kii, KII_CB_DELAY_MS cb, void* userdata) {
    kii->_cb_delay_ms = cb;
    kii->_delay_ms_data = userdata;
}

void kii_set_json_parser_resource(kii_t* kii, jkii_resource_t* resource) {
    kii->_json_resource = resource;
}

void kii_set_cb_json_parser_resource(
    kii_t* kii,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free)
{
    kii->_cb_json_alloc = cb_alloc;
    kii->_cb_json_free = cb_free;
}

void kii_set_cb_slist_resource(
        kii_t* kii,
        KHC_CB_SLIST_ALLOC cb_alloc,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_alloc_data,
        void* cb_free_data) {
    kii->_cb_slist_alloc = cb_alloc;
    kii->_cb_slist_free = cb_free;
    kii->_slist_alloc_data = cb_alloc_data;
    kii->_slist_free_data = cb_free_data;
}

const char* kii_get_etag(kii_t* kii) {
    return kii->_etag;
}

int kii_get_resp_status(kii_t* kii) {
    return khc_get_status_code(&kii->_khc);
}

size_t kii_get_resp_body_length(kii_t* kii) {
    return kii->_rw_buff_written;
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
    khc_slist_free_all_using_cb_free(kii->_req_headers, kii->_cb_slist_free, kii->_slist_free_data);
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
