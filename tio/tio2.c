#include <string.h>
#include "tio2.h"
#include "kii.h"
#include "khc.h"

void tio_handler_set_cb_sock_connect_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_CONNECT cb_connect,
    void* userdata)
{
    kii_set_http_cb_sock_connect(&handler->_kii, cb_connect, userdata);
}

void tio_handler_set_cb_sock_send_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    kii_set_http_cb_sock_send(&handler->_kii, cb_send, userdata);
}

void tio_handler_set_cb_sock_recv_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    kii_set_http_cb_sock_recv(&handler->_kii, cb_recv, userdata);
}

void tio_handler_set_cb_sock_close_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{
    kii_set_http_cb_sock_close(&handler->_kii, cb_close, userdata);
}

void tio_handler_set_http_buff(
    tio_handler_t* handler,
    char* buff,
    size_t buff_size)
{
    kii_set_buff(&handler->_kii, buff, buff_size);
}

void tio_handler_set_cb_sock_connect_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_CONNECT cb_connect,
    void* userdata)
{
    kii_set_mqtt_cb_sock_connect(&handler->_kii, cb_connect, userdata);
}

void tio_handler_set_cb_sock_send_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    kii_set_mqtt_cb_sock_send(&handler->_kii, cb_send, userdata);
}

void tio_handler_set_cb_sock_recv_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    kii_set_mqtt_cb_sock_recv(&handler->_kii, cb_recv, userdata);
}

void tio_handler_set_cb_sock_close_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{
    kii_set_mqtt_cb_sock_close(&handler->_kii, cb_close, userdata);
}

void tio_handler_set_cb_task_create(
    tio_handler_t* handler,
    KII_TASK_CREATE cb_task_create)
{
    handler->_kii.task_create_cb = cb_task_create;
}

void tio_hadler_set_cb_delay_ms(
    tio_handler_t* handler,
    KII_DELAY_MS cb_delay_ms)
{
    handler->_kii.delay_ms_cb = cb_delay_ms;
}

void tio_handler_set_mqtt_buff(
    tio_handler_t* handler,
    char* buff,
    size_t buff_size)
{
    //FIXME: Kii should provide setter API.
    handler->_kii.mqtt_buffer = buff;
    handler->_kii.mqtt_buffer_size =buff_size;
}

void tio_handler_set_keep_alive_interval(
    tio_handler_t* handler,
    size_t keep_alive_interval)
{
    handler->_keep_alive_interval = keep_alive_interval;
}

void tio_handler_set_app(
    tio_handler_t* handler,
    const char* app_id,
    const char* host)
{
    // FIXME: Kii should provide setter API.
    strncpy(handler->_kii._app_id, app_id, sizeof(handler->_kii._app_id)-1);
    strncpy(handler->_kii._app_host, host, sizeof(handler->_kii._app_host)-1);
}

tio_code_t tio_handler_start(
    tio_handler_t* handler,
    const tio_author_t* author,
    const kii_mqtt_endpoint_t* endpoint,
    TIO_CB_ACTION cb_action,
    void* userdata)
{
    //TODO: Implement it.
    return TIO_ERR_FAIL;
}

void tio_updater_set_cb_sock_connect(
    tio_updater_t* updater,
    KHC_CB_SOCK_CONNECT cb_connect,
    void* userdata)
{
    kii_set_http_cb_sock_connect(&updater->_kii, cb_connect, userdata);
}

void tio_updater_set_cb_sock_send(
    tio_updater_t* updater,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    kii_set_http_cb_sock_send(&updater->_kii, cb_send, userdata);
}

void tio_updater_set_cb_sock_recv(
    tio_updater_t* updater,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    kii_set_http_cb_sock_recv(&updater->_kii, cb_recv, userdata);
}

void tio_updater_set_cb_sock_close(
    tio_updater_t* updater,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{
    kii_set_http_cb_sock_close(&updater->_kii, cb_close, userdata);
}

void tio_updater_set_cb_task_create(
    tio_updater_t* updater,
    KII_TASK_CREATE cb_task_create)
{
    // FIXME: Kii should provide setter API.
    updater->_kii.task_create_cb = cb_task_create;
}

void tio_updater_set_cb_delay_ms(
    tio_updater_t* updater,
    KII_DELAY_MS cb_delay_ms)
{
    // FIXME: Kii should provide setter API.
    updater->_kii.delay_ms_cb = cb_delay_ms;
}

void tio_updater_set_buff(
    tio_updater_t* updater,
    char* buff,
    size_t buff_size)
{
    kii_set_buff(&updater->_kii, buff, buff_size);
}

void tio_updater_set_app(
    tio_updater_t* updater,
    const char* app_id,
    const char* host)
{
    // FIXME: Kii should provide setter API.
    strncpy(updater->_kii._app_id, app_id, sizeof(updater->_kii._app_id)-1);
    strncpy(updater->_kii._app_host, host, sizeof(updater->_kii._app_host)-1);
}

void tio_updater_set_interval(
    tio_updater_t* updater,
    size_t update_interval)
{
    updater->_update_interval = update_interval;
}

tio_code_t tio_updater_start(
    tio_updater_t* updater,
    const tio_author_t* author,
    TIO_CB_READ state_reader,
    void* userdata)
{
    //TODO: Implement it.
    return TIO_ERR_FAIL;
}