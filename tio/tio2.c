#include "tio2.h"

void tio_handler_set_cb_sock_connect_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_CONNECT cb_connect,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_send_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_recv_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_close_http(
    tio_handler_t* handler,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{

}

void tio_handler_set_http_buff(
    tio_handler_t* handler,
    char* buff,
    size_t buff_size)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_connect_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_CONNECT cb_connect,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_send_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_recv_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_sock_close_mqtt(
    tio_handler_t* handler,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_handler_set_cb_task_create(
    tio_handler_t* handler,
    KII_TASK_CREATE cb_task_create)
{
    //TODO: Implement it.
}

void tio_hadler_set_cb_delay_ms(
    tio_handler_t* handler,
    KII_DELAY_MS cb_delay_ms)
{
    //TODO: Implement it.
}

void tio_handler_set_mqtt_buff(
    tio_handler_t* handler,
    char* buff,
    size_t buff_size)
{
    //TODO: Implement it.
}

void tio_handler_set_keep_alive_interval(
    tio_handler_t* handler,
    size_t keep_alive_interval)
{
    //TODO: Implement it.
}

void tio_handler_set_app(
    tio_handler_t* handler,
    const char* app_id,
    const char* host)
{
    //TODO: Implement it.
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
    //TODO: Implement it.
}

void tio_updater_set_cb_sock_send(
    tio_updater_t* updater,
    KHC_CB_SOCK_SEND cb_send,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_updater_set_cb_sock_recv(
    tio_updater_t* updater,
    KHC_CB_SOCK_RECV cb_recv,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_updater_set_cb_sock_close(
    tio_updater_t* updater,
    KHC_CB_SOCK_CLOSE cb_close,
    void* userdata)
{
    //TODO: Implement it.
}

void tio_updater_set_cb_task_create(
    tio_updater_t* updater,
    KII_TASK_CREATE cb_task_create)
{
    //TODO: Implement it.
}

void tio_updater_set_cb_delay_ms(
    tio_updater_t* updater,
    KII_DELAY_MS cb_delay_ms)
{
    //TODO: Implement it.
}

void tio_updater_set_buff(
    tio_updater_t* updater,
    char* buff, size_t buff_size)
{
    //TODO: Implement it.
}

void tio_updater_set_app(
    tio_updater_t* updater,
    const char* app_id,
    const char* host)
{
    //TODO: Implement it.
}

void tio_updater_set_interval(
    tio_updater_t* updater,
    size_t update_interval)
{
    //TODO: Implement it.
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