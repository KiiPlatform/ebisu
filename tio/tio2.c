#include <string.h>
#include "tio2.h"
#include "tio.h"
#include "kii.h"
#include "khc.h"
#include "tio_impl.h"
#include "command_parser.h"

const char TIO_TASK_NAME_UPDATE_STATE[] = "task_update_state";

void tio_handler_init(tio_handler_t* handler)
{
    handler->_kii._author.author_id[0] = '\0';
    handler->_kii._author.access_token[0] = '\0';
}

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

void tio_handler_set_cb_delay_ms(
    tio_handler_t* handler,
    KII_DELAY_MS cb_delay_ms)
{
    handler->_kii.delay_ms_cb = cb_delay_ms;
}

void tio_handler_set_cb_err(
    tio_handler_t* handler,
    TIO_CB_ERR cb_err,
    void* userdata)
{
    handler->_cb_err = cb_err;
    handler->_cb_err_data = userdata;
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
    kii_init(&handler->_kii, host, app_id);
}

static void _cb_receive_push(char* palyload, size_t payload_length, void* userdata) {
    tio_handler_t* handler = (tio_handler_t*)userdata;
    tio_code_t handle_res = _handle_command(handler, palyload, payload_length);
    if (handler->_cb_err != NULL) {
        handler->_cb_err(handle_res, "Failed to handle command", handler->_cb_err_data);
    }
}

tio_code_t tio_handler_start(
    tio_handler_t* handler,
    const tio_author_t* author,
    TIO_CB_ACTION cb_action,
    void* userdata)
{
    if (author != NULL) {
        handler->_kii._author = *author;
    }
    handler->_cb_action = cb_action;
    handler->_cb_action_data = userdata;
    kii_code_t res = kii_start_push_routine(
        &handler->_kii,
        handler->_keep_alive_interval,
        _cb_receive_push,
        (void*)handler);
    return _tio_convert_code(res);
}

void tio_updater_init(tio_updater_t* updater)
{
    updater->_kii._author.author_id[0] = '\0';
    updater->_kii._author.access_token[0] = '\0';
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

void tio_updater_set_cb_error(
    tio_updater_t* updater,
    TIO_CB_ERR cb_err,
    void* userdata)
{
    updater->_cb_err = cb_err;
    updater->_cb_err_data = userdata;
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
    kii_init(&updater->_kii, host, app_id);
}

void tio_updater_set_interval(
    tio_updater_t* updater,
    size_t update_interval)
{
    updater->_update_interval = update_interval;
}

static void* _update_state(void* data) {
    tio_updater_t* updater = (tio_updater_t*)data;
    while(1) {
        updater->_kii.delay_ms_cb(updater->_update_interval * 1000);
        size_t state_size = updater->_cb_state_size(updater->_cb_state_size_data);
        if (state_size > 0) {
            kii_code_t res = kii_ti_put_state(
                &updater->_kii,
                state_size,
                updater->_state_reader,
                updater->_state_reader_data,
                NULL);
            if (res != KII_ERR_OK) {
                tio_code_t code = _tio_convert_code(res);
                if (updater->_cb_err != NULL) {
                    updater->_cb_err(code, "Failed to upload state", updater->_cb_err_data);
                }
            }
        }
    }
    return NULL;
}

tio_code_t tio_updater_start(
    tio_updater_t* updater,
    const tio_author_t* author,
    TIO_CB_SIZE cb_state_size,
    void* cb_state_size_data,
    TIO_CB_READ state_reader,
    void* state_reader_data)
{
    if (author != NULL) {
        updater->_kii._author = *author;
    }
 
    updater->_cb_state_size = cb_state_size;
    updater->_cb_state_size_data = cb_state_size_data;

    updater->_state_reader = state_reader;
    updater->_state_reader_data = state_reader_data;

    kii_task_code_t res = updater->_kii.task_create_cb(
        TIO_TASK_NAME_UPDATE_STATE,
        _update_state,
        (void*)updater);

    if (res != KII_TASKC_OK) {
        return TIO_ERR_CREATE_TASK;
    }
    return TIO_ERR_OK;
}
