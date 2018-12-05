/**
 * @file kii.h
 * @brief This is a file defining Kii Cloud APIs.
 */
#ifndef KII_H
#define KII_H

#include "khc.h"
#include "kii_task_callback.h"

#include <jkii.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define M_KII_LOG(...) printf(__VA_ARGS__)
#else
#define M_KII_LOG(...)
#endif

/**
 * \brief MQTT task name.
 * Name can be referenced inside task create callback set by
 * kii_set_cb_task_create().
 */
#define KII_TASK_NAME_MQTT "kii_mqtt_task"

/**
 * \brief Callback writes data.
 *
 * Used to propagate response body.
 * The callback is called multiple times until the whole response body is propagated.
 *
 * Following APIs require this callback implementation since the response body could be large.
 * - kii_download_object_body()
 *
 * If you dealing with file, You may open the file before the API call and
 * close after returned.
 *
 * \param [in] buff Buffer stores data to be written.
 * You need to determine its size by size argument since null termination is not ensured.
 * \param [in] size Size of the data chunk requested to write.
 * \param [in] userdata Context object pointer set by APIs requires this callback.
 * \return Size actually written. If the returned value and size argument value is different,
 * API execution is aborted and API returns KII_ERR_WRITE_CALLBACK.
 */
typedef size_t (*KII_CB_WRITE)(char *buff, size_t size, void *userdata);
/**
 * \brief Callback read data.
 *
 * Used to propagate request body.
 * The callback is called multiple times until the implementation returns 0.
 *
 * Following APIs require this callback implementation since the request body could be large.
 * - kii_upload_object_body()
 * - kii_ti_put_state()
 * - kii_ti_put_bulk_states()
 * - kii_ti_patch_state()
 * - kii_ti_patch_bulk_states()
 *
 * If you dealing with file, You may open the file before the API call and
 * close after returned.
 *
 * \param [out] buff Buffer stores data read.
 * \param [in] size Requested size to read.
 * \param [in] userdata Context object pointer set by APIs requires this callback.
 * \return Size actually read. Returning 0 indicates that whole data is read.
 */
typedef size_t (*KII_CB_READ)(char *buffer, size_t size, void *userdata);
/**
 * \brief Error codes definition.
 */
typedef enum kii_code_t
{
    KII_ERR_OK, /**< \brief Succeeded */
    KII_ERR_SOCK_CONNECT, /**< \brief Socket error in connection. */
    KII_ERR_SOCK_CLOSE, /**< \brief Socket error in closing. */
    KII_ERR_SOCK_SEND, /**< \brief Socket error in sending data. */
    KII_ERR_SOCK_RECV, /**< \brief Socket error in receiving data. */
    KII_ERR_HEADER_CALLBACK, /**< \brief Error in header callback. */
    KII_ERR_WRITE_CALLBACK, /**< \brief Error in write callback. */
    KII_ERR_ALLOCATION, /**< \brief Error in memory allocation. */
    KII_ERR_TOO_LARGE_DATA, /**< \brief Data is larger than expected. */
    KII_ERR_RESP_STATUS, /**< \brief REST API returns error status code. */
    KII_ERR_PARSE_JSON, /**< \brief REST API returns error status code. */
    KII_ERR_FAIL /**< \brief Other errors. */
} kii_code_t;

/** \brief Represents scope of bucket/ topic. */
typedef enum kii_scope_type_t
{
    KII_SCOPE_APP, /**< \brief App scope */
    KII_SCOPE_USER, /**< \brief User scope */
    KII_SCOPE_GROUP, /**< \brief Group scope */
    KII_SCOPE_THING /**< \brief Thing scope */
} kii_scope_type_t;

/** \brief Represents bucket */
typedef struct kii_bucket_t
{
    kii_scope_type_t scope; /**< \brief Scope of the bucket. */
    const char *scope_id; /**< \brief ID of the scope. (eg. User ID, Thing ID, etc.) */
    const char *bucket_name; /**< \brief Name of the bucket. */
} kii_bucket_t;

/** \brief Represents topic */
typedef struct kii_topic_t {
    kii_scope_type_t scope; /**< \brief Scope of the topic. */
    const char* scope_id; /**< \brief ID of the scope. (App/ User/ Group/ Thing ID) */
    const char* topic_name; /**< \brief Name of the topic. */
} kii_topic_t;

/** \brief Represents author of API. */
typedef struct kii_author_t
{
    /** \brief ID of the author. */
    char author_id[128];
    /** \brief Access token of the author. */
    char access_token[128];
} kii_author_t;

/** \brief Represents MQTT endpoint. */
typedef struct kii_mqtt_endpoint_t {
    char username[64]; /** \brief Username used to connect. */
    char password[128]; /** \brief Password used to connect. */
    char topic[64]; /** \brief Topic ID to be subscribed. */
    char host[64]; /** \brief Host address */
    /**
     * \brief Port number used for non-secure connection.
     * Highly recommend use port_ssl.
     */
    unsigned int port_tcp;
    /**
     * \brief Port number used for secure connection.
     */
    unsigned int port_ssl;
    /**
     * \brief Obsoleted parameter. Ignore it.
     */
    unsigned long ttl;
} kii_mqtt_endpoint_t;

/**
 * \brief Represents object ID.
 */
typedef struct kii_object_id_t {
    char id[64]; /**< \brief Object ID */
} kii_object_id_t;

/**
 * \brief Represents installation ID.
 */
typedef struct kii_installation_id_t {
    char id[64]; /**< \brief Installation ID */
} kii_installation_id_t;

struct kii_t;

/**
 * \brief Called when received push message via MQTT.
 *
 * \param [in] message Received message.
 * You need to determine size of the message by message_length
 * argument since it may not be null terminated.
 * \param [in] message_length Length of the message.
 * \param [in] userdata Context object pointer given to kii_start_push_routine().
 */
typedef void (*KII_PUSH_RECEIVED_CB)(
                const char* message,
                size_t message_length,
                void* userdata);

/**
 * \brief Stores data/ callbacks used by kii module.
 */
typedef struct kii_t {
    khc _khc; /**< \private */
	kii_author_t _author; /**< \private */
	char _app_id[128]; /**< \private */
	char _app_host[128]; /**< \private */
    char* _sdk_info; /**< \private */

    void* _mqtt_sock_connect_ctx; /**< \private */
    void* _mqtt_sock_send_ctx; /**< \private */
    void* _mqtt_sock_recv_ctx; /**< \private */
    void* _mqtt_sock_close_ctx; /**< \private */
    KHC_CB_SOCK_CONNECT _cb_mqtt_sock_connect; /**< \private */
    KHC_CB_SOCK_SEND _cb_mqtt_sock_send; /**< \private */
    KHC_CB_SOCK_RECV _cb_mqtt_sock_recv; /**< \private */
    KHC_CB_SOCK_CLOSE _cb_mqtt_sock_close_cb; /**< \private */
    unsigned int _mqtt_to_recv_sec; /**< \private */
    unsigned int _mqtt_to_send_sec; /**< \private */

    KII_CB_TASK_CREATE _cb_task_create; /**< \private */
    void* _task_create_data; /**< \private */

    KII_CB_TASK_CONTINUE _cb_task_continue; /**< \private */
    void* _task_continue_data; /**< \private */

    KII_CB_TASK_EXIT _cb_task_exit; /**< \private */
    void* _task_exit_data; /**< \private */

    KII_CB_DELAY_MS _cb_delay_ms; /**< \private */
    void* _delay_ms_data; /**< \private */

    KII_PUSH_RECEIVED_CB _cb_push_received; /**< \private */
    void* _push_data; /**< \private */

    int _mqtt_connected; /**< \private */

    char* _mqtt_buffer; /**< \private */
    size_t _mqtt_buffer_size; /**< \private */

    unsigned int _keep_alive_interval; /**< \private */

    char* _rw_buff; /**< \private */
    size_t _rw_buff_size; /**< \private */
    size_t _rw_buff_req_size; /**< \private */
    size_t _rw_buff_read; /**< \private */
    size_t _rw_buff_written; /**< \private */

    khc_slist* _req_headers; /**< \private */

    char _etag[64]; /**< \private */

    jkii_resource_t* _json_resource; /**< \private */

    JKII_CB_RESOURCE_ALLOC _cb_json_alloc; /**< \private */
    JKII_CB_RESOURCE_FREE _cb_json_free; /**< \private */

    KHC_CB_SLIST_ALLOC _cb_slist_alloc; /**< \private */
    KHC_CB_SLIST_FREE _cb_slist_free; /**< \private */
    void* _slist_alloc_data; /**< \private */
    void* _slist_free_data; /**< \private */
} kii_t;

/**
 * \brief Initializes kii_t instance.
 * \param [out] kii kii_t instance.
 */
void kii_init(
		kii_t* kii);

/**
 * \brief Set site.
 * \param [out] kii kii_t instance.
 * \param [in] site should be a hostname or alias one of
 * "CN", "CN3", "JP", "US", "SG" or "EU".
 */
void kii_set_site(
		kii_t* kii,
		const char* site);

/**
 * \brief Set Application ID.
 * \param [out] kii kii_t instance.
 * \param [in] app_id Application ID published by Kii.
 */
void kii_set_app_id(
		kii_t* kii,
		const char* app_id);

/**
 * \brief Authorize thing with vendor thing id and password.
 * After the authentication, access token is stored in
 * kii instance and used to authorize other API calls.

 * \param [inout] kii kii_t instance.
 * \param [in] vendor_thing_id Thing identifier given by the vendor.
 * \param [in] password Password of the thing given by the vendor.
 * \return kii_code_t.
 */
kii_code_t kii_auth_thing(
		kii_t* kii,
		const char* vendor_thing_id,
		const char* password);

/**
 * \brief Register a new thing.
 * After the registration, access token is stored in
 * kii instance and used to authorize other API calls.

 * \param [in,out] kii kii_t instance.
 * \param [in] vendor_thing_id Thing identifier given by the vendor.
 * \param [in] thing_type Type of the thing given by the vendor.
 * \param [in] password Password of the thing given by the vendor.
 * \return kii_code_t.
 */
kii_code_t kii_register_thing(
		kii_t* kii,
		const char* vendor_thing_id,
		const char* thing_type,
		const char* password);

/**
 * \brief Post Kii object.

 * \param [in,out] kii kii_t instance.
 * \param [in] bucket Specify the bucket to which the object is stored.
 * \param [in] object_data Key-Value pair of the object in json format.
 * \param [in] object_content_type Content-Type of the object. If null,
 * application/json will be applied.
 * \param [out] object_id Created object ID is written to the buffer
 * when succeeded to create new object.
 * Memory used for the struct and char array insidde
 * must be allocated/ free by the application.
 * \return kii_code_t
 */
kii_code_t kii_post_object(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_data,
		const char* object_content_type,
		kii_object_id_t* out_object_id);

/**
 * \brief Put Kii object.

 * \param [in,out] kii kii_t instance.
 * \param [in] bucket Specify the bucket to which the object is stored.
 * \param [in] object_id Specify the id of the object.
 * \param [in] object_data Key-Value pair of the object in JSON format.
 * \param [in] object_content_type Content-Type of the object. If null,
 * application/json will be applied.
 * \param [in] opt_etag If specified, If-Match header is sent to the endpoint.
 * If NULL, If-Match header is not sent.
 * \return  kii_code_t
 */
kii_code_t kii_put_object(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* object_data,
		const char* object_content_type,
        const char* opt_etag);

/**
 * \brief Patch Kii object.
 * Key-Value pair included in patch_data is updated/ added.
 * Other existing Key-Value pair stored in the object are remain untouched.

 * \param [in,out] kii kii_t instance.
 * \param [in] bucket Specify the bucket to which the object is stored.
 * \param [in] object_id Specify the id of the object.
 * \param [in] patch_data Key-Value pairs of the object in JSON format.
 * \param [in] opt_etag If specified, If-Match header is sent to the endpoint.
 * If NULL, If-Match header is not sent.
 * \return  kii_code_t
 */
kii_code_t kii_patch_object(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* patch_data,
		const char* opt_etag);

/**
 * \brief Delete Kii object.

 * \param [in,out] kii kii_t instance.
 * \param [in] Specify the bucket to which the object is stored.
 * \param [in] object_id Specify the id of the object.
 * \return  kii_code_t
 */
kii_code_t kii_delete_object(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id);

/**
 * \brief Get Kii object.
 *  When succeeded, obtained object data is cached in kii_core_t#response_body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  kii_code_t
 */
kii_code_t kii_get_object(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id);

/** Upload object body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] body_content_type content-type of the body.
 *  \param [in] read_cb callback function called for reading body contents.
 *  \param [in] userdata read_cb context data.
 *  \return kii_code_t
 */
kii_code_t kii_upload_object_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* body_content_type,
        const KII_CB_READ read_cb,
        void* userdata
);

/** Download object body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] write_cb callback function writes body contents.
 *  \param [in] userdata write_cb context data.
 *  \return kii_code_t
 */
kii_code_t kii_download_object_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
        const KII_CB_WRITE write_cb,
        void* userdata);

/** Subscribe to specified bucket.
 *  After succeeded,
 *  Event happened on the bucket will be notified via push notification.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket to subscribe.
 *  \return kii_code_t
 */
kii_code_t kii_subscribe_bucket(
		kii_t* kii,
		const kii_bucket_t* bucket);

/** Unsubscribe specified bucket.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket to subscribe.
 *  \return kii_code_t
 */
kii_code_t kii_unsubscribe_bucket(
		kii_t* kii,
		const kii_bucket_t* bucket);

/** Subscribe to specified topic.
 *  After succeeded,
 *  Message sent to the topic will be notified via push notification.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to subscribe.
 *  \return kii_code_t
 */
kii_code_t kii_subscribe_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Unsubscribe specified topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to subscribe.
 *  \return kii_code_t
 */
kii_code_t kii_unsubscribe_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Put new topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to create.
 *  \return kii_code_t
 */
kii_code_t kii_put_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Delete topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to delete.
 *  \return kii_code_t
 */
kii_code_t kii_delete_topic(
		kii_t* kii,
		const kii_topic_t* topic);

kii_code_t kii_install_push(
        kii_t* kii,
        kii_bool_t development,
        kii_installation_id_t* out_installation_id);

kii_code_t kii_get_mqtt_endpoint(
    kii_t* kii,
    const char* installation_id,
    kii_mqtt_endpoint_t* endpoint);

/** Start push notification receiving routine.
 *  After succeeded, callback is called when push message is delivered to this
 *  thing.
 *  \param [inout] kii sdk instance.
 *  \param [in] MQTT keep alive interval in second. If 0, Keep Alive mechanism is disabled.
 * Otherwise, ping req is sent to MQTT broker periodically with the specified interval.
 * Sending too many request with short interval consumes resouces. We recommend 30 seconds or longer interval.
 *  \param [in] callback  callback function called when push message delivered.
 *  \param [in] userdata context object passed to callback.
 *  \return kii_code_t
 */
kii_code_t kii_start_push_routine(
		kii_t* kii,
        unsigned int keep_alive_interval,
		KII_PUSH_RECEIVED_CB callback,
        void* userdata);

/** Execute server code.
 *  \param [inout] kii sdk instance.
 *  \param [in] endpoint_name name of the endpoint to be executed.
 *  \param [in] params parameters given to endpoint. should be formatted in json.
 *  \return kii_code_t
 */
kii_code_t kii_execute_server_code(
		kii_t* kii,
		const char* endpoint_name,
		const char* params);

kii_code_t kii_ti_onboard(
    kii_t* kii,
    const char* vendor_thing_id,
    const char* password,
    const char* thing_type,
    const char* firmware_version,
    const char* layout_position,
    const char* thing_properties);

kii_code_t kii_ti_put_firmware_version(
    kii_t* kii,
    const char* firmware_version);

typedef struct kii_ti_firmware_version_t {
    char firmware_version[128];
} kii_ti_firmware_version_t;

kii_code_t kii_ti_get_firmware_version(
    kii_t* kii,
    kii_ti_firmware_version_t* version);

kii_code_t kii_ti_put_thing_type(
    kii_t* kii,
    const char* thing_type);

kii_code_t kii_ti_put_state(
    kii_t* kii,
    KII_CB_READ state_read_cb,
    void* state_read_cb_data,
    const char* opt_content_type,
    const char* opt_normalizer_host);

kii_code_t kii_ti_put_bulk_states(
    kii_t* kii,
    KII_CB_READ state_read_cb,
    void* state_read_cb_data,
    const char* opt_content_type,
    const char* opt_normalizer_host);

kii_code_t kii_ti_patch_state(
    kii_t* kii,
    KII_CB_READ state_read_cb,
    void* state_read_cb_data,
    const char* opt_content_type,
    const char* opt_normalizer_host);

kii_code_t kii_ti_patch_bulk_states(
    kii_t* kii,
    KII_CB_READ state_read_cb,
    void* state_read_cb_data,
    const char* opt_content_type,
    const char* opt_normalizer_host);

/** start to create request for REST API.
 *
 * Between this function and kii_api_call_run(kii_t*), you can call
 * kii_api_call_append_body(kii_t*, const char* size_t) and
 * kii_api_call_append_header(kii_t*, const char*, const char*) any
 * number of times.
 *
 * @param [in] kii SDK object.
 * @param [in] http_method method of http request.
 * @param [in] resource_path resource path of http request.
 * @param [in] content_type content type of http_body.
 * @param [in] set_authentication_header a flag to set or not
 * authentication header.
 * @return result of preparation.
 */
kii_code_t kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        kii_bool_t set_authentication_header);

/** append request body.
 *
 * This function must be called between kii_api_call_start(kii_t*,
 * const char*, const char*, const char*, kii_bool_t) and
 * kii_api_call_run(kii_t*).
 *
 * @param [in] kii SDK object.
 * @param [in] chunk part of the body to be appended.
 * @param [in] chunk_size size of the chunk.
 * @return result of addition.
 */
kii_code_t kii_api_call_append_body(
        kii_t* kii,
        const char* chunk,
        size_t chunk_size);

/** append request header.
 *
 * This function must be called between kii_api_call_start(kii_t*,
 * const char*, const char*, const char*, kii_bool_t) and
 * kii_api_call_run(kii_t*).
 *
 * @param [in] kii SDK object.
 * @param [in] key key of http header.
 * @param [in] value value of http header.
 * @return result of addition.
 */
kii_code_t
kii_api_call_append_header(
        kii_t* kii,
        const char* key,
        const char* value);

/** run with created request for REST API.
 *
 * HTTP request is created with following APIs:
 *
 * - kii_api_call_start(kii_t*, const char*,const char*, const char*,
 *   kii_bool_t)
 * - kii_api_call_append_body(kii_t*, const char*, size_t)
 * - kii_api_call_append_header(kii_t*, const char*, const char*)
 *
 * After creation of HTTP request, this function calls REST API with
 * created request.
 * Response status can be obtained by kii_get_resp_status(kii_t*)
 * Response body is written to the buffer set by kii_api_call_run(kii_t*)
 *
 * @param [in] kii SDK object.
 * @return result of the request creation.
 */
kii_code_t kii_api_call_run(kii_t* kii);

/**
 * \brief Set buffer used to construct/ parse HTTP request/ response.

 * This method must be called and set valid buffer before calling method initiate HTTP session
 * such as kii_auth_thing(), kii_post_object(), etc.
 * The buffer is used to serialize/ deserialize JSON.

 * When handling request body which could be large in following APIs,
 * - kii_upload_object_body(),
 * - kii_ti_put_state()
 * The buffer is not used. Stream based KII_CB_READ is used instead.
 * You don't have to take account the buffer size used by those request.

 * Similary when handling response body which could be large in following APIs,
 * kii_download_object_body()
 * The buffer is not used. Stream based KII_CB_WRITE is used instead.
 * You don't have to take account the buffer size used by those response.

 * You can change the size of buffer depending on the request/ response size.
 * It must be enough large to store whole request/ response except for method listed above.
 * Typically, 4096 bytes is enough. However it varies depending on your data schema used to define
 * object or thing. If object becomes large, consider putting them in object body.

 * \param [out] kii instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void kii_set_buff(kii_t* kii, char* buff, size_t buff_size);

/**
 * \brief Set stream buffer.
 * Stream buffer is used store part of HTTP body when
 * reading/ writing it from the network.

 * If this method is not called or set NULL to the buffer,
 * kii allocates memory of stream buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by kii is 1024 bytes.

 * You can change the size of buffer depending on your request/ response size.
 * It must be enough large to store size line in chunked encoded message.
 * However, you may use much larger buffer since size line might require very small buffer
 * as it consists of HEX size and CRLF for the better performance.

 * If you set the buffer by the method, the method must be called before calling method initiate HTTP session
 * such as kii_auth_thing(), kii_post_object(), etc.
 * and memory used by the buffer can be safely freed after the method returned.

 * \param [out] kii instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void kii_set_stream_buff(kii_t* kii, char* buff, size_t buff_size);

/**
 * \brief Set response header buffer.

 * The buffer is used to store single HTTP response header.
 * If this method is not called or set NULL to the buffer,
 * kii allocates memory of response header buffer when the HTTP session started
 * and free when the HTTP session ends.
 * The buffer allocated by kii is 256 bytes.

 * If header is larger than the buffer, the header is skipped and not parsed.
 * kii needs to parse Status Line, Content-Length, Transfer-Encoding and ETag header.
 * The buffer must have enough size to store those headers. 256 bytes would be enough.
 * If you set the buffer by the method, the method must be called before calling method initiate HTTP session
 * such as kii_auth_thing(), kii_post_object(), etc.
 * and memory used by the buffer can be safely freed after the method returned.

 * \param [out] kii instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void kii_set_resp_header_buff(kii_t* kii, char* buff, size_t buff_size);

void kii_set_cb_http_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata);
void kii_set_cb_http_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata);
void kii_set_cb_http_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata);
void kii_set_cb_http_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata);

/**
 * \brief Set buffer used to parse MQTT message.

 * This method must be called and set valid buffer before calling method
 * kii_start_push_routine()
 * The buffer is used to parse MQTT message.

 * You can change the size of buffer depending on the request/ response size.
 * It must be enough large to store whole message send by MQTT.
 * Typically, 1024 bytes is enough.
 * However it varies depending on your data schema used to define Commands.
 * Avoid defining large Commands.

 * \param [out] kii instance.
 * \param [in] buffer pointer to the buffer.
 * \param [in] buff_size size of the buffer.
 */
void kii_set_mqtt_buff(kii_t* kii, char* buff, size_t buff_size);

void kii_set_cb_mqtt_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata);
void kii_set_cb_mqtt_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata);
void kii_set_cb_mqtt_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata);
void kii_set_cb_mqtt_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata);

void kii_set_mqtt_to_sock_recv(kii_t* kii, unsigned int to_sock_recv_sec);
void kii_set_mqtt_to_sock_send(kii_t* kii, unsigned int to_sock_send_sec);

void kii_set_cb_task_create(kii_t* kii, KII_CB_TASK_CREATE create_cb, void* userdata);

/**
 * \brief set callback determines whether to continue or discontinue task.

 * If this method is not called or NULL is set, task exits only when un-recoverble error occurs.
 * If you need cancellation mechanism, you need to set this callback.
 * Terminate task without using this callback may cause memory leak.
 * This method must be called before calling kii_start_push_routine().

 * In case checking cancellation flag in continue_cb, the flag might be set by other task/ thread.
 * Implementation must ensure consistency of the flag by using Mutex, etc.

 * If un-recoverble error occurs, task exits the infinite loop and immediately calls KII_CB_TASK_EXIT callback if set.
 * In this case KII_CB_TASK_CONTINUE callback is not called.

 * \param kii [out] kii instance
 * \param continue_cb [in] Callback determines whether to continue or discontinue task.
 * If continue_cb returns KII_TRUE, task continues. Otherwise the task exits the infinite loop
 * and calls KII_CB_TASK_EXIT callback if set.
 * task_info argument type of the continue_cb function (defined as void* in KII_CB_TASK_EXIT) is kii_mqtt_task_info*.
 * \param userdata [in] Context data pointer passed as second argument when continue_cb is called.
 */
void kii_set_cb_task_continue(kii_t* kii, KII_CB_TASK_CONTINUE continue_cb, void* userdata);

/**
 * \brief Callback called right before exit of MQTT task.

 * Task exits when the task is discontinued by KII_CB_TASK_CONTINUE callback or
 * un-recoverble error occurs.
 * In exit_cb, you'll need to free memory used for MQTT buffer set by kii_set_mqtt_buff(),
 * Memory used for the userdata passed to following callbacks in case not yet freed.

 * - kii_set_cb_mqtt_sock_send()
 * - kii_set_cb_mqtt_sock_connect()
 * - kii_set_cb_mqtt_sock_recv()
 * - kii_set_cb_mqtt_sock_close()
 * - kii_set_cb_task_continue()
 * - kii_set_cb_task_exit()

 * In addition, you may need to call task/ thread termination API.
 * It depends on the task/ threading framework you used to create task/ thread.
 * After the exit_cb returned, task function immediately returns.

 * If this API is not called or set NULL,
 * task function immediately returns when task is discontinued or un-recoverble error occurs.

 * \param kii instance
 * \param exit_cb Called right before the exit.
 * task_info argument type of exit_cb (defined as void* in KII_CB_TASK_EXIT) is kii_mqtt_task_info*.
 * \param userdata [in] Context data pointer passed as second argument when exit_cb is called.
 */
void kii_set_cb_task_exit(kii_t* kii, KII_CB_TASK_EXIT exit_cb, void* userdata);
void kii_set_cb_delay_ms(kii_t* kii, KII_CB_DELAY_MS delay_cb, void* userdata);

/** Set JSON paraser resource
 * @param [inout] kii SDK instance.
 * @param [in] resource to be used parse JSON. 256 tokens_num might be enough for almost all usecases.
 * If you need to parse large object or allocate exact size of memory used,
 * see kii_set_cb_json_parser_resource(kii_t, JKII_CB_RESOURCE_ALLOC, JKII_CB_RESOURCE_FREE)
 */
void kii_set_json_parser_resource(kii_t* kii, jkii_resource_t* resource);

/** Set JSON paraser resource allocators.
 *  To use Allocator instead of fixed size memory given by kii_set_json_parser_resource(kii_t, jkii_resource_t),
 *  call kii_set_json_parser_resource(kii_t, jkii_resource_t) with NULL resource argument.
 * @param [inout] kii SDK instance.
 * @param [in] cb_alloc allocator callback.
 * @param [in] cb_free free callback should free memories allocated in cb_alloc.
 */
void kii_set_cb_json_parser_resource(kii_t* kii,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free);

/**
 * \brief Set khc_slist (linked list) resource allocators.

 * If this method is not called, default allocators implemented with malloc/free is used to
 * allocate linked list used to construct HTTP request headers.
 */
void kii_set_cb_slist_resource(
    kii_t* kii,
    KHC_CB_SLIST_ALLOC cb_alloc,
    KHC_CB_SLIST_FREE cb_free,
    void* cb_alloc_data,
    void* cb_free_data);

const char* kii_get_etag(kii_t* kii);

int kii_get_resp_status(kii_t* kii);

/**
 * \brief Get response body length.
 * If the response is chunked, decoded response body length is returned.
 *
 * \param [in] kii kii_t instance.
 * \return Length of response body.
 */
size_t kii_get_resp_body_length(kii_t* kii);

typedef enum
{
    KII_MQTT_ST_INSTALL_PUSH,
    KII_MQTT_ST_GET_ENDPOINT,
    KII_MQTT_ST_SOCK_CONNECT,
    KII_MQTT_ST_SEND_CONNECT,
    KII_MQTT_ST_RECV_CONNACK,
    KII_MQTT_ST_SEND_SUBSCRIBE,
    KII_MQTT_ST_RECV_SUBACK,
    KII_MQTT_ST_RECV_READY,
    KII_MQTT_ST_RECV_MSG,
    KII_MQTT_ST_SEND_PINGREQ,
    KII_MQTT_ST_RECONNECT,
    KII_MQTT_ST_ERR_EXIT,
    KII_MQTT_ST_DISCONTINUED,
} kii_mqtt_task_state;

typedef enum
{
    KII_MQTT_ERR_OK,
    KII_MQTT_ERR_INSTALLATION,
    KII_MQTT_ERR_GET_ENDPOINT,
    KII_MQTT_ERR_INSUFFICIENT_BUFF
} kii_mqtt_error;

typedef struct {
    kii_mqtt_error error;
    kii_mqtt_task_state task_state;
} kii_mqtt_task_info;

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

