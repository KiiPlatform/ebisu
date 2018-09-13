/**
 * @file kii.h
 * @brief This is a file defining Kii Cloud APIs.
 */
#ifndef KII_H
#define KII_H

#include "khc.h"
#include "kii_task_callback.h"

#include <kii_json.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KII_OBJECTID_SIZE 36

#define KII_UPLOADID_SIZE 64

#ifdef DEBUG
#ifndef __FILE__
#define __FILE__ ("__FILE__ macro is not available")
#endif

#ifndef __LINE__
#define __LINE__ (-1)
#endif

#define M_KII_LOG(x) \
	if (kii->kii_core.logger_cb != NULL) {\
		kii->kii_core.logger_cb("file:%s, line:%d ", __FILE__, __LINE__); \
		(x); \
	}
#else
#define M_KII_LOG(x)
#endif

#define KII_TASK_NAME_RECV_MSG "recv_msg_task"
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
#define KII_TASK_NAME_PING_REQ "ping_req_task"
#endif

typedef size_t (*KII_CB_WRITE)(char *ptr, size_t size, size_t count, void *userdata);
typedef size_t (*KII_CB_READ)(char *buffer, size_t size, size_t count, void *userdata);

typedef enum kii_code_t
{
    KII_ERR_OK,
    KII_ERR_SOCK_CONNECT,
    KII_ERR_SOCK_CLOSE,
    KII_ERR_SOCK_SEND,
    KII_ERR_SOCK_RECV,
    KII_ERR_HEADER_CALLBACK,
    KII_ERR_WRITE_CALLBACK,
    KII_ERR_ALLOCATION,
    KII_ERR_TOO_LARGE_DATA,
    KII_ERR_RESP_STATUS,
    KII_ERR_PARSE_JSON,
    KII_ERR_FAIL
} kii_code_t;

/** bool type definition */
typedef enum kii_bool_t
{
    KII_FALSE = 0,
    KII_TRUE
} kii_bool_t;

/** represents scope of bucket/ topic. */
typedef enum kii_scope_type_t
{
    KII_SCOPE_APP,
    KII_SCOPE_USER,
    KII_SCOPE_GROUP,
    KII_SCOPE_THING
} kii_scope_type_t;

/** represents bucket */
typedef struct kii_bucket_t
{
    kii_scope_type_t scope;
    const char *scope_id;
    const char *bucket_name;
} kii_bucket_t;

/** represents topic */
typedef struct kii_topic_t {
    kii_scope_type_t scope;
    char* scope_id;
    char* topic_name;
} kii_topic_t;

/** represents author of SDK api. */
typedef struct kii_author_t
{
    /** ID of the author */
    char author_id[128];
    /** access token of the author */
    char access_token[128];
} kii_author_t;

struct kii_t;

typedef void (*KII_PUSH_RECEIVED_CB)(
                struct kii_t* kii,
                char* message,
                size_t message_length);

typedef struct kii_t {
    khc _khc;
	kii_author_t _author;
	char _app_id[128];
	char _app_host[128];
    char* _sdk_info;
    /** Socket context for MQTT client.
     *
     * If you want to use MQTT client, You must set this socket
     * context.  If You want to use ping req, you need to define
     * KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
     * macro. KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS is interval of
     * sending ping request. KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
     * requires seconds of ping request interval. If the interval is
     * too short, MQTT client often disconnect connection to a MQTT
     * server. We recommend 30 seconds or upper.
     */
    void* mqtt_sock_connect_ctx;
    void* mqtt_sock_send_ctx;
    void* mqtt_sock_recv_ctx;
    void* mqtt_sock_close_ctx;
    KHC_CB_SOCK_CONNECT mqtt_sock_connect_cb;
    KHC_CB_SOCK_SEND mqtt_sock_send_cb;
    KHC_CB_SOCK_RECV mqtt_sock_recv_cb;
    KHC_CB_SOCK_CLOSE mqtt_sock_close_cb;

    KII_TASK_CREATE task_create_cb;

    KII_DELAY_MS delay_ms_cb;

    KII_PUSH_RECEIVED_CB push_received_cb;

    int _mqtt_connected;

    char* mqtt_buffer;
    size_t mqtt_buffer_size;

    char* _rw_buff;
    size_t _rw_buff_size;
    size_t _rw_buff_req_size;
    size_t _rw_buff_read;
    size_t _rw_buff_written;

    khc_slist* _req_headers;

    char _etag[64];
    /** Resource used by KII JSON library.
     *
     * This field is optional. If KII_JSON_FIXED_TOKEN_NUM macro is
     * defined, KII JSON library takes resources by myself on stack
     * memory. In KII_JSON_FIXED_TOKEN_NUM case, token size of Kii
     * JSON library is number defined by KII_JSON_FIXED_TOKEN_NUM. If
     * your environment has small stack size, you should use this
     * field and manage this resources by yourself.
     *
     * This field is not managed by this SDK. If
     * kii_json_resource_t#tokens is allocated from heap memory, you
     * must free these by yourself before applications are
     * terminated.
     *
     * Value of this field is not thread safe. You must not share
     * kii_json_resource_t#tokens instance with two or more kii_t
     * instance.
     */
    kii_json_resource_t kii_json_resource;

    /** Callback to resize to kii_json_resource contents.
     *
     * This field is optional. If this field is NULL. This SDK does
     * not try to allocate kii_t#kii_json_resource. As a result, Some
     * APIs may fail with KII_JSON_PARSE_SHORTAGE_TOKENS.
     */
    KII_JSON_RESOURCE_CB kii_json_resource_cb;

} kii_t;

/** Initializes Kii SDK
 *  \param [inout] kii sdk instance.
 *  \param [in] site the input of site name,
 *  should be one of "CN", "CN3", "JP", "US", "SG" or "EU"
 *  \param [in] app_id the input of Application ID
 *  \return  0:success, -1: failure
 */
int kii_init(
		kii_t* kii,
		const char* site,
		const char* app_id);

/** Authorize thing with vendor thing id and password.
 *  After the authentication, access token is used to call APIs access to
 *  Kii Cloud so that the thing can access private data.
 *  \param [inout] kii sdk instance.
 *  \param [in] vendor_thing_id the thing identifier given by vendor.
 *  \param [in] password the password of the thing given by vendor.
 *  \return 0:success, -1: failure
 */
kii_code_t kii_thing_auth(
		kii_t* kii,
		const char* vendor_thing_id,
		const char* password);

/** Register new thing
 *  After the registtration, access token is used to call APIs access to
 *  Kii Cloud so that the thing can access private data.
 *  \param [inout] kii sdk instance.
 *  \param [in] vendor_thing_id the thing identifier given by vendor.
 *  \param [in] thing_type the type of the thing given by vendor.
 *  \param [in] password - the password of the thing given by vendor.
 *  \return kii_code_t.
 */
kii_code_t kii_thing_register(
		kii_t* kii,
		const char* vendor_thing_id,
		const char* thing_type,
		const char* password);

/** Post Kii object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_data key-value pair of the object in json format.
 *  \param [in] object_content_type content-type of the object. If null,
 *  application/json will be applied.
 *  \param [out] object_id output of the created object ID.
 *  Supplied when succeeded to create new object.
 *  Must be allocated by application with the size of KII_OBJECTID_SIZE + 1
 *  before this api call.
 *  \return kii_code_t
 */
kii_code_t kii_object_post(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_data,
		const char* object_content_type,
		char* out_object_id);

/** Create new object with the specified ID
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] object_data key-value pair of the object in json format.
 *  \param [in] object_content_type content-type of the object. If null,
 *  application/json will be applied.
 *  \param [in] if specified, If-Match header is sent to the endpoint.
 *  \return  kii_code_t
 */
kii_code_t kii_object_put(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* object_data,
		const char* object_content_type,
        const char* opt_etag);

/** Partial update of the object
 *  Only the specified key-value is updated and other key-values won't be
 *  updated/ removed.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] patch_data key-value pair of the object in json format.
 *  \param [in] opt_etag etag of the object. if specified, update will be failed
 *  if there is updates on cloud. if NULL, forcibly updates.
 *  \return  kii_code_t
 */
kii_code_t kii_object_patch(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* patch_data,
		const char* opt_etag);

/** Delete the object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  kii_code_t
 */
kii_code_t kii_object_delete(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id);

/** Get the object
 *  When succeeded, obtained object data is cached in kii_core_t#response_body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  kii_code_t
 */
kii_code_t kii_object_get(
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
kii_code_t kii_object_upload_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* body_content_type,
        size_t body_content_length,
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
kii_code_t kii_object_download_body(
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
 *  \return 0:success, -1: failure
 */
int kii_push_subscribe_bucket(
		kii_t* kii,
		const kii_bucket_t* bucket);

/** Unsubscribe specified bucket.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket to subscribe.
 *  \return 0:success, -1: failure
 */
int kii_push_unsubscribe_bucket(
		kii_t* kii,
		const kii_bucket_t* bucket);

/** Subscribe to specified topic.
 *  After succeeded,
 *  Message sent to the topic will be notified via push notification.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to subscribe.
 *  \return 0:success, -1: failure
 */
int kii_push_subscribe_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Unsubscribe specified topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to subscribe.
 *  \return 0:success, -1: failure
 */
int kii_push_unsubscribe_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Create new topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to create.
 *  \return 0:success, -1: failure
 */
int kii_push_create_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Delete existing topic.
 *  \param [inout] kii sdk instance.
 *  \param [in] topic specify the topic to delete.
 *  \return 0:success, -1: failure
 */
int kii_push_delete_topic(
		kii_t* kii,
		const kii_topic_t* topic);

/** Start push notification receiving routine.
 *  After succeeded, callback is called when push message is delivered to this
 *  thing.
 *  \param [inout] kii sdk instance.
 *  \param [in] callback  callback function called when push message delivered. 
 *  \return 0:success, -1: failure
 */
int kii_push_start_routine(
		kii_t* kii,
		KII_PUSH_RECEIVED_CB callback);

/** Execute server code.
 *  \param [inout] kii sdk instance.
 *  \param [in] endpoint_name name of the endpoint to be executed.
 *  \param [in] params parameters given to endpoint. should be formatted in json.
 *  \return 0:success, -1: failure
 */
int kii_server_code_execute(
		kii_t* kii,
		const char* endpoint_name,
		const char* params);

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
 * @return result of preparation. if 0, preparation is succeeded,
 * otherwise failed
 */
int kii_api_call_start(
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
 * @param [in] body_data appended body data.
 * @param [in] body_size appended body data size.
 * @return result of appending. if 0 appending is succeeded, otherwise
 * failed.
 */
int kii_api_call_append_body(
        kii_t* kii,
        const void* body_data,
        size_t body_size);

/** append request header.
 *
 * This function must be called between kii_api_call_start(kii_t*,
 * const char*, const char*, const char*, kii_bool_t) and
 * kii_api_call_run(kii_t*).
 *
 * @param [in] kii SDK object.
 * @param [in] key key of http header.
 * @param [in] value value of http header.
 * @return result of appending. if 0 appending is succeeded, otherwise
 * failed.
 */
int
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
 * created request. As a result, HTTP response is set to
 * kii_t#kii_core#response_body and kii_t#kii_core#response_code.
 *
 * @param [in] kii SDK object.
 * @return result of closing request creation if 0 it is succeeded,
 * otherwise failed.
 */
int kii_api_call_run(kii_t* kii);

int kii_api_call(
    kii_t *kii,
    const char *http_method,
    const char *resource_path,
    const void *http_body,
    size_t body_size,
    const char *content_type,
    char *header,
    ...);

int kii_set_buff(kii_t* kii, char* buff, size_t buff_size);

int kii_set_http_cb_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata);
int kii_set_http_cb_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata);
int kii_set_http_cb_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata);
int kii_set_http_cb_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata);

int kii_set_mqtt_cb_sock_connect(kii_t* kii, KHC_CB_SOCK_CONNECT cb, void* userdata);
int kii_set_mqtt_cb_sock_send(kii_t* kii, KHC_CB_SOCK_SEND cb, void* userdata);
int kii_set_mqtt_cb_sock_recv(kii_t* kii, KHC_CB_SOCK_RECV cb, void* userdata);
int kii_set_mqtt_cb_sock_close(kii_t* kii, KHC_CB_SOCK_CLOSE cb, void* userdata);

char* kii_get_etag(kii_t* kii);

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

