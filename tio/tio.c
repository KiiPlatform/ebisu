#include "tio.h"

#include <kii.h>
#include <kii_json.h>

#include <stdio.h>
#include <string.h>


/* If your environment does not have assert, you must set
   TIO_NOASSERT define. */
#ifdef TIO_NOASSERT
  #define M_TIO_ASSERT(s)
#else
  #include <assert.h>
  #define M_TIO_ASSERT(s) assert(s)
#endif

#define EVAL(f, v) f(v)
#define TOSTR(s) #s
#define ULONG_MAX_STR EVAL(TOSTR, ULONG_MAX)
#define ULONGBUFSIZE (sizeof(ULONG_MAX_STR) / sizeof(char))

#define CONST_STRLEN(str) sizeof(str) - 1

#define APPEND_BODY_CONST(kii, str) kii_api_call_append_body(kii, str, CONST_STRLEN(str))
#define APPEND_BODY(kii, str) kii_api_call_append_body(kii, str, strlen(str))

#define APP_PATH "api/apps/"
#define OAUTH_PATH "/oauth2/token"
#define THING_IF_APP_PATH "thing-if/apps/"
#define ONBOARDING_PATH "/onboardings"
#define TARGET_PART "/targets/thing:"
#define COMMAND_PART "/commands/"
#define RESULTS_PART "/action-results"
#define STATES_PART "/states"
#define THINGS_PART "/things/"
#define FIRMWARE_VERSION_PART "/firmware-version"
#define THING_TYPE_PART "/thing-type"
#define CONTENT_TYPE_VENDOR_THING_ID "application/vnd.kii.OnboardingWithVendorThingIDByThing+json"
#define CONTENT_TYPE_THING_ID "application/vnd.kii.OnboardingWithThingIDByThing+json"
#define CONTENT_UPDATE_FIRMWARE_VERSION "application/vnd.kii.ThingFirmwareVersionUpdateRequest+json"
#define CONTENT_UPDATE_THING_TYPE "application/vnd.kii.ThingTypeUpdateRequest+json"
#define CONTENT_UPDATE_STATE "application/vnd.kii.MultipleTraitState+json"


#define THING_IF_INFO "sn=tic;sv=1.0.1"

typedef enum _get_key_and_value_t {
    PRV_GET_KEY_AND_VALUE_SUCCESS,
    PRV_GET_KEY_AND_VALUE_FAIL,
    PRV_GET_KEY_AND_VALUE_FINISH
} _get_key_and_value_t;

static int _kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        kii_bool_t set_authentication_header,
        tio_error_t* error)
{
    int retval = kii_api_call_start(
            kii,
            http_method,
            resource_path,
            content_type,
            set_authentication_header);
    if (retval != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to start api call"));
        if (error != NULL){
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
    }
    return retval;
}

static int _append_key_value(
        kii_t* kii,
        const char* key,
        const char* value,
        kii_bool_t is_successor,
        kii_bool_t is_string)
{
    if (key == NULL) {
        M_KII_LOG(kii->kii_core.logger_cb("key not specified.\n"));
        return -1;
    }
    if (value == NULL) {
        M_KII_LOG(
            kii->kii_core.logger_cb("value not specified for key: %s.\n", key));
        return -1;
    }

    if (is_successor == KII_TRUE) {
        if (kii_api_call_append_body(kii, ",", CONST_STRLEN(",")) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb(
                "request size overflowed: (%s, %s).\n", key, value));
            return -1;
        }
    }
    /* Write key. */
    if (kii_api_call_append_body(kii, "\"", CONST_STRLEN("\"")) != 0 ||
            kii_api_call_append_body(kii, key, strlen(key)) != 0 ||
            kii_api_call_append_body(kii, "\":", CONST_STRLEN("\":")) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb(
            "request size overflowed: (%s, %s).\n", key, value));
        return -1;
    }

    /* Write value. */
    if (is_string == KII_TRUE) {
        if (kii_api_call_append_body(kii, "\"", CONST_STRLEN("\"")) != 0 ||
                kii_api_call_append_body(kii, value, strlen(value)) != 0 ||
                kii_api_call_append_body(kii, "\"", CONST_STRLEN("\"")) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb(
                    "request size overflowed: (%s, %s).\n", key, value));
            return -1;
        }
    } else {
        if (kii_api_call_append_body(kii, value, strlen(value)) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb(
                    "request size overflowed: (%s, %s).\n", key, value));
            return -1;
        }
    }
    return 0;
}

static int _append_key_value_string(
        kii_t* kii,
        const char* key,
        const char* value,
        kii_bool_t is_successor)
{
    return _append_key_value(kii, key, value, is_successor, KII_TRUE);
}

static int _append_key_value_string_optional(
        kii_t* kii,
        const char* key,
        const char* value,
        kii_bool_t is_successor)
{
    if (key == NULL || value == NULL) {
        return 0;
    }
    return _append_key_value(kii, key, value, is_successor, KII_TRUE);
}

static int _append_key_value_object_optional(
        kii_t* kii,
        const char* key,
        const char* value,
        kii_bool_t is_successor)
{
    if (key == NULL || value == NULL) {
        return 0;
    }
    return _append_key_value(kii, key, value, is_successor, KII_FALSE);
}

static int _append_key_value_bool(
        kii_t* kii,
        const char* key,
        kii_bool_t value,
        kii_bool_t is_successor)
{
    return _append_key_value(
            kii,
            key,
            value == KII_TRUE ? "true" : "false",
            is_successor,
            KII_FALSE);
}

static kii_json_parse_result_t _tio_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields)
{
    kii_json_parse_result_t retval;
    // FIXME: Enable to configure resource or resource allocator
    kii_json_token_t tokens[256];
    kii_json_resource_t resource = {tokens, 256};

    retval = kii_json_parse(json_string, json_string_size, fields, &resource);

    return retval;
}

static kii_bool_t _execute_http_session(
        kii_t* kii,
        tio_error_t* error)
{
    M_TIO_ASSERT(kii != NULL);

    kii_code_t res = kii_api_call_run(kii);
    // FIXME renew thing-if error handling system.
    switch (res) {
        case KII_ERR_OK:
            break;
        case KII_ERR_TOO_LARGE_DATA:
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
            return KII_FALSE;
        default:
            return KII_FALSE;
    }

    int resp_status = kii_get_resp_status(kii);
    /* check http status */
    if (resp_status < 200 ||
            resp_status >= 300) {
        if (error != NULL) {
            kii_json_field_t fields[2];
            memset(fields, 0x00, sizeof(fields));
            fields[0].path = "/errorCode";
            fields[0].type = KII_JSON_FIELD_TYPE_STRING;
            fields[0].field_copy.string = error->error_code;
            fields[0].field_copy_buff_size = sizeof(error->error_code) /
                sizeof(error->error_code[0]);
            fields[1].path = NULL;
            _tio_json_read_object(
                    kii,
                    kii->_rw_buff,
                    kii->_rw_buff_written,
                    fields);
            error->code = TIO_ERROR_HTTP;
            error->http_status_code = resp_status;
        }
        return KII_FALSE;
    }
    return KII_TRUE;
}

static kii_bool_t _set_onboard_resource_path(
        const char* app_id,
        char* resource_path,
        size_t resource_path_len)
{
    size_t onboard_resource_path_length =
      strlen(THING_IF_APP_PATH) +
      strlen(app_id) +
      strlen(ONBOARDING_PATH);

    if (resource_path_len <= onboard_resource_path_length) {
        M_TIO_ASSERT(0);
        return KII_FALSE;
    }

    sprintf(resource_path, "%s%s%s",
            THING_IF_APP_PATH,
            app_id,
            ONBOARDING_PATH);
    return KII_TRUE;
}

static kii_bool_t _set_state_resource_path(
        const char* app_id,
        const char* author_id,
        char* resource_path,
        size_t resource_path_len)
{
    size_t state_resource_path_length =
      strlen(THING_IF_APP_PATH) +
      strlen(app_id) +
      strlen(TARGET_PART) +
      strlen(author_id) +
      strlen(STATES_PART);

    if (resource_path_len <= state_resource_path_length) {
        M_TIO_ASSERT(0);
        return KII_FALSE;
    }

    sprintf(resource_path, "%s%s%s%s%s",
            THING_IF_APP_PATH,
            app_id,
            TARGET_PART,
            author_id,
            STATES_PART);
    return KII_TRUE;
}

static kii_bool_t _set_firmware_version_resource_path(
        const char* app_id,
        const char* author_id,
        char* resource_path,
        size_t resource_path_len)
{
    size_t firmware_version_resource_path_length =
      strlen(THING_IF_APP_PATH) +
      strlen(app_id) +
      strlen(THINGS_PART) +
      strlen(author_id) +
      strlen(FIRMWARE_VERSION_PART);

    if (resource_path_len <= firmware_version_resource_path_length) {
        M_TIO_ASSERT(0);
        return KII_FALSE;
    }
    sprintf(resource_path, "%s%s%s%s%s",
            THING_IF_APP_PATH,
            app_id,
            THINGS_PART,
            author_id,
            FIRMWARE_VERSION_PART);
    return KII_TRUE;
}

static kii_bool_t _set_thing_type_resource_path(
        const char* app_id,
        const char* author_id,
        char* resource_path,
        size_t resource_path_len)
{
    size_t thing_type_path_length = strlen(THING_IF_APP_PATH) +
      strlen(app_id) +
      strlen(THINGS_PART) +
      strlen(author_id) +
      strlen(THING_TYPE_PART);

    if (resource_path_len <= thing_type_path_length) {
        M_TIO_ASSERT(0);
        return KII_FALSE;
    }
    sprintf(resource_path, "%s%s%s%s%s",
            THING_IF_APP_PATH,
            app_id,
            THINGS_PART,
            author_id,
            THING_TYPE_PART);
  return KII_TRUE;
}

static int _thing_if_parse_onboarding_response(
        kii_t* kii,
        tio_error_t* error)
{
    kii_json_field_t fields[3];

    M_TIO_ASSERT(kii != NULL);

    memset(fields, 0, sizeof(fields));
    fields[0].name = "thingID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.author_id;
    fields[0].field_copy_buff_size = sizeof(kii->_author.author_id) /
            sizeof(kii->_author.author_id[0]);
    fields[1].name = "accessToken";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.access_token;
    fields[1].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[2].name = NULL;

    if (_tio_json_read_object(kii, kii->_rw_buff,
                    kii->_rw_buff_written, fields) !=
            KII_JSON_PARSE_SUCCESS) {
        if (error != NULL) {
            error->code = TIO_ERROR_INVALID_PAYLOAD;
        }
        return -1;
    }

    return 0;
}

static kii_bool_t _init_tio(
        tio_t* tio,
        const char* app_id,
        const char* app_key,
        const char* app_host,
        tio_command_handler_resource_t* command_handler_resource,
        tio_state_updater_resource_t* state_updater_resource,
        tio_system_cb_t* system_cb
        )
{
    M_TIO_ASSERT(tio != NULL);
    M_TIO_ASSERT(app_id != NULL);
    M_TIO_ASSERT(app_key != NULL);
    M_TIO_ASSERT(app_host != NULL);
    M_TIO_ASSERT(command_handler_resource != NULL);
    M_TIO_ASSERT(state_updater_resource != NULL);

    memset(tio, 0x00, sizeof(tio_t));
    memset(command_handler_resource->mqtt_buffer, 0x00,
            command_handler_resource->mqtt_buffer_size);
    memset(command_handler_resource->buffer, 0x00,
            command_handler_resource->buffer_size);
    memset(state_updater_resource->buffer, 0x00,
            state_updater_resource->buffer_size);

    /* Initialize command_handler */
    kii_init(&tio->command_handler, app_host, app_id);

    kii_set_buff(
        &tio->command_handler,
        command_handler_resource->buffer,
        command_handler_resource->buffer_size);

    tio->command_handler.mqtt_buffer =
        command_handler_resource->mqtt_buffer;
    tio->command_handler.mqtt_buffer_size =
        command_handler_resource->mqtt_buffer_size;

    tio->action_handler = command_handler_resource->action_handler;
    tio->state_handler_for_command_reaction=
        command_handler_resource->state_handler;
    tio->custom_push_handler =
        command_handler_resource->custom_push_handler;

    /* Initialize state_updater */
    kii_init(&tio->state_updater, app_host, app_id);

    kii_set_buff(
        &tio->state_updater,
        state_updater_resource->buffer,
        state_updater_resource->buffer_size);

    tio->state_handler_for_period = state_updater_resource->state_handler;
    tio->state_update_period = state_updater_resource->period;

    /* setup command handler callbacks. */
    // FIXEME: Pass context object.
    kii_set_http_cb_sock_connect(&tio->command_handler, system_cb->socket_connect_cb, NULL);
    kii_set_http_cb_sock_send(&tio->command_handler, system_cb->socket_send_cb, NULL);
    kii_set_http_cb_sock_recv(&tio->command_handler, system_cb->socket_recv_cb, NULL);
    kii_set_http_cb_sock_close(&tio->command_handler, system_cb->socket_close_cb, NULL);

    tio->command_handler.mqtt_sock_connect_cb = system_cb->mqtt_socket_connect_cb;
    tio->command_handler.mqtt_sock_send_cb = system_cb->mqtt_socket_send_cb;
    tio->command_handler.mqtt_sock_recv_cb = system_cb->mqtt_socket_recv_cb;
    tio->command_handler.mqtt_sock_close_cb = system_cb->mqtt_socket_close_cb;
    tio->command_handler.task_create_cb = system_cb->task_create_cb;
    tio->command_handler.delay_ms_cb = system_cb->delay_ms_cb;

    /* setup state updater callbacks. */
    kii_set_http_cb_sock_connect(&tio->state_updater, system_cb->socket_connect_cb, NULL);
    kii_set_http_cb_sock_send(&tio->state_updater, system_cb->socket_send_cb, NULL);
    kii_set_http_cb_sock_recv(&tio->state_updater, system_cb->socket_recv_cb, NULL);
    kii_set_http_cb_sock_close(&tio->state_updater, system_cb->socket_close_cb, NULL);

    tio->state_updater.task_create_cb = system_cb->task_create_cb;
    tio->state_updater.delay_ms_cb = system_cb->delay_ms_cb;

    tio->state = TIO_STATE_INITIALIZED;

    return KII_TRUE;
}

kii_bool_t init_tio(
        tio_t* tio,
        const char* app_id,
        const char* app_key,
        const char* app_host,
        tio_command_handler_resource_t* command_handler_resource,
        tio_state_updater_resource_t* state_updater_resource,
        tio_system_cb_t* system_cb)
{
    return _init_tio(tio, app_id, app_key, app_host,
            command_handler_resource, state_updater_resource, system_cb);
}

static int _tio_get_key_and_value_from_json(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        char** out_key,
        char** out_value,
        size_t* out_key_len,
        size_t* out_value_len)
{
    jsmn_parser parser;
    int parse_result = JSMN_ERROR_NOMEM;
    size_t tokens_num = 256;
    kii_json_token_t tokens[tokens_num];

    jsmn_init(&parser);

    parse_result = jsmn_parse(&parser, json_string, json_string_len, tokens,
            tokens_num);
    if (parse_result >= 0) {
        if (tokens[0].type != JSMN_OBJECT) {
            return -1;
        }
        if (tokens[1].type != JSMN_STRING) {
            return -1;
        }
        *out_key = (char*)(json_string + tokens[1].start);
        *out_key_len = tokens[1].end - tokens[1].start;
        *out_value = (char*)(json_string + tokens[2].start);
        *out_value_len = tokens[2].end - tokens[2].start;
        return 0;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        return -1;
    } else if (parse_result == JSMN_ERROR_INVAL) {
        return -1;
    } else if (parse_result == JSMN_ERROR_PART) {
        return -1;
    } else {
        return -1;
    }
}

static kii_bool_t _writer(kii_t* kii, const char* buff)
{
    if (kii_api_call_append_body(kii, buff, strlen(buff)) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("request size overflowed.\n"));
        return KII_FALSE;
    }
    return KII_TRUE;
}

static kii_bool_t _send_state(tio_t* ctx)
{
    kii_t* kii = &ctx->state_updater;
    char resource_path[256];
    kii_bool_t succeeded = _set_state_resource_path(
            kii->_app_id,
            kii->_author.author_id,
            resource_path,
            sizeof(resource_path) / sizeof(resource_path[0]));

    if (succeeded == KII_FALSE) {
        M_KII_LOG(kii->kii_core.logger_cb(
                "resource path is longer than expected.\n"));
        return KII_FALSE;
    }

    if (kii_api_call_start(kii, "PUT", resource_path, CONTENT_UPDATE_STATE,
                    KII_TRUE) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to start api call.\n"));
        return KII_FALSE;
    }
    if (ctx->state_handler_for_command_reaction(kii,
                    _writer) == KII_FALSE) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to start api call.\n"));
        return KII_FALSE;
    }
    if (kii_api_call_run(kii) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
        return KII_FALSE;
    }

    return KII_TRUE;
}

static _get_key_and_value_t get_key_and_value_at_index(
        kii_t* kii,
        const char* array_str,
        size_t array_len,
        size_t index,
        char** key,
        size_t* key_len,
        char** value,
        size_t* value_len)
{
    kii_json_field_t item[2];
    char index_str[ULONGBUFSIZE];
    memset(item, 0x00, sizeof(item));
    item[0].path = index_str;
    item[0].type = KII_JSON_FIELD_TYPE_OBJECT;
    item[0].field_copy.string = NULL;
    item[0].result = KII_JSON_FIELD_PARSE_SUCCESS;
    item[1].path = NULL;
    sprintf(index_str, "/[%lu]", (unsigned long)index);

    switch (_tio_json_read_object(
                kii,
                array_str,
                array_len,
                item)) {
        case KII_JSON_PARSE_SUCCESS:
            if (_tio_get_key_and_value_from_json(
                        kii,
                        array_str + item[0].start,
                        item[0].end - item[0].start,
                        key,
                        value,
                        key_len,
                        value_len) != 0) {
                M_KII_LOG(kii->kii_core.logger_cb("fail to parse item."))
                return PRV_GET_KEY_AND_VALUE_FAIL;
            }
            return PRV_GET_KEY_AND_VALUE_SUCCESS;
        case KII_JSON_PARSE_PARTIAL_SUCCESS:
            /* This must be end of array. */
            return PRV_GET_KEY_AND_VALUE_FINISH;
        case KII_JSON_PARSE_ROOT_TYPE_ERROR:
        case KII_JSON_PARSE_INVALID_INPUT:
        default:
            M_KII_LOG(kii->kii_core.logger_cb("unexpected error.\n"));
            return PRV_GET_KEY_AND_VALUE_FAIL;
    }
}

static void handle_command(tio_t* ctx, char* buffer, size_t buffer_size)
{
    kii_t* kii = &ctx->command_handler;
    char* alias_actions_str = NULL;
    size_t alias_actions_len = 0;
    size_t alias_index = 0;
    _get_key_and_value_t alias_result;

    /*
      1. Get start position of alias action array
      2. Start to make request to update command result with kii_api_call_start.
    */
    {
        kii_json_field_t fields[3];
        char resource_path[256];
        size_t results_path_length;
        memset(fields, 0x00, sizeof(fields));
        fields[0].path = "/commandID";
        fields[0].type = KII_JSON_FIELD_TYPE_STRING;
        fields[0].field_copy.string = NULL;
        fields[1].path = "/actions";
        fields[1].type = KII_JSON_FIELD_TYPE_ARRAY;
        fields[1].field_copy.string = NULL;
        fields[2].path = NULL;

        switch(_tio_json_read_object(
                kii, buffer, buffer_size, fields)) {
            case KII_JSON_PARSE_SUCCESS:
                break;
            case KII_JSON_PARSE_PARTIAL_SUCCESS:
                if (fields[0].result != KII_JSON_FIELD_PARSE_SUCCESS) {
                    /* no command ID. */
                    return;
                }
                break;
            default:
                M_KII_LOG(kii->kii_core.logger_cb(
                        "fail to parse received message.\n"));
                return;
        }

        results_path_length =
            strlen(THING_IF_APP_PATH) +
            strlen(kii->_app_id) +
            strlen(TARGET_PART) +
            strlen(kii->_author.author_id) +
            strlen(COMMAND_PART) +
            (fields[0].end - fields[0].start - 1) +
            strlen(RESULTS_PART);

        if (sizeof(resource_path) / sizeof(resource_path[0]) <=
                results_path_length) {
            M_KII_LOG(kii->kii_core.logger_cb(
                    "resource path is longer than expected.\n"));
            return;
        }

        resource_path[0] = '\0';
        strcat(resource_path, THING_IF_APP_PATH);
        strcat(resource_path, kii->_app_id);
        strcat(resource_path, TARGET_PART);
        strcat(resource_path, kii->_author.author_id);
        strcat(resource_path, COMMAND_PART);
        strncat(resource_path, buffer + fields[0].start,
                fields[0].end - fields[0].start);
        strcat(resource_path, RESULTS_PART);
        /* TODO: Check properties. */

        if (kii_api_call_start(kii, "PUT", resource_path, "application/json",
                        KII_TRUE) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb("fail to start api call.\n"));
            return;
        }

        alias_actions_str = buffer + fields[1].start;
        alias_actions_len = fields[1].end - fields[1].start;
    }

    if (kii_api_call_append_body(kii, "{\"actionResults\":[",
                    CONST_STRLEN("{\"actionResults\":[")) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("request size overflowed.\n"));
        return;
    }

    // FIXME: Review error handling system and removed M_KII_LOG logging.
    for (alias_index = 0, alias_result = PRV_GET_KEY_AND_VALUE_SUCCESS;
            alias_result == PRV_GET_KEY_AND_VALUE_SUCCESS;
            ++alias_index) {
        TIO_ACTION_HANDLER handler =
            ctx->action_handler;
        char* alias_name;
        char* actions;
        size_t alias_name_len, actions_len;
        alias_result = get_key_and_value_at_index(
                kii,
                alias_actions_str,
                alias_actions_len,
                alias_index,
                &alias_name,
                &alias_name_len,
                &actions,
                &actions_len);
        switch (alias_result) {
            case PRV_GET_KEY_AND_VALUE_FAIL:
                M_KII_LOG(kii->kii_core.logger_cb("fail to get alias.\n"));
                return;
            case PRV_GET_KEY_AND_VALUE_SUCCESS:
            {
                char alias_name_swap;
                size_t action_index;
                _get_key_and_value_t action_result;

                alias_name_swap = alias_name[alias_name_len];
                alias_name[alias_name_len] = '\0';

                for (action_index = 0,
                            action_result = PRV_GET_KEY_AND_VALUE_SUCCESS;
                        action_result == PRV_GET_KEY_AND_VALUE_SUCCESS;
                        ++action_index) {
                    char* name;
                    char* value;
                    size_t name_len, value_len;
                    action_result = get_key_and_value_at_index(
                            kii,
                            actions,
                            actions_len,
                            action_index,
                            &name,
                            &name_len,
                            &value,
                            &value_len);
                    switch (action_result) {
                        case PRV_GET_KEY_AND_VALUE_FAIL:
                            M_KII_LOG(kii->kii_core.logger_cb(
                                    "fail to get action.\n"));
                            return;
                        case PRV_GET_KEY_AND_VALUE_SUCCESS:
                        {
                            char name_swap, value_swap;
                            char error[EMESSAGE_SIZE + 1];
                            kii_bool_t succeeded;
                            name_swap = name[name_len];
                            value_swap = value[value_len];
                            name[name_len] = '\0';
                            value[value_len] = '\0';

                            succeeded =
                              (*handler)(alias_name, name, value, error);

                            if (alias_index > 0 || action_index > 0) {
                                if (APPEND_BODY_CONST(kii, ",") != 0) {
                                    M_KII_LOG(kii->kii_core.logger_cb(
                                            "request size overflowed.\n"));
                                }
                            }
                            if (APPEND_BODY_CONST(kii, "{\"") != 0 ||
                                    APPEND_BODY(kii, name) != 0 ||
                                    APPEND_BODY_CONST(kii, "\":{") != 0 ||
                                    _append_key_value_bool(
                                        kii,
                                        "succeeded",
                                        succeeded,
                                        KII_FALSE) != 0 ||
                                    _append_key_value_string_optional(
                                        kii,
                                        "errorMessage",
                                        succeeded == KII_FALSE ? error : NULL,
                                        KII_TRUE) != 0 ||
                                    APPEND_BODY_CONST(kii, "}}") != 0) {
                                M_KII_LOG(kii->kii_core.logger_cb(
                                        "request size overflowed.\n"));
                                return;
                            }
                            name[name_len] = name_swap;
                            value[value_len] = value_swap;
                            break;
                        }
                        case PRV_GET_KEY_AND_VALUE_FINISH:
                            /* finished to parse aliases. */
                            break;
                        default:
                            M_KII_LOG(kii->kii_core.logger_cb(
                                    "unknown result %d.\n", action_result));
                            M_TIO_ASSERT(0);
                            return;
                    }
                }
                alias_name[alias_name_len] = alias_name_swap;
                break;
            }
            case PRV_GET_KEY_AND_VALUE_FINISH:
                /* finished to parse aliases. */
                break;
            default:
                M_KII_LOG(
                    kii->kii_core.logger_cb("unknown result %d.\n",
                            alias_result));
                M_TIO_ASSERT(0);
                return;
        }
    }

    if (kii_api_call_append_body(kii, "]}", sizeof("]}") - 1) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("request size overflowed.\n"));
        return;
    }
    if (kii_api_call_run(kii) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
        return;
    }

    if (_send_state(ctx) != KII_TRUE) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to send state.\n"));
    }

    return;
}

static void received_callback(char* buffer, size_t buffer_size, void* userdata) {
    kii_bool_t skip = KII_FALSE;
    tio_t* ctx = (tio_t*)userdata;
    if (ctx->custom_push_handler != NULL) {
        TIO_CUSTOM_PUSH_HANDLER handler =
            ctx->custom_push_handler;
        skip = (*handler)(&ctx->command_handler, buffer, buffer_size);
    }
    if (skip == KII_FALSE) {
        handle_command(ctx, buffer, buffer_size);
    }
}

static int _tio_get_anonymous_token(
        kii_t* kii,
        tio_error_t* error)
{
    char resource_path[64];
    kii_json_field_t fields[2];
    size_t oauth_path_length =
        strlen(APP_PATH) +
        strlen(kii->_app_id) +
        strlen(OAUTH_PATH);

    M_TIO_ASSERT(kii);

    if (sizeof(resource_path) / sizeof(resource_path[0]) <= oauth_path_length) {
        if (error != NULL) {
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
        return -1;
    }
    // FIXME: use snprintf and handle the case buffer size is insufficient.
    sprintf(resource_path, "%s%s%s",
            APP_PATH, kii->_app_id, OAUTH_PATH);

    if (kii_api_call_start(kii, "POST", resource_path, "application/json",
                    KII_FALSE) != 0) {
        if (error != NULL) {
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
        return -1;
    }

    if (APPEND_BODY_CONST(kii, "{") != 0 ||
            _append_key_value_string(
                kii, "grant_type", "client_credentials", KII_FALSE) != 0 ||
            _append_key_value_string(
                kii, "client_id", kii->_app_id, KII_TRUE) != 0 ||
            _append_key_value_string(
                kii, "client_secret", "dummy", KII_TRUE) != 0 ||
            APPEND_BODY_CONST(kii, "}") != 0) {
        if (error != NULL) {
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
        return -1;
    }

    if (_execute_http_session(kii, error) != KII_TRUE) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
        return -1;
    }

    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/access_token";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[1].path = NULL;

    if (_tio_json_read_object(kii, kii->_rw_buff, kii->_rw_buff_written, fields)
            != KII_JSON_PARSE_SUCCESS) {
        if (error != NULL) {
            error->code = TIO_ERROR_INVALID_PAYLOAD;
        }
        return -1;
    }

    return 0;
}

static kii_bool_t _onboard_with_vendor_thing_id(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* thing_properties,
        const char* firmware_version,
        const char* layout_position,
        tio_error_t* error)
{
    char resource_path[64];
    kii_bool_t succeeded;

    if (_tio_get_anonymous_token(kii, error) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get anonymous token.\n"));
        return KII_FALSE;
    }

    succeeded = _set_onboard_resource_path(
            kii->_app_id,
            resource_path,
            sizeof(resource_path) / sizeof(resource_path[0]));
    if (succeeded == KII_FALSE) {
        // FIXME remove redundant logging.
        M_KII_LOG(kii->kii_core.logger_cb(
                "resource path is longer than expected.\n"));
        return KII_FALSE;
    }

    if (kii_api_call_start(kii, "POST", resource_path,
                    CONTENT_TYPE_VENDOR_THING_ID, KII_TRUE) != 0) {
        // FIXME remove redundant logging.
        M_KII_LOG(kii->kii_core.logger_cb("fail to start api call.\n"));
        if (error != NULL) {
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
        return KII_FALSE;
    }

    /* Create JSON string. */
    if (APPEND_BODY_CONST(kii, "{") != 0 ||
            _append_key_value_string(
                kii, "vendorThingID", vendor_thing_id, KII_FALSE) != 0 ||
            _append_key_value_string(
                kii, "thingPassword", password, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "thingType", thing_type, KII_TRUE) != 0 ||
            _append_key_value_object_optional(
                kii, "thingProperties", thing_properties, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "firmwareVersion", firmware_version, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "layoutPosition", layout_position, KII_TRUE) != 0 ||
            APPEND_BODY_CONST(kii, "}") != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("request size overflowed.\n"));
        if (error != NULL) {
            error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
        }
        return KII_FALSE;
    }

    if (_execute_http_session(kii, error) != KII_TRUE) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
        return KII_FALSE;
    }

    if (_thing_if_parse_onboarding_response(kii, error) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to parse resonse.\n"));
        return KII_FALSE;
    }

    return KII_TRUE;
}

static void* _update_status(void *sdata)
{
    tio_t* ctx = (tio_t*)sdata;
    kii_t* kii = &ctx->state_updater;
    char resource_path[256];
    kii_bool_t succeeded = _set_state_resource_path(
            kii->_app_id,
            kii->_author.author_id,
            resource_path,
            sizeof(resource_path) / sizeof(resource_path[0]));

    if (succeeded == KII_FALSE) {
        M_KII_LOG(kii->kii_core.logger_cb(
                "resource path is longer than expected.\n"));
        return NULL;
    }

    while(1) {
        kii->delay_ms_cb(
            (ctx->state_update_period * 1000));

        if (kii_api_call_start(kii, "PUT", resource_path, CONTENT_UPDATE_STATE,
                        KII_TRUE) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb(
                    "fail to start api call.\n"));
            continue;
        }
        if (ctx->state_handler_for_period(kii,
                        _writer) == KII_FALSE) {
            M_KII_LOG(kii->kii_core.logger_cb(
                    "fail to start api call.\n"));
            continue;
        }
        if (kii_api_call_run(kii) != 0) {
            M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
            continue;
        }
    }
    return NULL;
}

static kii_bool_t _set_author(
        kii_author_t* author,
        const char* thing_id,
        const char* access_token)
{
    if (sizeof(author->author_id) / sizeof(author->author_id[0]) <=
            strlen(thing_id)) {
        return KII_FALSE;
    }
    if (sizeof(author->access_token) / sizeof(author->access_token[0]) <=
            strlen(access_token)) {
        return KII_FALSE;
    }

    strncpy(author->author_id, thing_id,
            sizeof(author->author_id) / sizeof(author->author_id[0]));
    strncpy(author->access_token, access_token,
            sizeof(author->access_token) / sizeof(author->access_token[0]));
    return KII_TRUE;
}

kii_bool_t onboard_with_vendor_thing_id(
        tio_t* tio,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties,
        tio_error_t* error)
{
    if (tio->state == TIO_STATE_STARTED) {
        if (error != NULL) {
            error->code = TIO_ERROR_INVALID_STATE;
        }
        return KII_FALSE;
    }

    if (_onboard_with_vendor_thing_id(&tio->command_handler,
                    vendor_thing_id, password, thing_type,
                    thing_properties, firmware_version, layout_position, error)
            == KII_FALSE) {
        return KII_FALSE;
    }

    if (_set_author(&(tio->state_updater._author),
                    tio->command_handler._author.author_id,
                    tio->command_handler._author.access_token)
            == KII_FALSE) {
        return KII_FALSE;
    }

    tio->state = TIO_STATE_ONBOARDED;

    return KII_TRUE;
}

static kii_bool_t _onboard_with_thing_id(
        kii_t* kii,
        const char* thing_id,
        const char* password,
        const char* thing_type,
        const char* thing_properties,
        const char* firmware_version,
        const char* layout_position,
        tio_error_t* error)
{
    char resource_path[64];
    kii_bool_t succeeded;

    if (_tio_get_anonymous_token(kii, error) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get anonymous token.\n"));
        return KII_FALSE;
    }

    succeeded = _set_onboard_resource_path(
            kii->_app_id,
            resource_path,
            sizeof(resource_path) / sizeof(resource_path[0]));
    if (succeeded == KII_FALSE) {
        M_KII_LOG(kii->kii_core.logger_cb(
                "resource path is longer than expected.\n"));
        return KII_FALSE;
    }

    if (kii_api_call_start(kii, "POST", resource_path, CONTENT_TYPE_THING_ID,
                    KII_TRUE) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb(
            "fail to start api call.\n"));
    }

    /* Append key value pairs. */
    if (APPEND_BODY_CONST(kii, "{") != 0 ||
            _append_key_value_string(kii, "thingID", thing_id, KII_FALSE) != 0 ||
            _append_key_value_string(
                kii, "thingPassword", password, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "thingType", thing_type, KII_TRUE) != 0 ||
            _append_key_value_object_optional(
                kii, "thingProperties", thing_properties, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "firmwareVersion", firmware_version, KII_TRUE) != 0 ||
            _append_key_value_string_optional(
                kii, "layoutPosition", layout_position, KII_TRUE) != 0 ||
            APPEND_BODY_CONST(kii, "}") != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("request size overflowed.\n"));
        return KII_FALSE;
    }

    if (_execute_http_session(kii, error) != KII_TRUE) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to run api.\n"));
        return KII_FALSE;
    }

    if (_thing_if_parse_onboarding_response(kii, error) != 0) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to parse resonse.\n"));
        return KII_FALSE;
    }

    return KII_TRUE;
}

kii_bool_t onboard_with_thing_id(
        tio_t* tio,
        const char* thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties,
        tio_error_t* error)
{
    if (tio->state == TIO_STATE_STARTED) {
        if (error != NULL) {
            error->code = TIO_ERROR_INVALID_STATE;
        }
        return KII_FALSE;
    }

    if (_onboard_with_thing_id(&tio->command_handler, thing_id,
                    password, thing_type, thing_properties, firmware_version,
                    layout_position, error) == KII_FALSE) {
        return KII_FALSE;
    }

    if (_set_author(&(tio->state_updater._author),
                    tio->command_handler._author.author_id,
                    tio->command_handler._author.access_token)
            == KII_FALSE) {
        return KII_FALSE;
    }

    tio->state = TIO_STATE_ONBOARDED;

    return KII_TRUE;
}

kii_bool_t init_tio_with_onboarded_thing(
        tio_t* tio,
        const char* app_id,
        const char* app_key,
        const char* app_host,
        const char* thing_id,
        const char* access_token,
        tio_command_handler_resource_t* command_handler_resource,
        tio_state_updater_resource_t* state_updater_resource,
        tio_system_cb_t* system_cb)
{
    if (_init_tio(tio, app_id, app_key, app_host,
                    command_handler_resource, state_updater_resource,
                    system_cb) == KII_FALSE) {
        return KII_FALSE;
    }

    if (_set_author(&tio->command_handler._author,
                    thing_id, access_token) == KII_FALSE) {
        return KII_FALSE;
    }

    if (_set_author(&tio->state_updater._author,
                    thing_id, access_token) == KII_FALSE) {
        return KII_FALSE;
    }

    tio->state = TIO_STATE_ONBOARDED;

    return KII_TRUE;
}

kii_bool_t start(tio_t* tio)
{
    if (tio->state != TIO_STATE_ONBOARDED) {
        return KII_FALSE;
    }

    // FIXME: Set Keep Alive Interval.
    if (kii_start_push_routine(
            &tio->command_handler, 0, received_callback, tio) != 0) {
        return KII_FALSE;
    }

    tio->state_updater.task_create_cb(
        TIO_TASK_NAME_STATUS_UPDATE,
        _update_status, (void*)tio);

    return KII_TRUE;
}

kii_bool_t get_firmware_version(
        tio_t* tio,
        char* firmware_version,
        size_t firmware_version_len,
        tio_error_t* error)
{
    switch (tio->state) {
        case TIO_STATE_INITIALIZED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_STARTED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_ONBOARDED:
        {
            char resource_path[128];
            kii_t* kii = &(tio->command_handler);
            kii_bool_t succeeded = _set_firmware_version_resource_path(
                    kii->_app_id,
                    kii->_author.author_id,
                    resource_path,
                    sizeof(resource_path) / sizeof(resource_path[0]));

            if (succeeded != KII_TRUE) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "resource path is longer than expected.\n"));
                return KII_FALSE;
            }
            if (_kii_api_call_start(
                    kii, "GET", resource_path, NULL, KII_TRUE, error) != 0) {
                return KII_FALSE;
            }
            if (_execute_http_session(kii, error) != KII_TRUE) {
                return KII_FALSE;
            } else {
                kii_json_field_t fields[2];
                memset(fields, 0x00, sizeof(fields));
                memset(firmware_version, 0x00,
                        sizeof(char) * firmware_version_len);
                fields[0].path = "/firmwareVersion";
                fields[0].type = KII_JSON_FIELD_TYPE_STRING;
                fields[0].field_copy.string = firmware_version;
                fields[0].field_copy_buff_size = firmware_version_len;
                fields[1].path = NULL;
                if (_tio_json_read_object(
                        kii,
                        kii->_rw_buff,
                        kii->_rw_buff_written,
                        fields) != KII_JSON_PARSE_SUCCESS) {
                    if (error != NULL) {
                        if (fields[0].result ==
                                KII_JSON_FIELD_PARSE_COPY_FAILED) {
                            error->code =
                                TIO_ERROR_INSUFFICIENT_ARG_BUFFER;
                        } else {
                            error->code = TIO_ERROR_INVALID_PAYLOAD;
                        }
                    }
                    return KII_FALSE;
                }
                return KII_TRUE;
            }
        }
        default:
            /* Unexpected error*/
            M_TIO_ASSERT(0);
            return KII_FALSE;
    }
}

kii_bool_t update_firmware_version(
        tio_t* tio,
        const char* firmware_version,
        tio_error_t* error)
{
    switch (tio->state) {
        case TIO_STATE_INITIALIZED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_STARTED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_ONBOARDED:
        {
            char resource_path[128];
            kii_t* kii = &(tio->command_handler);
            kii_bool_t succeeded = _set_firmware_version_resource_path(
                    kii->_app_id,
                    kii->_author.author_id,
                    resource_path,
                    sizeof(resource_path) / sizeof(resource_path[0]));

            if (succeeded != KII_TRUE) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "resource path is longer than expected.\n"));
                return KII_FALSE;
            }
            if (_kii_api_call_start(
                    kii,
                    "PUT",
                    resource_path,
                    CONTENT_UPDATE_FIRMWARE_VERSION,
                    KII_TRUE,
                    error) != 0) {
                return KII_FALSE;
            }
            if (APPEND_BODY_CONST(kii, "{") != 0 ||
                    _append_key_value_string(
                        kii, "firmwareVersion", firmware_version, KII_FALSE) != 0 ||
                    APPEND_BODY_CONST(kii, "}") != 0) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "request size overflowed.\n"));
                if (error != NULL) {
                    error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
                }
                return KII_FALSE;
            }
            return _execute_http_session(kii, error) == KII_TRUE ?
                KII_TRUE : KII_FALSE;
        }
        default:
            /* Unexpected error*/
            M_TIO_ASSERT(0);
            return KII_FALSE;
    }
}

kii_bool_t get_thing_type(
        tio_t* tio,
        char* thing_type,
        size_t thing_type_len,
        tio_error_t* error)
{
    switch (tio->state) {
        case TIO_STATE_INITIALIZED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_STARTED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_ONBOARDED:
        {
            char resource_path[128];
            kii_t* kii = &(tio->command_handler);
            kii_bool_t succeeded = _set_thing_type_resource_path(
                    kii->_app_id,
                    kii->_author.author_id,
                    resource_path,
                    sizeof(resource_path) / sizeof(resource_path[0]));

            if (succeeded != KII_TRUE) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "resource path is longer than expected.\n"));
                return KII_FALSE;
            }
            if (_kii_api_call_start(
                    kii, "GET", resource_path, NULL, KII_TRUE, error) != 0) {
                return KII_FALSE;
            }
            if (_execute_http_session(kii, error) != KII_TRUE) {
                return KII_FALSE;
            } else {
                kii_json_field_t fields[2];
                memset(fields, 0x00, sizeof(fields));
                memset(thing_type, 0x00, sizeof(char) * thing_type_len);
                fields[0].path = "/thingType";
                fields[0].type = KII_JSON_FIELD_TYPE_STRING;
                fields[0].field_copy.string = thing_type;
                fields[0].field_copy_buff_size = thing_type_len;
                fields[1].path = NULL;
                if (_tio_json_read_object(
                        kii,
                        kii->_rw_buff,
                        kii->_rw_buff_written,
                        fields) != KII_JSON_PARSE_SUCCESS) {
                    if (error != NULL) {
                        if (fields[0].result ==
                                KII_JSON_FIELD_PARSE_COPY_FAILED) {
                            error->code =
                                TIO_ERROR_INSUFFICIENT_ARG_BUFFER;
                        } else {
                            error->code = TIO_ERROR_INVALID_PAYLOAD;
                        }
                    }
                    return KII_FALSE;
                }
                return KII_TRUE;
            }
        }
        default:
            /* Unexpected error*/
            M_TIO_ASSERT(0);
            return KII_FALSE;
    }
}

kii_bool_t update_thing_type(
        tio_t* tio,
        const char* thing_type,
        tio_error_t* error)
{
    switch (tio->state) {
        case TIO_STATE_INITIALIZED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_STARTED:
            if (error != NULL) {
                error->code = TIO_ERROR_INVALID_STATE;
            }
            return KII_FALSE;
        case TIO_STATE_ONBOARDED:
        {
            char resource_path[128];
            kii_t* kii = &(tio->command_handler);
            kii_bool_t succeeded = _set_thing_type_resource_path(
                    kii->_app_id,
                    kii->_author.author_id,
                    resource_path,
                    sizeof(resource_path) / sizeof(resource_path[0]));

            if (succeeded != KII_TRUE) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "resource path is longer than expected.\n"));
                return KII_FALSE;
            }
            if (_kii_api_call_start(
                    kii,
                    "PUT",
                    resource_path,
                    CONTENT_UPDATE_THING_TYPE,
                    KII_TRUE,
                    error) != 0) {
                return KII_FALSE;
            }
            if (APPEND_BODY_CONST(kii, "{") != 0 ||
                    _append_key_value_string(
                        kii, "thingType", thing_type, KII_FALSE) != 0 ||
                    APPEND_BODY_CONST(kii, "}") != 0) {
                M_KII_LOG(kii->kii_core.logger_cb(
                        "request size overflowed.\n"));
                if (error != NULL) {
                    error->code = TIO_ERROR_INSUFFICIENT_BUFFER;
                }
                return KII_FALSE;
            }
            return _execute_http_session(kii, error) == KII_TRUE ?
                KII_TRUE : KII_FALSE;
        }
        default:
            /* Unexpected error*/
            M_TIO_ASSERT(0);
            return KII_FALSE;
    }
}

/**
 * This function is for testing purpose.
 * Not visible from applications since the function is not declared in public headers.
 */
void test_handle_command(tio_t* ctx, char* buffer, size_t buffer_size)
{
    handle_command(ctx, buffer, buffer_size);
}
