#include "command_parser.h"
#include "kii.h"
#include "tio_impl.h"
#include "kii_json_utils.h"

_cmd_parser_code_t _get_object_in_array(
    const char* json_array,
    size_t json_array_length,
    size_t index,
    char** out_object,
    size_t* out_object_length)
{
    // TODO: implement it.
    return _CMD_PARSE_FAIL;
}

_cmd_parser_code_t _parse_first_kv(
    const char* object,
    size_t object_length,
    char** out_key,
    size_t* out_key_length,
    char** out_value,
    size_t* out_value_length)
{
    // TODO: implement it.
    return _CMD_PARSE_FAIL;
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
    // TODO: implement it.
    return _CMD_PARSE_FAIL;
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
    // TODO: implement it.
    return _CMD_PARSE_FAIL;
}

static tio_code_t _start_result_request(
    tio_handler_t* handler,
    const char* command_id)
{
    // Start making command result request.
    char command_result_path[128];
    int path_len = snprintf(
        command_result_path,
        128,
        "/thing-if/apps/%s/targets/thing:%s/commands/%s",
        handler->_kii._app_id, handler->_kii._author.author_id, command_id);
    if (path_len >= 128) {
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
            int esc_len = kii_escape_str(err_message, esc_msg, sizeof(esc_msg)/sizeof(esc_msg[0]));
            if (esc_len < 0) {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            int len = snprintf(temp_err, 128, ",\"errorMessagea\":\"%s\"", esc_msg);
            if (len >= 128)
            {
                return TIO_ERR_TOO_LARGE_DATA;
            }
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
    fields[1].path = "/actions";
    fields[1].type = KII_JSON_FIELD_TYPE_ARRAY;
    fields[1].field_copy.string = NULL;
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
                    tio_code_t app_res =_append_action_result(
                        handler,
                        action_idx,
                        succeeded,
                        &action,
                        action_err.err_message,
                        work_buff,
                        sizeof(work_buff)/sizeof(work_buff[0]));
                    if (app_res != TIO_ERR_OK) {
                        return app_res;
                    }
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