#ifndef __tio__
#define __tio__

#ifdef __cplusplus
extern "C" {
#endif

#include "kii.h"
#include "khc.h"
#include "jkii.h"

extern const char TIO_TASK_NAME_UPDATE_STATE[];

typedef kii_bool_t tio_bool_t;

typedef kii_author_t tio_author_t;

typedef enum tio_code_t {
    TIO_ERR_OK,
    TIO_ERR_SOCK_CONNECT,
    TIO_ERR_SOCK_SEND,
    TIO_ERR_SOCK_RECV,
    TIO_ERR_SOCK_CLOSE,
    TIO_ERR_WRITE_CALLBACK,
    TIO_ERR_HEADER_CALLBACK,
    TIO_ERR_ALLOCATION,
    TIO_ERR_TOO_LARGE_DATA,
    TIO_ERR_RESP_STATUS,
    TIO_ERR_PARSE_JSON,
    TIO_ERR_CREATE_TASK,
    TIO_ERR_FAIL
} tio_code_t;

typedef enum tio_data_type_t {
    TIO_TYPE_NULL,
    TIO_TYPE_BOOLEAN,
    TIO_TYPE_INTEGER,
    TIO_TYPE_DOUBLE,
    TIO_TYPE_STRING,
    TIO_TYPE_OBJECT,
    TIO_TYPE_ARRAY
} tio_data_type_t;

typedef struct tio_action_value_t {
    tio_data_type_t type;
    union {
        long long_value;
        double double_value;
        tio_bool_t bool_value;
        const char *opaque_value;
    } param;
    size_t opaque_value_length;
} tio_action_value_t;

typedef struct tio_action_t {
    const char* alias;
    size_t alias_length;
    const char* action_name;
    size_t action_name_length;
    tio_action_value_t action_value;
} tio_action_t;

typedef struct tio_action_err_t {
    char err_message[64];
} tio_action_err_t;

typedef size_t (*TIO_CB_SIZE)(void* userdata);
typedef size_t (*TIO_CB_READ)(char *buffer, size_t size, void *userdata);
typedef tio_bool_t (*TIO_CB_ACTION)(tio_action_t* action, tio_action_err_t* err, void* userdata);
typedef void (*TIO_CB_ERR)(tio_code_t code, const char* err_message, void* userdata);

typedef struct tio_handler_t {
    TIO_CB_ACTION _cb_action;
    void* _cb_action_data;
    TIO_CB_ERR _cb_err;
    void* _cb_err_data;
    kii_t _kii;
    char* _http_buff;
    size_t _http_buff_size;
    char* _mqtt_buff;
    size_t _mqtt_buff_size;
    size_t _keep_alive_interval;
} tio_handler_t;

typedef struct tio_updater_t {
    TIO_CB_SIZE _cb_state_size;
    void* _cb_state_size_data;
    TIO_CB_READ _state_reader;
    void* _state_reader_data;
    TIO_CB_ERR _cb_err;
    void* _cb_err_data;
    kii_t _kii;
    size_t _update_interval;
} tio_updater_t;


void tio_handler_init(tio_handler_t* handler);

void tio_handler_set_cb_sock_connect_http(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_handler_set_cb_sock_send_http(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_handler_set_cb_sock_recv_http(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_handler_set_cb_sock_close_http(tio_handler_t* handler, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

/**
 * \brief Set buffer used to construct/ parse HTTP request/ response.

 * This method must be called and set valid buffer before calling method before calling 
 * tio_handler_start().
 * The buffer is used to serialize/ deserialize JSON.

 * You can change the size of buffer depending on the request/ response size.
 * Typically, 1024 bytes is enough. However it varies depending on your data schema used to define
 * thing properties/ command. Avoid defining large thing properties/ command if you need to reduce memory usage.

 * Memory used by the buffer can be safely freed after you've terminated tio_handler task.

 * \param [out] handler instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_handler_set_http_buff(tio_handler_t* handler, char* buff, size_t buff_size);

/**
 * \brief Set stream buffer.
 * Stream buffer is used store part of HTTP body when
 * reading/ writing it from the network.

 * If this method is not called or set NULL to the buffer,
 * tio_handler allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by tio_handler is 1024 bytes.

 * You can change the size of buffer depending on your request/ response size.
 * It must be enough large to store size line in chunked encoded message.
 * However, you may use much larger buffer since size line might require very small buffer
 * as it consists of HEX size and CRLF for the better performance.

 * If you set the buffer by the method, the method must be called before tio_handler_start().
 * and memory used by the buffer can be safely freed after you've terminated tio_handler task.

 * \param [out] handler instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_handler_set_stream_buff(tio_handler_t* handler, char* buff, size_t buff_size);

/**
 * \brief Set response header buffer.

 * The buffer is used to store single HTTP response header.
 * If this method is not called or set NULL to the buffer,
 * tio_handler allocates memory of response header buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by tio_handler is 256 bytes.

 * If header is larger than the buffer, the header is skipped and not parsed.
 * tio_handler needs to parse Status Line, Content-Length and Transfer-Encoding header.
 * The buffer must have enough size to store those headers. 256 bytes would be enough.
 * If you set the buffer by the method, the method must be called before calling tio_handler_start()
 * and memory used by the buffer can be safely freed after you've terminated tio_handler task.

 * \param [out] handler instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_handler_set_resp_header_buff(tio_handler_t* handler, char* buff, size_t buff_size);

void tio_handler_set_cb_sock_connect_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_handler_set_cb_sock_send_mqtt(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_handler_set_cb_sock_recv_mqtt(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_handler_set_cb_sock_close_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

void tio_handler_set_mqtt_to_sock_recv(tio_handler_t* handler, unsigned int to_sock_recv_sec);
void tio_handler_set_mqtt_to_sock_send(tio_handler_t* handler, unsigned int to_sock_send_sec);

void tio_handler_set_cb_task_create(tio_handler_t* handler, KII_TASK_CREATE cb_task_create);
void tio_handler_set_cb_delay_ms(tio_handler_t* handler, KII_DELAY_MS cb_delay_ms);

void tio_handler_set_cb_err(tio_handler_t* handler, TIO_CB_ERR cb_err, void* userdata);

/**
 * \brief Set buffer used to parse MQTT message.

 * This method must be called and set valid buffer before calling method
 * tio_handler_start()
 * The buffer is used to parse MQTT message.

 * You can change the size of buffer depending on the request/ response size.
 * It must be enough large to store whole message send by MQTT.
 * Typically, 1024 bytes is enough.
 * However it varies depending on your data schema used to define Commands.
 * Avoid defining large Commands.

 * Memory used by the buffer can be safely freed after you've terminated tio_handler task.

 * \param [out] handler instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_handler_set_mqtt_buff(tio_handler_t* handler, char* buff, size_t buff_size);

void tio_handler_set_keep_alive_interval(tio_handler_t* handler, size_t keep_alive_interval);

void tio_handler_set_app(tio_handler_t* handler, const char* app_id, const char* host);

void tio_handler_set_json_parser_resource(tio_handler_t* handler, jkii_resource_t* resource);

void tio_handler_set_json_parser_resource_cb(
    tio_handler_t* handler,
    JKII_RESOURCE_ALLOC_CB alloc_cb,
    JKII_RESOURCE_FREE_CB free_cb);

/**
 * \brief Set custom memory allocator for the linked list used to constuct request headers of HTTP.

 * If this method is not called, default memory allocator using malloc/ free is used.
 * Notice: You need to call this method after
 * tio_handler_set_app(tio_handler_t*, const char*, const char*) since the
 * tio_handler_set_app() method has side effect resetting to default memory allocator.
 */
void tio_handler_set_slist_resource_cb(
    tio_handler_t* handler,
    KHC_SLIST_ALLOC_CB alloc_cb,
    KHC_SLIST_FREE_CB free_cb,
    void* alloc_cb_data,
    void* free_cb_data
);

tio_code_t tio_handler_onboard(
    tio_handler_t* handler,
    const char* vendor_thing_id,
    const char* password,
    const char* thing_type,
    const char* firmware_version,
    const char* layout_position,
    const char* thing_properties
);

const tio_author_t* tio_handler_get_author(
    tio_handler_t* handler
);

tio_code_t tio_handler_start(
    tio_handler_t* handler,
    const tio_author_t* author,
    TIO_CB_ACTION cb_action,
    void* userdata);

void tio_updater_init(tio_updater_t* updater);

void tio_updater_set_cb_sock_connect(tio_updater_t* updater, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
void tio_updater_set_cb_sock_send(tio_updater_t* updater, KHC_CB_SOCK_SEND cb_send, void* userdata);
void tio_updater_set_cb_sock_recv(tio_updater_t* updater, KHC_CB_SOCK_RECV cb_recv, void* userdata);
void tio_updater_set_cb_sock_close(tio_updater_t* updater, KHC_CB_SOCK_CLOSE cb_close, void* userdata);

void tio_updater_set_cb_task_create(tio_updater_t* updater, KII_TASK_CREATE cb_task_create);
void tio_updater_set_cb_delay_ms(tio_updater_t* updater, KII_DELAY_MS cb_delay_ms);

void tio_updater_set_cb_error(tio_updater_t* updater, TIO_CB_ERR cb_err, void* userdata);

/**
 * \brief Set buffer used to construct/ parse HTTP request/ response.

 * This method must be called and set valid buffer before calling method before calling 
 * tio_updater_start().
 * The buffer is used to serialize/ deserialize JSON.

 * You can change the size of buffer depending on the request/ response size.
 * Typically, 1024 bytes is enough.
 * Not that for uploading state, the buffer is not used and stream based TIO_CB_READ callback is used.
 * You don't have to take account into the size of the state.

 * Memory used by the buffer can be safely freed after you've terminated tio_updater task.

 * \param [out] updater instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_updater_set_buff(tio_updater_t* updater, char* buff, size_t buff_size);

/**
 * \brief Set stream buffer.
 * Stream buffer is used store part of HTTP body when
 * reading/ writing it from the network.

 * If this method is not called or set NULL to the buffer,
 * tio_updater allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by tio_updater is 1024 bytes.

 * You can change the size of buffer depending on your request/ response size.
 * It must be enough large to store size line in chunked encoded message.
 * However, you may use much larger buffer since size line might require very small buffer
 * as it consists of HEX size and CRLF for the better performance.

 * If you set the buffer by the method, the method must be called before tio_updater_start().
 * and memory used by the buffer can be safely freed after you've terminated tio_updater task.

 * \param [out] updater instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_updater_set_stream_buff(tio_updater_t* updater, char* buff, size_t buff_size);

/**
 * \brief Set response header buffer.

 * The buffer is used to store single HTTP response header.
 * If this method is not called or set NULL to the buffer,
 * tio_updater allocates memory of response header buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by tio_updater is 256 bytes.

 * If header is larger than the buffer, the header is skipped and not parsed.
 * tio_handler needs to parse Status Line, Content-Length and Transfer-Encoding header.
 * The buffer must have enough size to store those headers. 256 bytes would be enough.
 * If you set the buffer by the method, the method must be called before calling tio_updater_start()
 * and memory used by the buffer can be safely freed after you've terminated tio_updater task.

 * \param [out] updater instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void tio_updater_set_resp_header_buff(tio_updater_t* updater, char* buff, size_t buff_size);

void tio_updater_set_app(tio_updater_t* updater, const char* app_id, const char* host);

void tio_updater_set_interval(tio_updater_t* updater, size_t update_interval);

void tio_updater_set_json_parser_resource(tio_updater_t* updater, jkii_resource_t* resource);

void tio_updater_set_json_parser_resource_cb(
    tio_updater_t* updater,
    JKII_RESOURCE_ALLOC_CB alloc_cb,
    JKII_RESOURCE_FREE_CB free_cb);

/**
 * \brief Set custom memory allocator for the linked list used to constuct request headers of HTTP.

 * If this method is not called, default memory allocator using malloc/ free is used.
 * Notice: You need to call this method after
 * tio_updater_set_app(tio_updater_t*, const char*, const char*) since the
 * tio_updater_set_app() method has side effect resetting to default memory allocator.
 */
void tio_updater_set_slist_resource_cb(
    tio_updater_t* updater,
    KHC_SLIST_ALLOC_CB alloc_cb,
    KHC_SLIST_FREE_CB free_cb,
    void* alloc_cb_data,
    void* free_cb_data
);

tio_code_t tio_updater_onboard(
    tio_updater_t* updater,
    const char* vendor_thing_id,
    const char* password,
    const char* thing_type,
    const char* firmware_version,
    const char* layout_position,
    const char* thing_properties
);

const tio_author_t* tio_updater_get_author(
    tio_updater_t* updater
);

tio_code_t tio_updater_start(
    tio_updater_t* updater,
    const tio_author_t* author,
    TIO_CB_SIZE cb_state_size,
    void* cb_state_size_data,
    TIO_CB_READ state_reader,
    void* state_reader_data);

#ifdef __cplusplus
}
#endif

#endif
