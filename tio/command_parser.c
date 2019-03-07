#include "tio.h"
#include "command_parser_impl.h"
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
tio_code_t tio_handler_handle_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length)
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