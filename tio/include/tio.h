#ifndef __tio__
#define __tio__

#ifdef __cplusplus
extern "C" {
#endif

#include "kii.h"
#include "khc.h"
#include "jkii.h"

extern const char TIO_TASK_NAME_UPDATE_STATE[];

/**
 * \brief Boolean type.
 */
typedef kii_bool_t tio_bool_t;

/**
 * \brief Author of API.
 */
typedef kii_author_t tio_author_t;

/**
 * \brief Error Codes used in tio.
 */
typedef enum tio_code_t {
    TIO_ERR_OK, /**< \brief Succeeded */
    TIO_ERR_SOCK_CONNECT, /**< \brief Socket error in connection. */
    TIO_ERR_SOCK_SEND, /**< \brief Socket error in sending data. */
    TIO_ERR_SOCK_RECV, /**< \brief Socket error in receiving data. */
    TIO_ERR_SOCK_CLOSE, /**< \brief Socket error in closing. */
    TIO_ERR_WRITE_CALLBACK, /**< \brief Error in write callback. */
    TIO_ERR_HEADER_CALLBACK, /**< \brief Error in header callback. */
    TIO_ERR_ALLOCATION, /**< \brief Error in memory allocation. */
    TIO_ERR_TOO_LARGE_DATA, /**< \brief Data is larger than expected. */
    TIO_ERR_RESP_STATUS, /**< \brief REST API returns error status code. */
    TIO_ERR_PARSE_JSON, /**< \brief Error in parsing JSON. */
    TIO_ERR_CREATE_TASK, /**< \brief Error in creating task. */
    TIO_ERR_FAIL  /**< \brief Other errors. */
} tio_code_t;

/**
 * \brief Data types.

 * Same as JSON data types except for having Integer and Double instead of Number.
 */
typedef enum tio_data_type_t {
    TIO_TYPE_NULL, /**< \brief NULL type. */
    TIO_TYPE_BOOLEAN, /**< \brief Boolean type. */
    TIO_TYPE_INTEGER, /**< \brief Integer type. */
    TIO_TYPE_DOUBLE, /**< \brief Double type. */
    TIO_TYPE_STRING, /**< \brief String type. */
    TIO_TYPE_OBJECT, /**< \brief Object type. */
    TIO_TYPE_ARRAY /**< \brief Array type. */
} tio_data_type_t;

/**
 * \brief Represents value of the action.
 */
typedef struct tio_action_value_t {
    tio_data_type_t type; /**< \brief Data type of the value. */
    /**
     *  \brief union stores value.
     *
     * if type is TIO_TYPE_STRING, TIO_TYPE_OBJECT or TIO_TYPE_ARRAY,
     * opaque_value is the pointer to it's JSON string representation.
     * You need to use opaque_value_length to determine the length of the value
     * since it might not be null terminated.
     */
    union {
        long long_value; /**< Value stored when type is TIO_TYPE_INTEGER */
        double double_value; /**< Value stored when type is TIO_TYPE_DOUBLE */
        tio_bool_t bool_value; /**< Value stored when type is TIO_TYPE_BOOLEAN */
        const char *opaque_value; /**< Value stored when type is TIO_TYPE_STRING, TIO_TYPE_OBJECT or TIO_TYPE_ARRAY */
    } param;
    /**
     * \brief indicate length of opaque_value in case type is
     * TIO_TYPE_STRING, TIO_TYPE_OBJECT or TIO_TYPE_ARRAY.
     */
    size_t opaque_value_length;
} tio_action_value_t;

/**
 * \brief Represents action.
 */
typedef struct tio_action_t {
    /**
     * \brief Name of the alias.
     * You need to use alias_length field to determine the length. It might not be null terminated.
     */
    const char* alias;
    /**
     * \brief Length of the alias name.
     */
    size_t alias_length;
    /**
     * \brief Name of the action.
     * You need to use action_name_length field to determine the length. It might not be null terminated.
     */
    const char* action_name;
    /**
     * \brief Length of the action name.
     */
    size_t action_name_length;
    /**
     * \brief Value of the action.
     */
    tio_action_value_t action_value;
} tio_action_t;

/**
 * \brief Represents error.
 */
typedef struct tio_action_err_t {
    char err_message[64]; /**< \brief Error message (null terminated). */
} tio_action_err_t;

/**
 * \brief Callback asks for size of the state to be uploaded.
 *
 * \param userdata [inout] Context object pointer passed to tio_updater_start().
 * \return size of the state to be uploaded. if 0, TIO_CB_READ callback passed to tio_updater_start() is not called.
 */
typedef size_t (*TIO_CB_SIZE)(void* userdata);

/**
 * \brief Callback reads state.
 *
 * This callback would be called mutiple times until it returns 0.
 * Implementation should keep track of the total size already read and write rest data to the buffer.
 * \param buffer [out] Implementation must write the part of the state sequentially.
 * \param size [in] Size of the buffer.
 * \param userdata [inout] Context object pointer passed to tio_updater_start().
 * \return size of the state read. Returning 0 is required when all state has been read.
 */
typedef size_t (*TIO_CB_READ)(char *buffer, size_t size, void *userdata);

/**
 * \brief Callback handles action.
 *
 * Called when received remote control command from cloud.
 * Command may includes multiple actions. The callback is called per action.
 *
 * \param action [in] includes alias_name, action_name and action 
 */
typedef tio_bool_t (*TIO_CB_ACTION)(tio_action_t* action, tio_action_err_t* err, void* userdata);
/**
 * \brief Callback propagates error information.
 *
 * Called when error occurred.
 * Can be used for debugging and implementation is optional.
 *
 * \param [in] code Error code.
 * \param [in] err_message Error message.
 * \param [inout] userdata Context object pointer passed to tio_handler_set_cb_err()/ tio_updater_set_cb_err().
 */
typedef void (*TIO_CB_ERR)(tio_code_t code, const char* err_message, void* userdata);
/**
 * \brief Callback handles custom push notification.
 *
 * By default tio_handler handles remote controll command and ignores other kind of messages.
 * If you send message to the devices other than remote controll command, you can use this callback to handle them.
 * This callback is called before TIO_CB_ACTION callback, and if it returns KII_TRUE, tio_handler skips parsing message as remote controll command
 * and TIO_CB_ACTION callback won't be called.
 * if it returns KII_FALSE, tio_handler try to parse message as remote controll command and call TIO_CB_ACTION callback as well
 * if the message is remote controll command.
 * If you only needs to handle remote contoll command, you don't have to implement this callback and call tio_handler_set_cb_push().
 *
 * \param message Push message received from cloud.
 * Could be remote controll command or other message sent by Kii Topic/ Kii Bucket subscription.
 * \param message_length Length of the message.
 * \param userdata Context object pointer passed to tio_handler_set_cb_push().
 * \return KII_TRUE results skip handling message as remote controll command, KII_FALSE results try to handle message as remote controll command.
 */
typedef tio_bool_t (*TIO_CB_PUSH)(const char* message, size_t message_length, void* userdata);

/**
 * \brief Stores data/ callbacks used by tio_handler.
 */
typedef struct tio_handler_t {
    TIO_CB_ACTION _cb_action; /**< \private **/
    void* _cb_action_data; /**< \private **/
    TIO_CB_ERR _cb_err; /**< \private **/
    void* _cb_err_data; /**< \private **/
    TIO_CB_PUSH _cb_push; /**< \private **/
    void* _cb_push_data; /**< \private **/
    kii_t _kii; /**< \private **/
    size_t _keep_alive_interval; /**< \private **/
    KII_CB_TASK_CONTINUE _cb_task_continue; /**< \private **/
    void* _task_continue_data; /**< \private **/
    KII_CB_TASK_EXIT _cb_task_exit; /**< \private **/
    void* _task_exit_data; /**< \private **/
} tio_handler_t;

/**
 * \brief Indicates handler state.
 */
typedef struct {
    kii_mqtt_error error; /**< MQTT error code */
    kii_mqtt_task_state task_state; /**< Indicates processing phase of MQTT */
} tio_handler_task_info_t;

/**
 * \brief Stores data/ callbacks used by tio_updater.
 */
typedef struct tio_updater_t {
    TIO_CB_SIZE _cb_state_size; /**< \private **/
    void* _cb_state_size_data; /**< \private **/
    TIO_CB_READ _state_reader; /**< \private **/
    void* _state_reader_data; /**< \private **/
    TIO_CB_ERR _cb_err; /**< \private **/
    void* _cb_err_data; /**< \private **/
    kii_t _kii; /**< \private **/
    size_t _update_interval; /**< \private **/
    KII_CB_TASK_CONTINUE _cb_task_continue; /**< \private **/
    void* _task_continue_data; /**< \private **/
    KII_CB_TASK_EXIT _cb_task_exit; /**< \private **/
    void* _task_exit_data; /**< \private **/
} tio_updater_t;

/**
 * \brief tio_handler_t initializer.
 *
 * Must be called when start using tio_handler_t instance.
 *
 * \param handler [inout] instance.
 */
void tio_handler_init(tio_handler_t* handler);

/**
 * \brief set socket connect callback used for HTTP(S)
 *
 * \param handler [inout] instance.
 * \param cb_connect [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_connect.
 */
void tio_handler_set_cb_sock_connect_http(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
/**
 * \brief set socket send callback used for HTTP(S)
 *
 * \param handler [inout] instance.
 * \param cb_send [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_send.
 */
void tio_handler_set_cb_sock_send_http(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
/**
 * \brief set socket recv callback used for HTTP(S)
 *
 * \param handler [inout] instance.
 * \param cb_recv [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_recv.
 */
void tio_handler_set_cb_sock_recv_http(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
/**
 * \brief set socket close callback used for HTTP(S)
 *
 * \param handler [inout] instance.
 * \param cb_close [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_close.
 */
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

/**
 * \brief set socket connect callback used for MQTT(S)
 *
 * Note that socket used for MQTT must be blocking-mode and its recv/send timeout must be set by
 * tio_handler_set_mqtt_to_sock_recv()/ tio_handler_set_mqtt_to_sock_send() APIs.
 * It is necessary for sending pingReq message periodically to achieve Keep-Alive
 * since we don't require system clock APIs abstraction.
 * \param handler [inout] instance.
 * \param cb_connect [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_connect.
 */
void tio_handler_set_cb_sock_connect_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CONNECT cb_connect, void* userdata);
/**
 * \brief set socket send callback used for MQTT(S)
 *
 * Note that socket used for MQTT must be blocking-mode and its recv/send timeout must be set by
 * tio_handler_set_mqtt_to_sock_recv()/ tio_handler_set_mqtt_to_sock_send() APIs.
 * It is necessary for sending pingReq message periodically to achieve Keep-Alive
 * since we don't require system clock APIs abstraction.
 * \param handler [inout] instance.
 * \param cb_send [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_send.
 */
void tio_handler_set_cb_sock_send_mqtt(tio_handler_t* handler, KHC_CB_SOCK_SEND cb_send, void* userdata);
/**
 * \brief set socket recv callback used for MQTT(S)
 *
 * Note that socket used for MQTT must be blocking-mode and its recv/send timeout must be set by
 * tio_handler_set_mqtt_to_sock_recv()/ tio_handler_set_mqtt_to_sock_send() APIs.
 * It is necessary for sending pingReq message periodically to achieve Keep-Alive
 * since we don't require system clock APIs abstraction.
 * \param handler [inout] instance.
 * \param cb_recv [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_recv.
 */
void tio_handler_set_cb_sock_recv_mqtt(tio_handler_t* handler, KHC_CB_SOCK_RECV cb_recv, void* userdata);
/**
 * \brief set socket recv callback used for MQTT(S)
 *
 * Note that socket used for MQTT must be blocking-mode and its recv/send timeout must be set by
 * tio_handler_set_mqtt_to_sock_recv()/ tio_handler_set_mqtt_to_sock_send() APIs.
 * It is necessary for sending pingReq message periodically to achieve Keep-Alive
 * since we don't require system clock APIs abstraction.
 * \param handler [inout] instance.
 * \param cb_close [in] Callback function pointer.
 * \param userdata [in] Context object pointer passed to cb_close.
 */
void tio_handler_set_cb_sock_close_mqtt(tio_handler_t* handler, KHC_CB_SOCK_CLOSE cb_close, void* userdata);
/**
 * \brief Set timeout of receiving data from socket user for MQTT(S)
 *
 * This setting is mandatory to achieve MQTT keep-alive mechanism.
 * We use timeout instead of requirering system clock access to periodically send pingReq.
 * Socket recv implementation given to tio_handler_set_cb_sock_recv_mqtt()
 * must have same timeout specified by to_sock_recv_sec.
 *
 * \param handler [inout] instance.
 * \param to_sock_recv_sec [in] Socket recv timeout in seconds.
 */
void tio_handler_set_mqtt_to_sock_recv(tio_handler_t* handler, unsigned int to_sock_recv_sec);
/**
 * \brief Set timeout of receiving data from socket user for MQTT(S)
 *
 * This setting is mandatory to achieve MQTT keep-alive mechanism.
 * We use timeout instead of requirering system clock access to periodically send pingReq.
 * Socket send implementation given to tio_handler_set_cb_sock_send_mqtt()
 * must have same timeout specified by to_sock_send_sec.
 *
 * \param handler [inout] instance.
 * \param to_sock_recv_sec [in] Socket recv timeout in seconds.
 */
void tio_handler_set_mqtt_to_sock_send(tio_handler_t* handler, unsigned int to_sock_send_sec);

void tio_handler_set_cb_task_create(tio_handler_t* handler, KII_CB_TASK_CREATE cb_task_create, void* userdata);

/**
 * \brief set callback determines whether to continue or discontinue task.

 * If this method is not called or NULL is set, task exits only when un-recoverble error occurs.
 * If you need cancellation mechanism, you need to set this callback.
 * Terminate task without using this callback may cause memory leak.
 * This method must be called before calling tio_handler_start().

 * In case checking cancellation flag in continue_cb, the flag might be set by other task/ thread.
 * Implementation must ensure consistency of the flag by using Mutex, etc.

 * If un-recoverble error occurs, task exits the infinite loop and immediately calls KII_CB_TASK_EXIT callback if set.
 * In this case KII_CB_TASK_CONTINUE callback is not called.

 * \param tio_handler_t [out] tio_handler_t instance
 * \param continue_cb [in] Callback determines whether to continue or discontinue task.
 * If continue_cb returns KII_TRUE, task continues. Otherwise the task exits the infinite loop
 * and calls KII_CB_TASK_EXIT callback if set.
 * task_info argument type of the cb_continue (defined as void* in KII_CB_TASK_CONTINUE) is tio_handler_task_info*.
 * \param userdata [in] Context data pointer passed as second argument when cb_continue is called.
 */
void tio_handler_set_cb_task_continue(tio_handler_t* handler, KII_CB_TASK_CONTINUE cb_continue, void* userdata);

/**
 * \brief Callback called right before exit of tio_handler task.

 * Task exits when the task is discontinued by KII_CB_TASK_CONTINUE callback or
 * un-recoverble error occurs.
 * In exit_cb, you'll need to free memory used for buffers set by following APIs
 * - tio_handler_set_http_buff(),
 * - tio_handler_set_stream_buff(),
 * - tio_handler_set_resp_header_buff()
 * - tio_handler_set_mqtt_buff(),
 * and memory used for the userdata passed to following callbacks in case not yet freed.
 * - tio_handler_set_cb_sock_connect_http()
 * - tio_handler_set_cb_sock_send_http()
 * - tio_handler_set_cb_sock_recv_http()
 * - tio_handler_set_cb_sock_close_http()
 * - tio_handler_set_cb_sock_connect_mqtt()
 * - tio_handler_set_cb_sock_send_mqtt()
 * - tio_handler_set_cb_sock_recv_mqtt()
 * - tio_handler_set_cb_sock_close_mqtt()
 * - tio_handler_set_cb_task_continue()
 * - tio_handler_set_cb_task_exit()

 * In addition, you may need to call task/ thread termination API.
 * It depends on the task/ threading framework you used to create task/ thread.
 * After the exit_cb returned, task function immediately returns.

 * If this API is not called or set NULL,
 * task function immediately returns when task is discontinued or un-recoverble error occurs.

 * \param handler [out] tio_handler_t instance.
 * \param cb_exit [in] Called right before the exit.
 * task_info argument type of cb_exit (defined as void* in KII_CB_TASK_EXIT) is tio_handler_task_info*
 * \param userdata [in] Context data pointer passed as second argument when cb_exit is called.
 */
void tio_handler_set_cb_task_exit(tio_handler_t* handler, KII_CB_TASK_EXIT cb_exit, void* userdata);
void tio_handler_set_cb_delay_ms(tio_handler_t* handler, KII_CB_DELAY_MS cb_delay_ms, void* userdata);

void tio_handler_set_cb_err(tio_handler_t* handler, TIO_CB_ERR cb_err, void* userdata);

void tio_handler_set_cb_push(tio_handler_t* handler, TIO_CB_PUSH cb_push, void* userdata);

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

void tio_handler_set_cb_json_parser_resource(
    tio_handler_t* handler,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free);

/**
 * \brief Set custom memory allocator for the linked list used to constuct request headers of HTTP.

 * If this method is not called, default memory allocator using malloc/ free is used.
 * Notice: You need to call this method after
 * tio_handler_set_app(tio_handler_t*, const char*, const char*) since the
 * tio_handler_set_app() method has side effect resetting to default memory allocator.
 */
void tio_handler_set_cb_slist_resource(
    tio_handler_t* handler,
    KHC_CB_SLIST_ALLOC cb_alloc,
    KHC_CB_SLIST_FREE cb_free,
    void* cb_alloc_data,
    void* cb_free_data
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

void tio_updater_set_cb_task_create(tio_updater_t* updater, KII_CB_TASK_CREATE cb_task_create, void* userdata);

/**
 * \brief set callback determines whether to continue or discontinue task.

 * If this method is not called or NULL is set, task won't exit.
 * If you need cancellation mechanism, you need to set this callback.
 * Terminate task without using this callback may cause memory leak.
 * This method must be called before calling tio_updater_start().

 * In case checking cancellation flag in continue_cb, the flag might be set by other task/ thread.
 * Implementation must ensure consistency of the flag by using Mutex, etc.

 * \param updater [out] tio_updater_t instances
 * \param continue_cb [in] Callback determines whether to continue or discontinue task.
 * If continue_cb returns KII_TRUE, task continues. Otherwise the task exits the infinite loop
 * and calls KII_CB_TASK_EXIT callback if set.
 * task_info argument of the cb_continue (defined as void* in KII_CB_TASK_CONTINUE) is always NULL.
 * \param userdata [in] Context data pointer passed as second argument when cb_continue is called.
 */
void tio_updater_set_cb_task_continue(tio_updater_t* updater, KII_CB_TASK_CONTINUE cb_continue, void* userdata);

/**
 * \brief Callback called right before exit of tio_updater task.

 * Task exits when the task is discontinued by KII_CB_TASK_CONTINUE callback.
 * In exit_cb, you'll need to free memory used for buffers set by following APIs
 * - tio_updater_set_buff(),
 * - tio_updater_set_stream_buff(),
 * - tio_updater_set_resp_header_buff()
 * and memory used for the userdata passed to following callbacks in case not yet freed.
 * - tio_updater_set_cb_sock_connect()
 * - tio_updater_set_cb_sock_send()
 * - tio_updater_set_cb_sock_recv()
 * - tio_updater_set_cb_sock_close()
 * - tio_updater_set_cb_task_continue()
 * - tio_updater_set_cb_task_exit()

 * In addition, you may need to call task/ thread termination API.
 * It depends on the task/ threading framework you used to create task/ thread.
 * After the exit_cb returned, task function immediately returns.

 * If this API is not called or set NULL,
 * task function immediately returns when task is discontinued or un-recoverble error occurs.

 * \param updater [out] tio_updater_t instance
 * \param exit_cb [in] Callback called right befor exit.
 * task_info argument of the cb_exit (defind as void* in KII_CB_TASK_EXIT) function is always NULL.
 * \param userdata [in] Context data pointer passed as second argument when cb_exit is called.
 */
void tio_updater_set_cb_task_exit(tio_updater_t* updater, KII_CB_TASK_EXIT cb_exit, void* userdata);
void tio_updater_set_cb_delay_ms(tio_updater_t* updater, KII_CB_DELAY_MS cb_delay_ms, void* userdata);

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

void tio_updater_set_cb_json_parser_resource(
    tio_updater_t* updater,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free);

/**
 * \brief Set custom memory allocator for the linked list used to constuct request headers of HTTP.

 * If this method is not called, default memory allocator using malloc/ free is used.
 * Notice: You need to call this method after
 * tio_updater_set_app(tio_updater_t*, const char*, const char*) since the
 * tio_updater_set_app() method has side effect resetting to default memory allocator.
 */
void tio_updater_set_cb_slist_resource(
    tio_updater_t* updater,
    KHC_CB_SLIST_ALLOC cb_alloc,
    KHC_CB_SLIST_FREE cb_free,
    void* cb_alloc_data,
    void* cb_free_data
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
