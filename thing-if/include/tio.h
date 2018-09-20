#ifndef _KII_THING_IF_
#define _KII_THING_IF_

#include <kii.h>
#include <khc.h>

#define EMESSAGE_SIZE 50

#ifdef __cplusplus
extern "C" {
#endif

#define KII_THING_IF_TASK_NAME_STATUS_UPDATE "status_update_task"

/** States of tio_t instance. */
typedef enum tio_state_t {
    /** A tio_t instance is initialized. */
    KII_THING_IF_STATE_INITIALIZED,
    /** A tio_t instance is onboarded. */
    KII_THING_IF_STATE_ONBOARDED,
    /** A tio_t instance is started. */
    KII_THING_IF_STATE_STARTED
} tio_state_t;

/** Error reasons of thing-if ThingSDK. */
typedef enum tio_error_code_t {
    /** State of tio_t instance is invalid to use some functions. */
    KII_THING_IF_ERROR_INVALID_STATE,

    /** HTTP error. */
    KII_THING_IF_ERROR_HTTP,

    /** Socket error. */
    KII_THING_IF_ERROR_SOCKET,

    /** HTTP request/ response buffer is insufficient.
     *
     * You need to increase size of HTTP request/ response buffer of
     * tio_t::command_handler and/or
     * tio_t::state_updater.
     */
    KII_THING_IF_ERROR_INSUFFICIENT_BUFFER,

    /** Size of argument buffer is insufficient.
     *
     * Some functions such as ::get_thing_type receives a pointer of
     * an array.
     * This error is raised if the length of the given array is shorter than required.
     *
     * Application should increase the length of the array.
     **/
    KII_THING_IF_ERROR_INSUFFICIENT_ARG_BUFFER,

    /** Fail to parse HTTP response.
     *
     * Received body payload in the response is unexpected form.
     * You might not see this error
     * since the error is arranged for contingencies such as
     * received broken data from the undelying network.
     * If this error constantly happens you may need to ask for support.
     */
    KII_THING_IF_ERROR_INVALID_PAYLOAD
} tio_error_code_t;

/** Error information of thing-if ThingSDK. */
typedef struct tio_error_t {
    /** Error code. */
    tio_error_code_t code;

    /** HTTP status code.
     *
     * If ::tio_error_t::code is
     * ::KII_THING_IF_ERROR_HTTP, this value is set. Otherwise 0.
     */
    int http_status_code;

    /** Error code.
     *
     * If ::tio_error_t::code is
     * ::KII_THING_IF_ERROR_HTTP, this value is set. Otherwise
     * functions does not change this value.
     */
    char error_code[64];
} tio_error_t;

/** callback function for handling action.
 * @param [in] alias name of alias.
 * @param [in] action_name name of the action.
 * @param [in] action_params json object represents parameter of this action.
 * @param [out] error error message if operation is failed.(optional)
 * @return KII_TRUE if succeeded, otherwise KII_FALSE.
 */
typedef kii_bool_t
    (*KII_THING_IF_ACTION_HANDLER)
        (const char* alias,
         const char* action_name,
         const char* action_params,
         char error[EMESSAGE_SIZE + 1]);

/** a function pointer to write thing state.
 *
 * This function pointer is used at KII_THING_IF_STATE_HANDLER. This
 * function pointer is passed as second argument of
 * KII_THING_IF_STATE_HANDLER. Implementation of this function pointer is
 * provided by this SDK.
 *
 * @param [in] context context of state handler.
 * @param [in] buff json string of thing state. must be null terminated.
 * @return KII_TRUE if succeeded. otherwise KII_FALSE.
 */
typedef kii_bool_t (*KII_THING_IF_WRITER)(kii_t* kii, const char* buff);

/** callback function for writing thing state.
 *
 * This callback function should write current thing state with
 * KII_THING_IF_WRITER. for example:
 *
 * @code
 * kii_bool_t state_handler(
 *         kii_t* kii,
 *         KII_THING_IF_WRITER writer)
 * {
 *     char buf[256];
 *
 *     if ((*writer)(kii, "{\"power\":") == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     if (get_power() != 0) {
 *         if ((*writer)(kii, "true,") == KII_FALSE) {
 *             return KII_FALSE;
 *         }
 *     } else {
 *         if ((*writer)(kii, "false,") == KII_FALSE) {
 *             return KII_FALSE;
 *         }
 *     }
 *     if ((*writer)(kii, "\"brightness\":") == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     sprintf(buf, "%d,", get_brightness());
 *     if ((*writer)(kii, buf) == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     if ((*writer)(kii, "\"color\":[") == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     sprintf(buf, "%d,", get_color(0));
 *     if ((*writer)(kii, buf) == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     sprintf(buf, "%d,", get_color(1));
 *     if ((*writer)(kii, buf) == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     sprintf(buf, "%d],", get_color(2));
 *     if ((*writer)(kii, buf) == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     if ((*writer)(kii, "\"colorTemperature\":") == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     sprintf(buf, "%d}", get_colorTemperature());
 *     if ((*writer)(kii, buf) == KII_FALSE) {
 *         return KII_FALSE;
 *     }
 *     return KII_TRUE;
 * }
 * @endcode
 *
 * @param [in] kii state_updater object.
 * @param [in] writer writer to write thing state. implementation of
 * this writer is provided by this SDK.
 * @return KII_TRUE if succeeded. otherwise KII_FALSE.
 */
typedef kii_bool_t
    (*KII_THING_IF_STATE_HANDLER)
        (kii_t* kii,
         KII_THING_IF_WRITER writer);

/** callback function enables to handle all push notifications.
 *
 * This handler is optional and only used for advanced use-cases.
 * Push notification will be sent to the thing in following cases.
 * - Subscribed buckets events
 * - Messages sent to the subscribed topics
 * - Commands sent to this thing.
 *
 * Normally you don't need to implement this handler.
 * Without this handler, SDK only deals with push notification includes
 * Command. In this case, SDK parses the push notification and if its a 
 * valid Command to the thing, #KII_THING_IF_ACTION_HANDLER will be called.
 *
 * This handler will be implemented and passed to #tio_t
 * in case you need to handle bucket events notification, messages arrived to
 * subscribed topic or need to implement custom procedure when received a
 * Commands sent to this thing.
 *
 * You can choose by retruning KII_TRUE or KII_FALSE whether to let SDK to
 * continue handling with default logic which deals with Command push
 * notification.
 *
 * @param [in] kii_t kii_t object if you want to send request to kii
 * cloud you can use this kii_t object.
 * @param [in] message notification message
 * @param [in] message_length length of message.
 * @return
 * - KII_TRUE Let SDK to skip executing default logic handles Command.
 *   In this case #KII_THING_IF_ACTION_HANDLER won't be called even if the
 *   push notification includes valid Command.
 * - KII_FALSE Let SDK to execute default logic handles Command. If the push
 *   notification includes valid Command, #KII_THING_IF_ACTION_HANDLER will be
 *   called.
 *   (If the push notification is not Command, would be ignored safely.) 
 */
typedef kii_bool_t
    (*KII_THING_IF_CUSTOM_PUSH_HANDLER)
        (kii_t *kii,
         const char* message,
         size_t message_length);

/**
 * Resource for command handler.
 *
 * Invocation of #action_handler, #state_handler and
 * #custom_push_handler callback inside this struct is serialized
 * since they are called from the single task/thread.
 *
 * However, tio_state_updater_resource_t#state_handler and callbacks
 * inside this struct is invoked from different task/ thread.
 * That means the invocation could be concurrent.
 *
 * If you share the resources(memory, file, etc.)
 * among the callbacks called concurrently,
 * be aware for it and avoid deadlock when you implement synchronization for
 * the resources.
 */
typedef struct tio_command_handler_resource_t {
    /** HTTP request and response buffer for command handler. */
    char* buffer;

    /** HTTP request and response buffer size for command handler. */
    size_t buffer_size;

    /** buffer for MQTT. */
    char* mqtt_buffer;

    /** buffer size for MQTT. */
    size_t mqtt_buffer_size;

    /** callback function to handle received action. */
    KII_THING_IF_ACTION_HANDLER action_handler;

    /** callback function to write the thing state after a command is
     * processed.
     */
    KII_THING_IF_STATE_HANDLER state_handler;

    /** callback function to handle recived all push notifications.
     * Normally you can left this field as NULL.
     * Only required when you have to deal with push notification with your
     * custom logic.
     * @see KII_THING_IF_CUSTOM_PUSH_HANDLER
     */
    KII_THING_IF_CUSTOM_PUSH_HANDLER custom_push_handler;

} tio_command_handler_resource_t;

/**
 * Resource for state updater.
 * Invocation of #state_handler callback inside this struct is serialized since
 * it called from single task/thraed.
 *
 * However, #state_handler and callbacks inside
 * tio_command_handler_resource_t
 * would be invoked from different task/thread.
 * That means the invocation could be concurrent.
 *
 * If you share the resources(memory, file, etc.)
 * among the callbacks called concurrently,
 * be aware for it and avoid deadlock when you implement synchronization for
 * the resources.
 */
typedef struct tio_state_updater_resource_t {
    /** HTTP request and response buffer for state updater. */
    char* buffer;

    /** HTTP request and response buffer size for state updater. */
    size_t buffer_size;

    /** the period of updating state in seconds. */
    int period;

    /** callback function to write thing state.
     * called in every #period.
     */
    KII_THING_IF_STATE_HANDLER state_handler;
} tio_state_updater_resource_t;

/**
 * System callback function pointers.
 */
typedef struct tio_system_cb_t {
    KII_TASK_CREATE       task_create_cb;
    KII_DELAY_MS          delay_ms_cb;
    KHC_CB_SOCK_CONNECT socket_connect_cb;
    KHC_CB_SOCK_SEND    socket_send_cb;
    KHC_CB_SOCK_RECV    socket_recv_cb;
    KHC_CB_SOCK_CLOSE   socket_close_cb;
    KHC_CB_SOCK_CONNECT mqtt_socket_connect_cb;
    KHC_CB_SOCK_SEND    mqtt_socket_send_cb;
    KHC_CB_SOCK_RECV    mqtt_socket_recv_cb;
    KHC_CB_SOCK_CLOSE   mqtt_socket_close_cb;
} tio_system_cb_t;

typedef struct tio_t {
    kii_t command_handler;
    kii_t state_updater;
    KII_THING_IF_ACTION_HANDLER action_handler;
    KII_THING_IF_STATE_HANDLER state_handler_for_period;
    KII_THING_IF_STATE_HANDLER state_handler_for_command_reaction;
    KII_THING_IF_CUSTOM_PUSH_HANDLER custom_push_handler;
    /** Specify the period of updating state in seconds. */
    int state_update_period;
    /**
     * Represent state of tio_t instance. Application must
     * not change this value.
     */
    tio_state_t state;
} tio_t;

/** Initialize tio_t object.
 *
 * After this method is called, applications must call
 * onboard_with_vendor_thing_id(tio_t*, const char*, const char*,
 * const char*, const char*) or onboard_with_thing_id(tio_t*,
 * const char*, const char*) to onboard from thing.
 *
 * After this functions succeeded, tio_t::state becomes
 * ::KII_THING_IF_STATE_INITIALIZED.
 *
 * @param [in] tio tio_t object to be initialized.
 * @param [in] app_id the input of Application ID
 * @param [in] app_key the input of Application Key
 * @param [in] app_host host name. should be one of "CN", "CN3", "JP", "US",
 * "SG" or "EU".
 * @param [in] command_handler_data data container for command handler.
 * @param [in] state_updater_data data container for state updater.
 * @param [in] resource_cb callback to resize to kii_json_resource
 * contents. This is optional. If you build this SDK with
 * KII_JSON_FIXED_TOKEN_NUM macro, you can set NULL to this
 * argument. otherwise, you need to set kii_json_resource_t object to
 * this argument.
 *
 * @return KII_TRUE when succeeded, KII_FALSE when failed.
 */
kii_bool_t init_tio(
        tio_t* tio,
        const char* app_id,
        const char* app_key,
        const char* app_host,
        tio_command_handler_resource_t* command_handler_resource,
        tio_state_updater_resource_t* state_updater_resource,
        tio_system_cb_t* system_cb);

/** Start tio_t instance.
 *
 * thing-if ThingSDK starts to receive command and update states with
 * this function.
 *
 * This function must be called after one of following functions
 * - ::onboard_with_vendor_thing_id,
 * - ::onboard_with_thing_id
 * - ::init_tio_with_onboarded_thing
 *
 * After this functions succeeded, tio_t::state becomes
 * ::KII_THING_IF_STATE_STARTED.
 *
 * @param [in] tio_t This SDK instance.
 * @return KII_TRUE when succeeded, KII_FALSE when failed.
 */
kii_bool_t start(tio_t* tio);

/** Onboard to Thing_If Cloud with specified vendor thing ID.
 * tio_t#command_handler and tio_t#state_updater instances are
 * used to call api.
 *
 * After this functions succeeded, tio_t::state becomes
 * ::KII_THING_IF_STATE_ONBOARDED.
 *
 * @param [inout] tio This SDK instance.
 * @param [in] vendor_thing_id Vendor thing id given by thing
 * vendor. Must not be NULL and empty string.
 * @param [in] password Password of the thing given by thing
 * vendor. Must not be NULL and empty string.
 * @param [in] thing_type Type of the thing. If the thing is already
 * registered, this value would be ignored by Kii Cloud. If this value
 * is NULL or empty string, this value is ignored.
 * @param [in] firmware_version Firmware version of the thing. If the
 * thing is already registered, this value would be ignored by Kii
 * Cloud. If this value is NULL or empty string this value is ignored.
 * @param [in] layout_position Layout position of the thing. Should be
 * one of "STANDALONE", "GATEWAY" or "ENDNODE". If the thing is
 * already registered, this value would be ignored by Kii Cloud. If
 * this value is NULL or empty string and the thing is not registered
 * yet, then "STANDALONE" is used as default.
 * @param [in] thing_properties Other properties of the thing. please
 * refer to
 * http://docs.kii.com/rest/#thing_management-register_a_thing about
 * the format. If the thing is already registered, this value would be
 * ignored by Kii Cloud. If this value is NULL or empty string this
 * value is ignored.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error. This function does not set
 * ::KII_THING_IF_ERROR_INVALID_STATE to
 * ::tio_error_t::code.
 */
kii_bool_t onboard_with_vendor_thing_id(
        tio_t* tio,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties,
        tio_error_t* error);

/** Onboard to Thing_If Cloud with specified thing ID.
 * tio_t#command_handler and tio_t#state_updater instances are
 * used to call api.
 *
 * After this functions succeeded, tio_t::state becomes
 * ::KII_THING_IF_STATE_ONBOARDED.
 *
 * @param [inout] tio This SDK instance.
 * @param [in] thing_id thing ID issued by Kii Cloud. Must not be NULL
 * and empty string.
 * @param [in] password Password of the thing given by thing
 * vendor. Must not be NULL and empty string.
 * @param [in] thing_type Type of the thing. If the thing is already
 * registered, this value would be ignored by Kii Cloud. If this value
 * is NULL or empty string, this value is ignored.
 * @param [in] firmware_version Firmware version of the thing. If the
 * thing is already registered, this value would be ignored by Kii
 * Cloud. If this value is NULL or empty string this value is ignored.
 * @param [in] layout_position Layout position of the thing. Should be
 * one of "STANDALONE", "GATEWAY" or "ENDNODE". If the thing is
 * already registered, this value would be ignored by Kii Cloud. If
 * this value is NULL or empty string and the thing is not registered
 * yet, then "STANDALONE" is used as default.
 * @param [in] thing_properties Other properties of the thing. please
 * refer to
 * http://docs.kii.com/rest/#thing_management-register_a_thing about
 * the format. If the thing is already registered, this value would be
 * ignored by Kii Cloud. If this value is NULL or empty string this
 * value is ignored.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error. This function does not set
 * ::KII_THING_IF_ERROR_INVALID_STATE to
 * ::tio_error_t::code.
 */
kii_bool_t onboard_with_thing_id(
        tio_t* tio,
        const char* thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties,
        tio_error_t* error);


/** Initialize tio_t object with onboarded thing information.
 *
 * This api is used when onboard process has been done by controller
 * application (typically a mobile apps.) and thing ID and access
 * token is given by the controller application (via BLE, etc.) Since
 * in this case onboard process is already completed, no need to call
 * onboard_with_vendor_thing_id() or onboard_with_thing_id().
 *
 * tio_t#command_handler and tio_t#state_updater instances are
 * used to call api.
 *
 * After this functions succeeded, tio_t::state becomes
 * ::KII_THING_IF_STATE_ONBOARDED.
 *
 * @param [in] app_id the input of Application ID
 * @param [in] app_key the input of Application Key
 * @param [in] app_host host name. should be one of "CN", "CN3", "JP", "US",
 * "SG" or "EU".
 * @param [in] thing_id thing id given by a controller application
 * NonNull, NonEmpty value must be specified.
 * @param [in] access_token access token of the thing given by a
 * controller application. NonNull, NonEmpty value must be specified.
 * @param [in] command_handler_data data container for command handler.
 * @param [in] state_updater_data data container for state updater.
 * @param [in] resource_cb callback to resize to kii_json_resource
 * contents. This is optional. If you build this SDK with
 * KII_JSON_FIXED_TOKEN_NUM macro, you can set NULL to this
 * argument. otherwise, you need to set kii_json_resource_t object to
 * this argument.
 *
 * @return KII_TRUE when succeeded, KII_FALSE when failed.
 */
kii_bool_t init_tio_with_onboarded_thing(
        tio_t* tio,
        const char* app_id,
        const char* app_key,
        const char* app_host,
        const char* thing_id,
        const char* access_token,
        tio_command_handler_resource_t* command_handler_resource,
        tio_state_updater_resource_t* state_updater_resource,
        tio_system_cb_t* system_cb);

/** Upate firmware version of a thing.
 *
 * This function must be called between ::start and one of
 * following functions:
 * - ::onboard_with_vendor_thing_id,
 * - ::onboard_with_thing_id
 * - ::init_tio_with_onboarded_thing
 *
 * @param [in] tio_t This SDK instance.
 * @param [in] firmware_version firmware version to update.
 * @param [out] error Error infomation. This is optional. If NULL,
 * error information is not set.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error.
 */
kii_bool_t update_firmware_version(
        tio_t* tio,
        const char* firmware_version,
        tio_error_t* error);

/** Get firmware version of a thing.
 *
 * This function must be called between ::start and one of
 * following functions:
 * - ::onboard_with_vendor_thing_id,
 * - ::onboard_with_thing_id
 * - ::init_tio_with_onboarded_thing
 *
 * @param [in] tio_t This SDK instance.
 * @param [out] firmware_version a buffer to copy firmware version got
 * from Kii Cloud. This SDK makes the buffer null terminated string.
 * @param [in] firmware_version_len length of firmware_version which
 * is second argument of this function.
 * @param [out] error Error infomation. This is optional. If NULL,
 * error information is not set.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error.
 */
kii_bool_t get_firmware_version(
        tio_t* tio,
        char* firmware_version,
        size_t firmware_version_len,
        tio_error_t* error);

/** Upate thing type of a thing.
 *
 * This function must be called between ::start and one of
 * following functions:
 * - ::onboard_with_vendor_thing_id,
 * - ::onboard_with_thing_id
 * - ::init_tio_with_onboarded_thing
 *
 * @param [in] tio_t This SDK instance.
 * @param [in] thing_type thing type to update.
 * @param [out] error Error infomation. This is optional. If NULL,
 * error information is not set.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error.
 */
kii_bool_t update_thing_type(
        tio_t* tio,
        const char* thing_type,
        tio_error_t* error);

/** Get current thing type of a thing.
 *
 * This function must be called between ::start and one of
 * following functions:
 * - ::onboard_with_vendor_thing_id,
 * - ::onboard_with_thing_id
 * - ::init_tio_with_onboarded_thing
 *
 * @param [in] tio_t This SDK instance.
 * @param [out] thing_type a buffer to copy thing type got
 * from Kii Cloud. This SDK makes the buffer null terminated string.
 * @param [in] thing_type_len length of thing_type which
 * is second argument of this function.
 * @param [out] error Error infomation. This is optional. If NULL,
 * error information is not set.
 * @return KII_TRUE when succeeded, KII_FALSE when failed. If returned
 * value is KII_FALSE and error is not NULL, this SDK set error
 * information to the error.
 */
kii_bool_t get_thing_type(
        tio_t* tio,
        char* thing_type,
        size_t thing_type_len,
        tio_error_t* error);

#ifdef __cplusplus
}
#endif

#endif /* _KII_THING_IF_ */

