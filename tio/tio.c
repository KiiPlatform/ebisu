#include <string.h>
#include "tio.h"
#include "kii.h"
#include "kii_task_callback.h"
#include "khc.h"
#include "tio_impl.h"
#include "command_parser.h"

const char TIO_TASK_NAME_UPDATE_STATE[] = "task_update_state";

void _convert_task_info(kii_mqtt_task_info* mqtt_info, tio_handler_task_info_t* task_info) {
    task_info->error = mqtt_info->error;
    task_info->task_state = mqtt_info->task_state;
}

void _task_exit(void* task_info, void* userdata) {
    kii_mqtt_task_info* mqtt_info = (kii_mqtt_task_info*) task_info;
    tio_handler_task_info_t tio_task_info;
    _convert_task_info(mqtt_info, &tio_task_info);
    tio_handler_t* handler = (tio_handler_t*)userdata;
    if (handler->_cb_task_exit != NULL) {
        handler->_cb_task_exit(&tio_task_info, handler->_task_exit_data);
    }
}

tio_bool_t _task_continue(void* task_info, void* userdata) {
    kii_mqtt_task_info* mqtt_info = (kii_mqtt_task_info*) task_info;
    tio_handler_task_info_t tio_task_info;
    _convert_task_info(mqtt_info, &tio_task_info);
    tio_handler_t* handler = (tio_handler_t*)userdata;
    if (handler->_cb_task_continue != NULL) {
        return handler->_cb_task_continue(&tio_task_info, handler->_task_continue_data);
    } else {
        return KII_TRUE;
    }
}

void tio_handler_init(tio_handler_t* handler)
{
    kii_init(&handler->_kii);
    handler->_cb_action = NULL;
    handler->_cb_action_data = NULL;
    handler->_cb_err = NULL;
    handler->_cb_err_data = NULL;
    handler->_cb_push = NULL;
    handler->_cb_push_data = NULL;
    handler->_keep_alive_interval = 300;
    handler->_cb_task_continue = NULL;
    handler->_task_continue_data = NULL;
    handler->_cb_task_exit = NULL;
    handler->_task_exit_data = NULL;
    kii_set_cb_task_continue(&handler->_kii, _task_continue, handler);
    kii_set_cb_task_exit(&handler->_kii, _task_exit, handler);
}

void tio_handler_set_cb_sock_connect_http(
        tio_handler_t* handler,
        KHC_CB_SOCK_CONNECT cb_connect,
        void* userdata)
{
    kii_set_cb_http_sock_connect(&handler->_kii, cb_connect, userdata);
}

void tio_handler_set_cb_sock_send_http(
        tio_handler_t* handler,
        KHC_CB_SOCK_SEND cb_send,
        void* userdata)
{
    kii_set_cb_http_sock_send(&handler->_kii, cb_send, userdata);
}

void tio_handler_set_cb_sock_recv_http(
        tio_handler_t* handler,
        KHC_CB_SOCK_RECV cb_recv,
        void* userdata)
{
    kii_set_cb_http_sock_recv(&handler->_kii, cb_recv, userdata);
}

void tio_handler_set_cb_sock_close_http(
        tio_handler_t* handler,
        KHC_CB_SOCK_CLOSE cb_close,
        void* userdata)
{
    kii_set_cb_http_sock_close(&handler->_kii, cb_close, userdata);
}

void tio_handler_set_http_buff(
        tio_handler_t* handler,
        char* buff,
        size_t buff_size)
{
    kii_set_buff(&handler->_kii, buff, buff_size);
}

void tio_handler_set_stream_buff(tio_handler_t* handler, char* buff, size_t buff_size) {
    kii_set_stream_buff(&handler->_kii, buff, buff_size);
}

void tio_handler_set_resp_header_buff(tio_handler_t* handler, char* buff, size_t buff_size) {
    kii_set_resp_header_buff(&handler->_kii, buff, buff_size);
}

void tio_handler_set_cb_sock_connect_mqtt(
        tio_handler_t* handler,
        KHC_CB_SOCK_CONNECT cb_connect,
        void* userdata)
{
    kii_set_cb_mqtt_sock_connect(&handler->_kii, cb_connect, userdata);
}

void tio_handler_set_cb_sock_send_mqtt(
        tio_handler_t* handler,
        KHC_CB_SOCK_SEND cb_send,
        void* userdata)
{
    kii_set_cb_mqtt_sock_send(&handler->_kii, cb_send, userdata);
}

void tio_handler_set_cb_sock_recv_mqtt(
        tio_handler_t* handler,
        KHC_CB_SOCK_RECV cb_recv,
        void* userdata)
{
    kii_set_cb_mqtt_sock_recv(&handler->_kii, cb_recv, userdata);
}

void tio_handler_set_cb_sock_close_mqtt(
        tio_handler_t* handler,
        KHC_CB_SOCK_CLOSE cb_close,
        void* userdata)
{
    kii_set_cb_mqtt_sock_close(&handler->_kii, cb_close, userdata);
}

void tio_handler_set_mqtt_to_sock_recv(tio_handler_t* handler, unsigned int to_sock_recv_sec)
{
    kii_set_mqtt_to_sock_recv(&handler->_kii, to_sock_recv_sec);
}

void tio_handler_set_mqtt_to_sock_send(tio_handler_t* handler, unsigned int to_sock_send_sec)
{
    kii_set_mqtt_to_sock_send(&handler->_kii, to_sock_send_sec);
}

void tio_handler_set_cb_task_create(
        tio_handler_t* handler,
        KII_CB_TASK_CREATE cb_task_create,
        void* userdata)
{
    kii_set_cb_task_create(&handler->_kii, cb_task_create, userdata);
}

void tio_handler_set_cb_task_continue(tio_handler_t* handler, KII_CB_TASK_CONTINUE cb_continue, void* userdata)
{
    handler->_cb_task_continue = cb_continue;
    handler->_task_continue_data = userdata;
}

void tio_handler_set_cb_task_exit(tio_handler_t* handler, KII_CB_TASK_EXIT cb_exit, void* userdata)
{
    handler->_cb_task_exit = cb_exit;
    handler->_task_exit_data = userdata;
}

void tio_handler_set_cb_delay_ms(
        tio_handler_t* handler,
        KII_CB_DELAY_MS cb_delay_ms,
        void* userdata)
{
    kii_set_cb_delay_ms(&handler->_kii, cb_delay_ms, userdata);
}

void tio_handler_set_cb_err(
        tio_handler_t* handler,
        TIO_CB_ERR cb_err,
        void* userdata)
{
    handler->_cb_err = cb_err;
    handler->_cb_err_data = userdata;
}

void tio_handler_set_cb_push(
        tio_handler_t* handler,
        TIO_CB_PUSH cb_push,
        void* userdata)
{
    handler->_cb_push = cb_push;
    handler->_cb_push_data = userdata;
}

void tio_handler_set_mqtt_buff(
        tio_handler_t* handler,
        char* buff,
        size_t buff_size)
{
    kii_set_mqtt_buff(&handler->_kii, buff, buff_size);
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
    kii_set_site(&handler->_kii, host);
    kii_set_app_id(&handler->_kii, app_id);
}

static void _cb_receive_push(const char* payload, size_t payload_length, void* userdata) {
    tio_handler_t* handler = (tio_handler_t*)userdata;
    tio_bool_t skip = KII_FALSE;
    if (handler->_cb_push != NULL) {
        skip = handler->_cb_push(payload, payload_length, handler->_cb_push_data);
    }
    if (skip == KII_FALSE) {
        tio_code_t handle_res = _handle_command(handler, payload, payload_length);
        if (handle_res != TIO_ERR_OK && handler->_cb_err != NULL) {
            handler->_cb_err(handle_res, "Failed to handle command", handler->_cb_err_data);
        }
    }
}

void tio_handler_set_json_parser_resource(
        tio_handler_t* handler,
        jkii_resource_t* resource)
{
    kii_set_json_parser_resource(&handler->_kii, resource);
}

void tio_handler_set_cb_json_parser_resource(
        tio_handler_t* handler,
        JKII_CB_RESOURCE_ALLOC cb_alloc,
        JKII_CB_RESOURCE_FREE cb_free)
{
    kii_set_cb_json_parser_resource(&handler->_kii, cb_alloc, cb_free);
}

void tio_handler_set_cb_slist_resource(
        tio_handler_t* handler,
        KHC_CB_SLIST_ALLOC cb_alloc,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_alloc_data,
        void* cb_free_data)
{
    kii_set_cb_slist_resource(&handler->_kii, cb_alloc, cb_free, cb_alloc_data, cb_free_data);
}

tio_code_t tio_handler_onboard(
        tio_handler_t* handler,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties)
{
    kii_code_t ret =kii_ti_onboard(
            &handler->_kii,
            vendor_thing_id,
            password,
            thing_type,
            firmware_version,
            layout_position,
            thing_properties
            );
    return _tio_convert_code(ret);
}

const tio_author_t* tio_handler_get_author(tio_handler_t* handler)
{
    return &handler->_kii._author;
}

tio_code_t tio_handler_start(
        tio_handler_t* handler,
        const tio_author_t* author,
        TIO_CB_ACTION cb_action,
        void* userdata)
{
    handler->_kii._author = *author;

    handler->_cb_action = cb_action;
    handler->_cb_action_data = userdata;
    kii_code_t res = kii_start_push_task(
            &handler->_kii,
            handler->_keep_alive_interval,
            _cb_receive_push,
            (void*)handler);
    return _tio_convert_code(res);
}

void tio_updater_init(tio_updater_t* updater)
{
    kii_init(&updater->_kii);
    updater->_cb_state_size = NULL;
    updater->_cb_state_size_data = NULL;
    updater->_state_reader = NULL;
    updater->_state_reader_data = NULL;
    updater->_cb_err = NULL;
    updater->_cb_err_data = NULL;
    updater->_update_interval = 0;
    updater->_cb_task_continue = NULL;
    updater->_task_continue_data = NULL;
    updater->_cb_task_exit = NULL;
    updater->_task_exit_data = NULL;
}

void tio_updater_set_cb_sock_connect(
        tio_updater_t* updater,
        KHC_CB_SOCK_CONNECT cb_connect,
        void* userdata)
{
    kii_set_cb_http_sock_connect(&updater->_kii, cb_connect, userdata);
}

void tio_updater_set_cb_sock_send(
        tio_updater_t* updater,
        KHC_CB_SOCK_SEND cb_send,
        void* userdata)
{
    kii_set_cb_http_sock_send(&updater->_kii, cb_send, userdata);
}

void tio_updater_set_cb_sock_recv(
        tio_updater_t* updater,
        KHC_CB_SOCK_RECV cb_recv,
        void* userdata)
{
    kii_set_cb_http_sock_recv(&updater->_kii, cb_recv, userdata);
}

void tio_updater_set_cb_sock_close(
        tio_updater_t* updater,
        KHC_CB_SOCK_CLOSE cb_close,
        void* userdata)
{
    kii_set_cb_http_sock_close(&updater->_kii, cb_close, userdata);
}

void tio_updater_set_cb_task_create(
        tio_updater_t* updater,
        KII_CB_TASK_CREATE cb_task_create,
        void* userdata)
{
    kii_set_cb_task_create(&updater->_kii, cb_task_create, userdata);
}

void tio_updater_set_cb_task_continue(tio_updater_t* updater, KII_CB_TASK_CONTINUE cb_continue, void* userdata)
{
    updater->_cb_task_continue = cb_continue;
    updater->_task_continue_data = userdata;
}

void tio_updater_set_cb_task_exit(tio_updater_t* updater, KII_CB_TASK_EXIT cb_exit, void* userdata)
{
    updater->_cb_task_exit = cb_exit;
    updater->_task_exit_data = userdata;
}

void tio_updater_set_cb_delay_ms(
        tio_updater_t* updater,
        KII_CB_DELAY_MS cb_delay_ms,
        void* userdata)
{
    kii_set_cb_delay_ms(&updater->_kii, cb_delay_ms, userdata);
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

void tio_updater_set_stream_buff(tio_updater_t* updater, char* buff, size_t buff_size) {
    kii_set_stream_buff(&updater->_kii, buff, buff_size);
}

void tio_updater_set_resp_header_buff(tio_updater_t* updater, char* buff, size_t buff_size) {
    kii_set_resp_header_buff(&updater->_kii, buff, buff_size);
}

void tio_updater_set_app(
        tio_updater_t* updater,
        const char* app_id,
        const char* host)
{
    kii_set_site(&updater->_kii, host);
    kii_set_app_id(&updater->_kii, app_id);
}

void tio_updater_set_interval(
        tio_updater_t* updater,
        size_t update_interval)
{
    updater->_update_interval = update_interval;
}

static void* _update_state(void* data) {
    tio_updater_t* updater = (tio_updater_t*)data;
    size_t interval_remain_sec = updater->_update_interval;
    while(1) {
        // Ask to continue every 1 second.
        const size_t interval_chk_cnt_sec = 1;
        if (interval_remain_sec > interval_chk_cnt_sec) {
            if (updater->_cb_task_continue != NULL) {
                tio_bool_t cont = updater->_cb_task_continue(NULL, updater->_task_continue_data);
                if (cont != KII_TRUE) {
                    break;
                }
            }
            updater->_kii._cb_delay_ms(interval_chk_cnt_sec * 1000, updater->_kii._delay_ms_data);
            interval_remain_sec -= interval_chk_cnt_sec;
        } else {
            updater->_kii._cb_delay_ms(interval_remain_sec * 1000, updater->_kii._delay_ms_data);
            interval_remain_sec = updater->_update_interval;
            size_t state_size = updater->_cb_state_size(updater->_cb_state_size_data);
            if (state_size > 0) {
                kii_code_t res = kii_ti_put_state(
                        &updater->_kii,
                        updater->_state_reader,
                        updater->_state_reader_data,
                        NULL,
                        NULL,
                        NULL);
                if (res != KII_ERR_OK) {
                    tio_code_t code = _tio_convert_code(res);
                    if (updater->_cb_err != NULL) {
                        updater->_cb_err(code, "Failed to upload state", updater->_cb_err_data);
                    }
                }
            }
        }


    }
    if (updater->_cb_task_exit != NULL) {
        updater->_cb_task_exit(NULL, updater->_task_exit_data);
    }
    return NULL;
}

void tio_updater_set_json_parser_resource(
        tio_updater_t* updater,
        jkii_resource_t* resource)
{
    kii_set_json_parser_resource(&updater->_kii, resource);
}

void tio_updater_set_cb_json_parser_resource(
        tio_updater_t* updater,
        JKII_CB_RESOURCE_ALLOC cb_alloc,
        JKII_CB_RESOURCE_FREE cb_free)
{
    kii_set_cb_json_parser_resource(&updater->_kii, cb_alloc, cb_free);
}

void tio_updater_set_cb_slist_resource(
        tio_updater_t* updater,
        KHC_CB_SLIST_ALLOC cb_alloc,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_alloc_data,
        void* cb_free_data)
{
    kii_set_cb_slist_resource(&updater->_kii, cb_alloc, cb_free, cb_alloc_data, cb_free_data);
}

tio_code_t tio_updater_onboard(
        tio_updater_t* updater,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties)
{
    kii_code_t ret =kii_ti_onboard(
            &updater->_kii,
            vendor_thing_id,
            password,
            thing_type,
            firmware_version,
            layout_position,
            thing_properties
            );
    return _tio_convert_code(ret);
}

const tio_author_t* tio_updater_get_author(
        tio_updater_t* updater)
{
    return &updater->_kii._author;
}

tio_code_t tio_updater_start(
        tio_updater_t* updater,
        const tio_author_t* author,
        TIO_CB_SIZE cb_state_size,
        void* cb_state_size_data,
        TIO_CB_READ state_reader,
        void* state_reader_data)
{
    updater->_kii._author = *author;

    updater->_cb_state_size = cb_state_size;
    updater->_cb_state_size_data = cb_state_size_data;

    updater->_state_reader = state_reader;
    updater->_state_reader_data = state_reader_data;

    kii_task_code_t res = updater->_kii._cb_task_create(
            TIO_TASK_NAME_UPDATE_STATE,
            _update_state,
            (void*)updater,
            updater->_kii._task_create_data);

    if (res != KII_TASKC_OK) {
        return TIO_ERR_CREATE_TASK;
    }
    return TIO_ERR_OK;
}
