#include "command_parser.h"
#include "kii.h"
#include "tio_impl.h"
#include "jkii_utils.h"

#include <ebisu_str.h>

/* A step of handling a command that failed, with its code, at
   EBISU_LOG_DEBUG: the warning that the command failed says no more. */
#define _log_step(handler, step, code) \
    _tio_log(&(handler)->_kii, EBISU_LOG_DEBUG, "%s: %d", (step), (int)(code))

_cmd_parser_code_t _next_object_in_array(
        const char* json_array,
        size_t json_array_length,
        jkii_member_iter_t* iter,
        char** out_object,
        size_t* out_object_length)
{
    // Walked once, an element after the other. Looking each element up by its
    // index checked the array and walked it from its start again for every
    // one, so a command took time to the square of its actions.
    jkii_member_t member;
    jkii_parse_err_t res = jkii_next_member(json_array, json_array_length,
            iter, &member);
    if (res == JKII_ERR_OK) {
        // An element that isn't an object, or a member of what is an object
        // and not an array, is malformed. Taken for the end of the array, it
        // dropped what came after it, and the command was taken as good.
        if (member.key != NULL || member.type != JKII_FIELD_TYPE_OBJECT) {
            return _CMD_PARSE_FAIL;
        }
        *out_object = (char*)member.value;
        *out_object_length = member.value_length;
        return _CMD_PARSE_OK;
    } else if (res == JKII_ERR_NOT_FOUND) {
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
        jkii_field_type_t* out_value_type)
{
    jkii_member_iter_t iter;
    jkii_member_t member;
    memset(&iter, 0x00, sizeof(iter));
    // An object, with a member: an array's element has no key.
    if (jkii_next_member(object, object_length, &iter, &member) != JKII_ERR_OK ||
            member.key == NULL) {
        return _CMD_PARSE_FAIL;
    }
    *out_key = (char*)member.key;
    *out_key_length = member.key_length;
    *out_value = (char*)member.value;
    *out_value_length = member.value_length;
    *out_value_type = member.type;
    return _CMD_PARSE_OK;
}

_cmd_parser_code_t _parse_alias(
        tio_handler_t* handler,
        const char* actions_array,
        size_t actions_array_length,
        jkii_member_iter_t* alias_iter,
        char** out_alias,
        size_t* out_alias_length,
        char** out_actions_array_in_alias,
        size_t* out_actions_array_in_alias_length)
{
    char* alias;
    size_t alias_length;
    _cmd_parser_code_t res = _next_object_in_array(
            actions_array,
            actions_array_length,
            alias_iter,
            &alias,
            &alias_length);
    if (res != _CMD_PARSE_OK) {
        return res;
    }
    jkii_field_type_t alias_type = JKII_FIELD_TYPE_OBJECT;
    res = _parse_first_kv(
            alias,
            alias_length,
            out_alias,
            out_alias_length,
            out_actions_array_in_alias,
            out_actions_array_in_alias_length,
            &alias_type);
    if (res == _CMD_PARSE_OK && alias_type != JKII_FIELD_TYPE_ARRAY) {
        // An alias's actions are an array.
        return _CMD_PARSE_FAIL;
    }
    return res;
}

_cmd_parser_code_t _parse_primitive(
        const char* action_value,
        size_t action_value_length,
        tio_action_t* out_action)
{
    jkii_primitive_t jprim;
    // jkii has checked it is JSON, but does not set jprim for a number out of
    // range.
    jkii_primitive_err_t p_res = jkii_parse_primitive(action_value, action_value_length, &jprim);
    if (p_res != JKII_PRIMITIVE_ERR_OK) {
        return _CMD_PARSE_FAIL;
    }
    switch (jprim.type) {
        case JKII_FIELD_TYPE_NULL:
            out_action->action_value.type = TIO_TYPE_NULL;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JKII_FIELD_TYPE_BOOLEAN:
            out_action->action_value.type = TIO_TYPE_BOOLEAN;
            out_action->action_value.param.bool_value = jprim.value.boolean_value == JKII_TRUE;
            break;
        case JKII_FIELD_TYPE_INTEGER:
            out_action->action_value.type = TIO_TYPE_INTEGER;
            out_action->action_value.param.long_value = jprim.value.int_value;
            break;
        case JKII_FIELD_TYPE_LONG:
            out_action->action_value.type = TIO_TYPE_INTEGER;
            out_action->action_value.param.long_value = jprim.value.long_value;
            break;
        case JKII_FIELD_TYPE_DOUBLE:
            out_action->action_value.type = TIO_TYPE_DOUBLE;
            out_action->action_value.param.double_value = jprim.value.double_value;
            break;
        default:
            // Programing error.
            break;
    }
    return _CMD_PARSE_OK;
}

_cmd_parser_code_t _parse_action(
        tio_handler_t* handler,
        const char* alias,
        size_t alias_length,
        const char* actions_array_in_alias,
        size_t actions_array_in_alias_length,
        jkii_member_iter_t* action_iter,
        tio_action_t* out_action)
{
    char* action_object = NULL;
    size_t action_object_length = 0;
    _cmd_parser_code_t res = _next_object_in_array(
            actions_array_in_alias,
            actions_array_in_alias_length,
            action_iter,
            &action_object,
            &action_object_length);
    if (res != _CMD_PARSE_OK) {
        return res;
    }
    char* action_name = NULL;
    size_t action_name_length = 0;
    char* action_value = NULL;
    size_t action_value_length = 0;
    jkii_field_type_t action_type = JKII_FIELD_TYPE_OBJECT;
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
        case JKII_FIELD_TYPE_ARRAY:
            out_action->action_value.type = TIO_TYPE_ARRAY;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JKII_FIELD_TYPE_OBJECT:
            out_action->action_value.type = TIO_TYPE_OBJECT;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        case JKII_FIELD_TYPE_STRING:
            out_action->action_value.type = TIO_TYPE_STRING;
            out_action->action_value.opaque_value_length = action_value_length;
            out_action->action_value.param.opaque_value = action_value;
            break;
        default:
            // A number, true, false or null.
            return _parse_primitive(action_value, action_value_length, out_action);
    }
    return _CMD_PARSE_OK;
}

static tio_code_t _walk_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length,
        const TIO_CB_PARSED_ACTION cb_parsed_action,
        void* userdata);

static void _ignore_action(char* command_id, tio_action_t* action, void* userdata)
{
    (void)command_id;
    (void)action;
    (void)userdata;
}

// A malformed command is refused whole, before any of its actions runs:
// refused when the walk reached its malformed part, it had run the actions
// before it, whose results were never sent.
static tio_code_t _check_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length)
{
    return _walk_command(handler, command, command_length, _ignore_action, NULL);
}

static tio_code_t _start_result_request(
        tio_handler_t* handler,
        const char* command_id)
{
    // Start making command result request.
    char command_result_path[256];
    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, command_result_path, sizeof(command_result_path));
    ebisu_strbuf_appendf(&path, "/thing-if/apps/%s/targets/thing:%s/commands/%s/action-results",
            handler->_kii._app_id, handler->_kii._author.author_id, command_id);
    if (!ebisu_strbuf_ok(&path)) {
        return TIO_ERR_TOO_LARGE_DATA;
    }
    kii_code_t s_res = kii_api_call_start(
            &handler->_kii,
            "PUT",
            command_result_path,
            "application/vnd.kii.CommandResultsUpdateRequest+json",
            true);
    if (s_res != KII_ERR_OK) {
        _log_step(handler, "kii_api_call_start", s_res);
        return _tio_convert_code(s_res);
    }
    const char result_start[] = "{\"actionResults\":[";
    kii_code_t a_res = kii_api_call_append_body(
            &handler->_kii,
            result_start,
            strlen(result_start));
    if (a_res != KII_ERR_OK) {
        _log_step(handler, "kii_api_call_append_body", a_res);
        return _tio_convert_code(a_res);
    }
    return TIO_ERR_OK;
}

jkii_parse_err_t _validate_json(
        tio_handler_t* handler,
        const char* json_string,
        size_t json_string_size)
{
    (void)handler;
    return jkii_validate_root_object(json_string, json_string_size);
}

static tio_code_t _append_action_result(
        tio_handler_t* handler,
        size_t index,
        bool succeeded,
        tio_action_t* action,
        const char* err_message,
        const char* json_data,
        char* work_buff,
        size_t work_buff_size)
{
    int json_data_len = strlen(json_data);
    if (json_data_len > 0) {
        // Validate json_data.
        jkii_parse_err_t json_res = _validate_json(handler, json_data, json_data_len);
        if (json_res != JKII_ERR_OK) {
            _log_step(handler, "_validate_json", json_res);
            kii_cancel_request_being_prepared(&handler->_kii);
            return TIO_ERR_PARSE_JSON;
        }
    }
    char *comma = ",";
    if (index == 0)
    {
        comma = "";
    }
    // The action name is a slice of the parsed command, so its length comes
    // from the network. Copying it to the stack first sized the array by that
    // length, letting whoever publishes a command choose how much stack to
    // consume -- bounded only by the buffer passed to kii_set_mqtt_buff(), on
    // targets whose whole stack may be smaller than that. It is printed with a
    // precision instead, which needs no copy and no allocation.
    if (action->action_name_length >= work_buff_size) {
        return TIO_ERR_TOO_LARGE_DATA;
    }
    const int action_name_length = (int)action->action_name_length;
    ebisu_strbuf_t result;
    ebisu_strbuf_init(&result, work_buff, work_buff_size);
    if (succeeded)
    {
        ebisu_strbuf_appendf(&result, "%s{\"%.*s\":{\"succeeded\":true",
                comma, action_name_length, action->action_name);
    }
    else
    {
        char *err_part = "";
        char temp_err[128];
        size_t msg_len = strlen(err_message);
        if (msg_len > 0)
        {
            // Escaping can double the message, so this holds the longest
            // tio_action_err_t::err_message can be. Sizing it from msg_len made
            // it a variable length array whose bound depended on a caller the
            // parameter type does not constrain.
            char esc_msg[TIO_ACTION_ERR_MESSAGE_MAX_SIZE * 2 - 1];
            if (msg_len * 2 + 1 > sizeof(esc_msg)) {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            int esc_len = jkii_escape_str(err_message, esc_msg, sizeof(esc_msg)/sizeof(esc_msg[0]));
            if (esc_len < 0) {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            ebisu_strbuf_t err;
            ebisu_strbuf_init(&err, temp_err, sizeof(temp_err));
            ebisu_strbuf_appendf(&err, ",\"errorMessage\":\"%s\"", esc_msg);
            if (!ebisu_strbuf_ok(&err))
            {
                return TIO_ERR_TOO_LARGE_DATA;
            }
            err_part = temp_err;
        }
        ebisu_strbuf_appendf(&result, "%s{\"%.*s\":{\"succeeded\":false%s",
                comma, action_name_length, action->action_name, err_part);
    }
    if (json_data_len > 0) {
        ebisu_strbuf_append(&result, ",\"data\":");
        ebisu_strbuf_append(&result, json_data);
    }
    ebisu_strbuf_append(&result, "}}");
    if (!ebisu_strbuf_ok(&result))
    {
        return TIO_ERR_TOO_LARGE_DATA;
    }
    kii_code_t res = kii_api_call_append_body(&handler->_kii, work_buff, result.len);
    return _tio_convert_code(res);
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
        size_t command_length,
        TIO_CB_ACTION cb_action,
        void* cb_action_data)
{
    tio_code_t c_res = _check_command(handler, command, command_length);
    if (c_res != TIO_ERR_OK) {
        return c_res;
    }

    jkii_field_t fields[3];
    char command_id[64];
    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/commandID";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = command_id;
    fields[0].field_copy_buff_size = sizeof(command_id) / sizeof(command_id[0]);
    fields[1].path = "/actions";
    fields[1].type = JKII_FIELD_TYPE_ARRAY;
    fields[1].field_copy.string = NULL;
    fields[1].result = JKII_FIELD_ERR_OK;
    fields[2].path = NULL;

    jkii_parse_err_t res = _parse_json(handler, command, command_length, fields);
    if (res != JKII_ERR_OK) {
        return TIO_ERR_PARSE_JSON;
    }

    tio_code_t s_res = _start_result_request(handler, command_id);
    if (s_res != TIO_ERR_OK) {
        return s_res;
    }

    const char* actions_array = command + fields[1].start;
    size_t actions_array_length = fields[1].end - fields[1].start;
    tio_action_t action;
    jkii_member_iter_t alias_iter;
    memset(&alias_iter, 0x00, sizeof(alias_iter));
    for (size_t alias_idx = 0; ; ++alias_idx) {
        char* actions_array_in_alias = NULL;
        size_t actions_array_in_alias_length = 0;
        char* alias = NULL;
        size_t alias_length = 0;
        _cmd_parser_code_t res = _parse_alias(
                handler,
                actions_array,
                actions_array_length,
                &alias_iter,
                &alias,
                &alias_length,
                &actions_array_in_alias,
                &actions_array_in_alias_length
                );
        if (res == _CMD_PARSE_OK) {
            kii_code_t kii_res = KII_ERR_OK;
            if (alias_idx > 0) {
                kii_res = kii_api_call_append_body(&handler->_kii, ",{\"", 3);
            } else {
                kii_res = kii_api_call_append_body(&handler->_kii, "{\"", 2);
            }
            if (kii_res != KII_ERR_OK) {
                _log_step(handler, "kii_api_call_append_body (CMD_PARSE_OK, 1)", kii_res);
                return _tio_convert_code(kii_res);
            }
            kii_res = kii_api_call_append_body(&handler->_kii, alias, alias_length);
            if (kii_res != KII_ERR_OK) {
                _log_step(handler, "kii_api_call_append_body (CMD_PARSE_OK, 2)", kii_res);
                return _tio_convert_code(kii_res);
            }
            kii_res = kii_api_call_append_body(&handler->_kii, "\":[", 3);
            if (kii_res != KII_ERR_OK) {
                _log_step(handler, "kii_api_call_append_body (CMD_PARSE_OK, 3)", kii_res);
                return _tio_convert_code(kii_res);
            }
            jkii_member_iter_t action_iter;
            memset(&action_iter, 0x00, sizeof(action_iter));
            for (size_t action_idx = 0; ; ++action_idx) {
                _cmd_parser_code_t pa_res = _parse_action(
                        handler,
                        alias,
                        alias_length,
                        actions_array_in_alias,
                        actions_array_in_alias_length,
                        &action_iter,
                        &action);
                if (pa_res == _CMD_PARSE_OK) {
                    tio_action_err_t action_err;
                    action_err.err_message[0] = '\0';
                    tio_action_result_data_t action_result_data;
                    action_result_data.json[0] = '\0';
                    bool succeeded = cb_action(&action, &action_err, &action_result_data, cb_action_data);
                    char work_buff[256];
                    // The index within the alias, whose results are an array
                    // of their own: counted across the command, the first
                    // result of every alias after the first was written with
                    // a leading comma, and the server refused the body.
                    tio_code_t app_res = _append_action_result(
                            handler,
                            action_idx,
                            succeeded,
                            &action,
                            action_err.err_message,
                            action_result_data.json,
                            work_buff,
                            sizeof(work_buff)/sizeof(work_buff[0]));
                    if (app_res != TIO_ERR_OK) {
                        _log_step(handler, "_append_action_result", app_res);
                        kii_cancel_request_being_prepared(&handler->_kii);
                        return app_res;
                    }
                } else if (pa_res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
                    // Handled all actions in alias.
                    break;
                } else {
                    _log_step(handler, "_parse_action", pa_res);
                    kii_cancel_request_being_prepared(&handler->_kii);
                    return TIO_ERR_PARSE_JSON;
                }
            }
            kii_res = kii_api_call_append_body(&handler->_kii, "]}", 2);
            if (kii_res != KII_ERR_OK) {
                _log_step(handler, "kii_api_call_append_body (_CMD_PARSE_OK)", kii_res);
                return _tio_convert_code(kii_res);
            }
        } else if ( res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
            // Handled all actions in command.
            kii_code_t res = kii_api_call_append_body(&handler->_kii, "]}", 2);
            if (res != KII_ERR_OK) {
                _log_step(handler, "kii_api_call_append_body (Handled all actions in command)", res);
                return _tio_convert_code(res);
            }
            break;
        } else {
            _log_step(handler, "_parse_alias", res);
            kii_cancel_request_being_prepared(&handler->_kii);
            return TIO_ERR_PARSE_JSON;
        }
    }
    kii_code_t run_res = kii_api_call_run(&handler->_kii);
    if (run_res != KII_ERR_OK) {
        _log_step(handler, "kii_api_call_run", run_res);
        return _tio_convert_code(run_res);
    }
    // kii_api_call_run only reports transport errors.
    uint16_t resp_code = kii_get_resp_status(&handler->_kii);
    if (resp_code < 200 || 300 <= resp_code) {
        _log_step(handler, "kii_get_resp_status", resp_code);
        return TIO_ERR_RESP_STATUS;
    }
    return TIO_ERR_OK;
}

jkii_parse_err_t _parse_json(
        tio_handler_t* handler,
        const char* json_string,
        size_t json_string_size,
        jkii_field_t* fields)
{
    (void)handler;
    return jkii_parse(json_string, json_string_size, fields);
}


tio_code_t _parse_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length,
        const TIO_CB_PARSED_ACTION cb_parsed_action,
        void* userdata)
{
    tio_code_t c_res = _check_command(handler, command, command_length);
    if (c_res != TIO_ERR_OK) {
        return c_res;
    }
    return _walk_command(handler, command, command_length, cb_parsed_action,
            userdata);
}

static tio_code_t _walk_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length,
        const TIO_CB_PARSED_ACTION cb_parsed_action,
        void* userdata)
{
    jkii_field_t fields[3];
    char command_id[64];
    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/commandID";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = command_id;
    fields[0].field_copy_buff_size = sizeof(command_id) / sizeof(command_id[0]);
    fields[1].path = "/actions";
    fields[1].type = JKII_FIELD_TYPE_ARRAY;
    fields[1].field_copy.string = NULL;
    fields[1].result = JKII_FIELD_ERR_OK;
    fields[2].path = NULL;

    jkii_parse_err_t res = _parse_json(handler, command, command_length, fields);
    if (res != JKII_ERR_OK) {
        _log_step(handler, "_parse_json", res);
        return TIO_ERR_PARSE_JSON;
    }

    const char* actions_array = command + fields[1].start;
    size_t actions_array_length = fields[1].end - fields[1].start;
    tio_action_t action;
    jkii_member_iter_t alias_iter;
    memset(&alias_iter, 0x00, sizeof(alias_iter));
    for (;;) {
        char* actions_array_in_alias = NULL;
        size_t actions_array_in_alias_length = 0;
        char* alias = NULL;
        size_t alias_length = 0;
        _cmd_parser_code_t res = _parse_alias(
                handler,
                actions_array,
                actions_array_length,
                &alias_iter,
                &alias,
                &alias_length,
                &actions_array_in_alias,
                &actions_array_in_alias_length
                );
        if (res == _CMD_PARSE_OK) {
            jkii_member_iter_t action_iter;
            memset(&action_iter, 0x00, sizeof(action_iter));
            for (;;) {
                _cmd_parser_code_t pa_res = _parse_action(
                        handler,
                        alias,
                        alias_length,
                        actions_array_in_alias,
                        actions_array_in_alias_length,
                        &action_iter,
                        &action);
                if (pa_res == _CMD_PARSE_OK) {
                    cb_parsed_action(command_id, &action, userdata);
                } else if (pa_res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
                    // Handled all actions in alias.
                    break;
                } else {
                    _log_step(handler, "_parse_action", pa_res);
                    return TIO_ERR_PARSE_JSON;
                }
            }
        } else if ( res == _CMD_PARSE_ARRAY_OUT_OF_INDEX) {
            // Handled all actions in command.
            break;
        } else {
            _log_step(handler, "_parse_alias", res);
            return TIO_ERR_PARSE_JSON;
        }
    }
    return TIO_ERR_OK;
}
