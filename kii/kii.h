/**
 * @file kii.h
 * @brief This is a file defining Kii Cloud APIs.
 */
#ifndef KII_H
#define KII_H

#include "kii_core.h"
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

#define KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS 30

#define KII_TASK_NAME_RECV_MSG "kiiPush_recvMsgTask"
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
#define KII_TASK_NAME_PING_REQ "kiiPush_pingReqTask"
#endif

struct kii_t;

typedef void (*KII_PUSH_RECEIVED_CB)(
                struct kii_t* kii,
                char* message,
                size_t message_length);

typedef struct kii_t {
    kii_core_t kii_core;

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
    kii_socket_context_t mqtt_socket_context;
    KII_SOCKET_CONNECT_CB mqtt_socket_connect_cb;
    KII_SOCKET_SEND_CB mqtt_socket_send_cb;
    KII_SOCKET_RECV_CB mqtt_socket_recv_cb;
    KII_SOCKET_CLOSE_CB mqtt_socket_close_cb;

    KII_TASK_CREATE task_create_cb;

    KII_DELAY_MS delay_ms_cb;

    KII_PUSH_RECEIVED_CB push_received_cb;

    int _mqtt_connected;

    char* mqtt_buffer;
    size_t mqtt_buffer_size;

    void* app_context;

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
 *  \param [in] app_key the input of Application Key
 *  \return  0:success, -1: failure
 */
int kii_init(
		kii_t* kii,
		const char* site,
		const char* app_id,
		const char* app_key);

/** Authorize thing with vendor thing id and password.
 *  After the authentication, access token is used to call APIs access to
 *  Kii Cloud so that the thing can access private data.
 *  \param [inout] kii sdk instance.
 *  \param [in] vendor_thing_id the thing identifier given by vendor.
 *  \param [in] password the password of the thing given by vendor.
 *  \return 0:success, -1: failure
 */
int kii_thing_authenticate(
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
 *  \return 0:success, -1: failure
 */
int kii_thing_register(
		kii_t* kii,
		const char* vendor_thing_id,
		const char* thing_type,
		const char* password);

/** Create new object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_data key-value pair of the object in json format.
 *  \param [in] object_content_type content-type of the object. If null,
 *  application/json will be applied.
 *  \param [out] object_id output of the created object ID.
 *  Supplied when succeeded to create new object.
 *  Must be allocated by application with the size of KII_OBJECTID_SIZE + 1
 *  before this api call.
 *  \return 0:success, -1: failure
 */
int kii_object_create(
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
 *  \return  0:success, -1: failure
 */
int kii_object_create_with_id(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* object_data,
		const char* object_content_type);

/** Partial update of the object
 *  Only the specified key-value is updated and other key-values won't be
 *  updated/ removed.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] patch_data key-value pair of the object in json format.
 *  \param [in] opt_etag etag of the object. if specified, update will be failed
 *  if there is updates on cloud. if NULL, forcibly updates.
 *  \return  0:success, -1: failure
 */
int kii_object_patch(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* patch_data,
		const char* opt_etag);

/** Full update of the object
 * Replace the object with specified key-values.
 * Existing key-value pair which is not included in the replacement_data will be
 * removed.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] replacement_data key-value pair of the object in json format.
 *  \param [in] opt_etag etag of the object. if specified, update will be failed
 *  if there is updates on cloud. if NULL, forcibly updates.
 *  \return  0:success, -1: failure
 */
int kii_object_replace(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* replacement_data,
		const char* opt_etag);

/** Delete the object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  0:success, -1: failure
 */
int kii_object_delete(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id);

/** Get the object
 *  When succeeded, obtained object data is cached in kii_core_t#response_body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  0:success, -1: failure
 */
int kii_object_get(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id);

/** Upload object body at once.
 *  Upload object body at one time.
 *  If the data is large,
 *  consider use chunk upload with kii_object_upload_body().
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] body_content_type content-type of the body.
 *  \param [in] data object body data.
 *  \param [in] data_length length of the data.
 *  \return 0:success, -1: failure
 */
int kii_object_upload_body_at_once(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* body_content_type,
		const void* data,
		size_t data_length);

/** Initiate chunked object body upload.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [out] out_upload_id upload id to be used to continue chunked upload.
 *  Must be allocated by application with the size of KII_UPLOADID_SIZE + 1
 *  before this api call.
 *  \return 0:success; -1: failure
 */
int kii_object_init_upload_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		char* out_upload_id);

/** represents chunk data */
typedef struct kii_chunk_data_t {
	/** content-type of the body */
	char* body_content_type;
	/** position of the chunk.(bytes count) */
	size_t position;
	/** length of the chunk */
	size_t length;
	/** total length of the body */
	size_t total_length;
	/** chunk data */
	void* chunk;
} kii_chunk_data_t;

/** Upload object body chunk by chunk.
 *  You need to obtain upload id before execute this method by calling
 *  kii_object_init_upload_body().
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] upload id obtained by kii_object_init_upload_body().
 *  \param [in] chunk chunk of the body to upload.
 *  \return 0:success, -1: failure
 */
int kii_object_upload_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* upload_id,
		const kii_chunk_data_t* chunk);

/** Commit and finalize upload.
 *  After successfully upload all the chunk of the body by
 *  kii_object_upload_body() call this api to finalize upload.
 *  or in cancel upload can be called after the kii_object_init_upload_body()
 *  succeeded. Basically no need to cancel upload since Kii Cloud cleanups the
 *  impcompleted uploads periodically.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] upload id obtained by kii_object_init_upload_body().
 *  \param ]in] commit 0: cancell upload, >0: commit upload
 *  \return 0:success, -1: failure
 */
int kii_object_commit_upload(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* upload_id,
		unsigned int commit);

/** Download object body at one time.
 *  If the data size is large or unknown, consider use kii_object_download_body()
 *  instead.
 *  The result is cached in kii_core_t#response_body when succeeded.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [out] out_data_length length of the downloaded body. (in bytes)
 *  \return 0:success, -1: failure
 */
int kii_object_download_body_at_once(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		unsigned int* out_data_length);

/** Download object body chunk by chunk.
 *  Downloaded data is cached in kii_core_t#response_body after the download chunk is
 *  succeeded.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object of which body is added.
 *  \param [in] position starting position of the download (in bytes)
 *  \param [in] length length to download (in bytes)
 *  \param [out] out_actual_length actual length downloaded. (in bytes)
 *  \param [out] out_total_length total length of whole object body (in bytes)
 *  \return 0:success, -1: failure
 */
int kii_object_download_body(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		unsigned int position,
		unsigned int length,
		unsigned int* out_actual_length,
		unsigned int* out_total_length);

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

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

