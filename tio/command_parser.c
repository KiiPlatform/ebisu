#include "command_parser.h"
#include "kii.h"
#include "tio_impl.h"
#include "kii_json_utils.h"

#include <stdlib.h>

_cmd_parser_code_t _get_object_in_array(
    kii_json_resource_t* resource,
    KII_JSON_RESOURCE_ALLOC_CB alloc_cb,
    KII_JSON_RESOURCE_FREE_CB free_cb,
    const char* json_array,
    size_t json_array_length,
    size_t index,
    char** out_object,
    size_t* out_object_length)
{
    char idx_str[32];
    int len = snprintf(idx_str, 32, "/[%lu]", (unsigned long)index);
    if (len >= 32) {
        return _CMD_PARSE_ERR_DATA_TOO_LARGE;
    }
    kii_json_field_t field[2];
    memset(field, 0x00, sizeof(field));
    field[0].path = idx_str;
    field[0].type = KII_JSON_FIELD_TYPE_OBJECT;
    field[0].field_copy.string = NULL;
    field[0].result = KII_JSON_FIELD_PARSE_SUCCESS;
    field[1].path = NULL;

    kii_json_parse_result_t res = KII_JSON_PARSE_INVALID_INPUT;
    if (resource != NULL) {
        res = kii_json_parse(json_array, json_array_length, field, resource);
    } else {
        res = kii_json_parse_with_allocator(json_array, json_array_length, field, alloc_cb, free_cb);
    }

    if (res == KII_JSON_PARSE_SUCCESS) {
        *out_object = (char*)(json_array + field[0].start);
        *out_object_length = field[0].end - field[0].start;
        return _CMD_PARSE_OK;
    } else if (res == KII_JSON_PARSE_PARTIAL_SUCCESS) {
        return _CMD_PARSE_ARRAY_OUT_OF_INDEX;
    } else {
        return _CMD_PARSE_FAIL;
    }
}

_cmd_parser_code_t _parse_first_kv(
    const char* object,
    size_t object_length,
    char** out_key,
    size_t* out_key_length,
    char** out_value,
    size_t* out_value_length,
    jsmntype_t* out_value_type)
{
    jsmn_parser parser;
    jsmn_init(&parser);
    jsmnerr_t p_err = JSMN_ERROR_NOMEM;
    jsmntok_t tokens[64];

    p_err = jsmn_parse(&parser, object, object_length, tokens, 64);
    if (p_err >= 0) {
        if (tokens[0].type != JSMN_OBJECT) {
            return _CMD_PARSE_FAIL;
        }
        if (tokens[1].type != JSMN_STRING) {
            return _CMD_PARSE_FAIL;
        }
        *out_key = (char*)(object + tokens[1].start);
        *out_key_length = tokens[1].end - tokens[1].start;
        *out_value = (char*)(object + tokens[2].start);
        *out_value_length = tokens[2].end - tokens[2].start;
        *out_value_type = tokens[2].type;
        return _CMD_PARSE_OK;
    } else if (p_err == JSMN_ERROR_NOMEM) {
        return _CMD_PARSE_ERR_DATA_TOO_LARGE;
    } else {
        return _CMD_PARSE_FAIL;
    }
}

_cmd_parser_code_t _parse_alias(
    tio_handler_t* handler,
    const char* actions_array,
    size_t actions_array_length,
    size_t alias_index,
    char** out_alias,
    size_t* out_alias_length,
    char** out_actions_array_in_alias,
    size_t* out_actions_array_in_alias_length)
{
    char* alias;
    size_t alias_length;
    _cmd_parser_code_t res = _get_object_in_array(
        handler->_kii._json_resource,
        handler->_kii._json_alloc_cb,
        handler->_kii._json_free_cb,
        actions_array,
        actions_array_length,
        alias_index,
        &alias,
        &alias_length);
    if (res != _CMD_PARSE_OK) {
        return res;
    }
    jsmntype_t alias_type = JSMN_OBJECT;
    res = _parse_first_kv(
        alias,
        alias_length,
        out_alias,
        out_alias_length,
        out_actions_array_in_alias,
        out_actions_array_in_alias_length,
        &alias_type);
    return res;
}

int _check_double(const char* str, size_t len)
{
    for (int i = 0; i < len; ++i) {
        if (str[i] == '.' || str[i] == 'e' || str[i] == 'E') {
            return 0;
        }
    }
    return -1;
}

void _parse_primitive(
    const char* action_value,
    size_t action_value_length,
    tio_action_t* out_action)
{
    if (strncmp("null", action_value, (action_value_length > 4) ? action_value_length : 4) == 0) {
        out_action->action_value.type = TIO_TYPE_NULL;
        out_action->action_value.opaque_value_length = action_value_length;
        out_action->action_value.param.opaque_value = action_value;
    } else if (strncmp("true", action_value, (action_value_length > 4) ? action_value_length : 4) == 0) {
        out_action->action_value.type = TIO_TYPE_BOOLEAN;
        out_action->action_value.param.bool_value = KII_TRUE;
    } else if (strncmp("false", action_value, (action_value_length > 5) ? action_value_length : 5) == 0) {
        out_action->action_value.type = TIO_TYPE_BOOLEAN;
        out_action->action_value.param.bool_value = KII_FALSE;
    } else {
        if (_check_double(action_value, action_value_length) == 0) {
            out_action->action_value.type = TIO_TYPE_DOUBLE;
            out_action->action_value.param.double_value = strtod(action_value, NULL);
        } else {
            out_action->action_value.type = TIO_TYPE_INTEGER;
            out_action->action_value.param.long_value = strtol(action_value, NULL, 10);
        }
    }
}

_cmd_parser_code_t _parse_action(
    tio_handler_t* handler,
    const char* alias,
    size_t alias_length,
    const char* actions_array_in_alias,
    size_t actions_array_in_alias_length,
    size_t action_index,
    tio_action_t* out_action)
{
    char* action_object = NULL;
    size_t action_object_length = 0;
    _cmd_parser_code_t res = _get_object_in_array(
        handler->_kii._json_resource,
        handler->_kii._json_alloc_cb,
        handler->_kii._json_free_cb,
        actions_array_in_alias,
        actions_array_in_alias_length,
        action_index,
        &action_object,
        &action_object_length);
    if (res != _CMD_PARSE_OK) {
        return res;
    }
    char* action_name = NULL;
    size_t action_name_length = 0;
    char* action_value = NULL;
    size_t action_value_length = 0;
    jsmntype_t action_type = JSMN_OBJECT;
    res = _parse_first_kv(
        action_object,
        action_object_length,
        &action_name,
        &action_name_length,
        &action_value,
        &action_value_length,
        &action_type);
    if (res != _CMD_PARSE_OK) {
        return res;
    }
    out_action->alias = alias;
    out_action->alias_length = alias_length;
    out_action->action_name = action_name;
    out_action->action_name_length = action_name_length;
    switch (action_type) {
        case JSMN_ARRAY:
            out_action->action_value.type = TIO_TYPE_ARRAY;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JSMN_OBJECT:
            out_action->action_value.type = TIO_TYPE_OBJECT;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JSMN_STRING:
            out_action->action_value.type = TIO_TYPE_STRING;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JSMN_PRIMITIVE:
            _parse_primitive(action_value, action_value_length, out_action);
            break;
        default:
            break;
    }
    return _CMD_PARSE_OK;
}

static tio_code_t _start_result_request(
    tio_handler_t* handler,
    const char* command_id)
{
    // Start making command result request.
    char command_result_path[256];
    int path_len = snprintf(
        command_result_path,
        256,
        "/thing-if/apps/%s/targets/thing:%s/commands/%s/action-results",
        handler->_kii._app_id, handler->_kii._author.author_id, command_id);
    if (path_len >= 256) {
        return TIO_ERR_TOO_LARGE_DATA;
    }
    kii_code_t s_res = kii_api_call_start(
        &handler->_kii,
        "PUT",
        command_result_path,
        "application/json",
        KII_TRUE);
    if (s_res != KII_ERR_OK) {
        return _tio_convert_code(s_res);
    }
    const char result_start[] = "{\"actionResults\":[";
    kii_code_t a_res = kii_api_call_append_body(
        &handler->_kii,
        result_start,
        strlen(result_start));
    if (a_res != KII_ERR_OK) {
        return _tio_convert_code(s_res);
    }
    return TIO_ERR_OK;
}

static tio_code_t _append_action_result(
    tio_handler_t* handler,
    size_t index,
    tio_bool_t succeeded,
    tio_action_t* action,
    const char* err_message,
    char* work_buff,
    size_t work_buff_size)
{
    char *comma = ",";
    if (index == 0)
    {
        comma = "";
    }
    char action_name[action->action_name_length+1];
    strncpy(action_name, action->action_name, action->action_name_length);
    action_name[action->action_name_length] = '\0';
    if (succeeded == KII_TRUE)
    {
        int len = snprintf(
            work_buff, work_buff_size,
            "%s{\"%s\" : { \"succeeded\" : true }}",
            comma, action_name);
        if (len >= work_buff_size)
        {
            return TIO_ERR_TOO_LARGE_DATA;
        }
        kii_code_t res =  kii_api_call_append_body(&handler->_kii, work_buff, len);
        return _tio_convert_code(res);
    }
    else
    {
        char *err_part = "";
        char temp_err[128];
        size_t msg_len = strlen(err_message);
        if (msg_len > 0)
        {
            size_t temp_buff_size = msg_len * 2 + 1;
            char esc_msg[temp_buff_size];
            int esc_len = kii_json_escape_str(err_message, esc_msg, sizeof(esc_msg)/sizeof(esc_msg[0]));
            if (esc_len < 0) {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            int len = snprintf(temp_err, 128, ",\"errorMessage\":\"%s\"", esc_msg);
            if (len >= 128)
            {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            err_part = temp_err;
        }
        int len = snprintf(
            work_buff, work_buff_size,
            "%s{\"%s\" : { \"succeeded\" : false %s}}",
            comma, action_name, err_part);
        if (len >= work_buff_size)
        {
            return TIO_ERR_TOO_LARGE_DATA;
        }
        kii_code_t res = kii_api_call_append_body(&handler->_kii, work_buff, len);
        return _tio_convert_code(res);
    }
}

//
// actions : [
//    {alias1 : [ { setPower: true}, {setTemp : 25} ]}
//    {alias2 : [ {...}, ... ]},...
// ]
// actionResults :
//  [
//    {setPower : {succeeded : true}},
//    {setTemp : {succeeded : false, message : "com err"}},
// ]
tio_code_t _handle_command(
    tio_handler_t* handler,
    const char* command,
    size_t command_length)
{
    kii_json_field_t fields[3];
    char command_id[64];
    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/commandID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = command_id;
    fields[0].field_copy_buff_size = sizeof(command_id) / sizeof(command_id[0]);
    fields[1].path = "/actions";
    fields[1].type = KII_JSON_FIELD_TYPE_ARRAY;
    fields[1].field_copy.string = NULL;
    fields[1].result = KII_JSON_FIELD_PARSE_SUCCESS;
    fields[2].path = NULL;

    kii_json_parse_result_t res = _parse_json(handler, command, command_length, fields);
    if (res != KII_JSON_PARSE_SUCCESS) {
        return TIO_ERR_PARSE_JSON;
    }

    tio_code_t s_res = _start_result_request(handler, command_id);
    if (s_res != TIO_ERR_OK) {
        return s_res;
    }

    const char* actions_array = command + fields[1].start;
    size_t actions_array_length = fields[1].end - fields[1].start;
    tio_action_t action;
    size_t result_counts = 0;
    for (size_t alias_idx = 0; ; ++alias_idx) {
        char* actions_array_in_alias = NULL;
        size_t actions_array_in_alias_length = 0;
        char* alias = NULL;
        size_t alias_length = 0;
        _cmd_parser_code_t res = _parse_alias(
            handler,
            actions_array,
            actions_array_length,
            alias_idx,
            &alias,
            &alias_length,
            &actions_array_in_alias,
            &actions_array_in_alias_length
        );
        if (res == _CMD_PARSE_OK) {
            for (size_t action_idx = 0; ; ++action_idx) {
                _cmd_parser_code_t pa_res = _parse_action(
                    handler,
                    alias,
                    alias_length,
                    actions_array_in_alias,
                    actions_array_in_alias_length,
                    action_idx,
                    &action);
                if (pa_res == _CMD_PARSE_OK) {
                    tio_action_err_t action_err;
                    action_err.err_message[0] = '\0';
                    tio_bool_t succeeded = handler->_cb_action(&action, &action_err, handler->_cb_action_data);
                    char work_buff[256];
                    tio_code_t app_res = _append_action_result(
                        handler,
                        result_counts,
                        succeeded,
                        &action,
                        action_err.err_message,
                        work_buff,
                        sizeof(work_buff)/sizeof(work_buff[0]));
                    if (app_res != TIO_ERR_OK) {
                        return app_res;
                    }
                    ++result_counts;
                } else if (pa_res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
                    // Handled all actions in alias.
                    break;
                } else {
                    return TIO_ERR_PARSE_JSON;
                }
            }
        } else if ( res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
            // Handled all actions in command.
            kii_code_t res = kii_api_call_append_body(&handler->_kii, "]}", 2);
            if (res != KII_ERR_OK) {
                return _tio_convert_code(res);
            }
            break;
        } else {
            return TIO_ERR_PARSE_JSON;
        }
    }
    kii_code_t run_res = kii_api_call_run(&handler->_kii);
    if (run_res != KII_ERR_OK) {
        return _tio_convert_code(run_res);
    }
    return TIO_ERR_OK;
}

kii_json_parse_result_t _parse_json(
    tio_handler_t* handler,
    const char* json_string,
    size_t json_string_size,
    kii_json_field_t* fields)
{
    kii_json_resource_t* resource = handler->_kii._json_resource;
    kii_json_parse_result_t res = KII_JSON_PARSE_INVALID_INPUT;
    if (resource != NULL) {
        res = kii_json_parse(json_string, json_string_size, fields, resource);
    } else {
        KII_JSON_RESOURCE_ALLOC_CB alloc_cb = handler->_kii._json_alloc_cb;
        KII_JSON_RESOURCE_FREE_CB free_cb = handler->_kii._json_free_cb;
        res = kii_json_parse_with_allocator(json_string, json_string_size, fields, alloc_cb, free_cb);
    }
    return res;
}
