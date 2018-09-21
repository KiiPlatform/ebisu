#ifndef __tio2__
#define __tio2__

#include "kii.h"
#include "khc.h"

typedef kii_bool_t tio_bool_t;

typedef enum tio_data_type_t {
    TIO_TYPE_NULL,
    TIO_TYPE_BOOLEAN,
    TIO_TYPE_NUMBER,
    TIO_TYPE_STRING,
    TIO_TYPE_OBJECT,
    TIO_TYPE_ARRAY
} tio_data_type_t;

typedef struct tio_action_params_t {
    tio_data_type_t type;
    union {
        long long_value;
        double double_value;
        tio_bool_t bool_value;
        const char *string_value;
        const char *object_value;
        const char *array_value;
    } param;
} tio_action_params_t;

typedef struct tio_action_t {
    const char* alias;
    const char* action_name;
    tio_action_params_t action_params;
} tio_action_t;

typedef size_t (*TIO_CB_READ)(char *buffer, size_t size, size_t count, void *userdata);
typedef tio_bool_t (*TIO_CB_ACTION)(tio_action_t action, void* userdata);

typedef enum tio_code_t {
    TIO_ERR_OK,
    TIO_ERR_FAIL
} tio_code_t;

typedef struct tio_handler_t {
    TIO_CB_ACTION _cb_action;
    kii_t _kii;
    char* _http_buff;
    size_t _http_buff_size;
    char* _mqtt_buff;
    size_t _mqtt_buff_size;
    size_t _keep_alive_interval;
} tio_handler_t;

typedef struct tio_author_t {
    char id[64];
    char token[64];
} tio_author_t;

void tio_handler_set_cb_sock_connect_http(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_handler_set_cb_sock_send_http(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_handler_set_cb_sock_recv_http(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_handler_set_cb_sock_close_http(tio_handler_t* handler, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

void tio_handler_set_http_buff(tio_handler_t* handler, char* buff, size_t buff_size);

void tio_handler_set_cb_sock_connect_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_handler_set_cb_sock_send_mqtt(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_handler_set_cb_sock_recv_mqtt(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_handler_set_cb_sock_close_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

void tio_handler_set_mqtt_buff(tio_handler_t* handler, char* buff, size_t buff_size);

void tio_handler_set_keep_alive_interval(tio_handler_t* handler, size_t keep_alive_interval);

void tio_handler_set_app(tio_handler_t* handler, const char* app_id, const char* host);

tio_code_t tio_handler_start(tio_handler_t* handler, const tio_author_t* author, TIO_CB_ACTION cb_action, void* userdata);

typedef struct tio_updater_t {
    TIO_CB_READ _state_reader;
    kii_t _kii;
    size_t _update_interval;
} tio_updater_t;

void tio_updater_set_cb_sock_connect(tio_updater_t* updater, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_updater_set_cb_sock_send(tio_updater_t* updater, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_updater_set_cb_sock_recv(tio_updater_t* updater, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_updater_set_cb_sock_close(tio_updater_t* updater, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

void tio_updater_set_buff(tio_updater_t* updater, char* buff, size_t buff_size);

void tio_updater_set_app(tio_updater_t* updater, const char* app_id, const char* host);

void tio_updater_set_interval(tio_updater_t* updater, size_t update_interval);

tio_code_t tio_updater_start_with_author(
    tio_updater_t* updater,
    const tio_author_t* author,
    const kii_mqtt_endpoint_t* endpoint,
    TIO_CB_READ state_reader,
    void* userdata);

#endif