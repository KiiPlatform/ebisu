#ifndef _command_parser_
#define _command_parser_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "tio.h"

typedef enum _cmd_parser_code_t {
    _CMD_PARSE_OK,
    _CMD_PARSE_ARRAY_OUT_OF_INDEX,
    _CMD_PARSE_ERR_DATA_TOO_LARGE,
    _CMD_PARSE_FAIL
} _cmd_parser_code_t;

// The next object in json_array, which iter, zeroed before the first call,
// walks. _CMD_PARSE_ARRAY_OUT_OF_INDEX after the last, and _CMD_PARSE_FAIL
// for an element that isn't an object, or for json_array not an array.
_cmd_parser_code_t _next_object_in_array(
        const char* json_array,
        size_t json_array_length,
        jkii_member_iter_t* iter,
        char** out_object,
        size_t* out_object_length);

// Parse first key and value in the object.
// Command objects have only one key value.
_cmd_parser_code_t _parse_first_kv(
        const char* object,
        size_t object_length,
        char** out_key,
        size_t* out_key_length,
        char** out_value,
        size_t* out_value_length,
        jkii_field_type_t* out_value_type);

_cmd_parser_code_t _parse_action(
        tio_handler_t* handler,
        const char* alias,
        size_t alias_length,
        const char* actions_array_in_alias,
        size_t actions_array_in_alias_length,
        jkii_member_iter_t* action_iter,
        tio_action_t* out_action);

tio_code_t _handle_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length,
        TIO_CB_ACTION cb_action,
        void* cb_action_data);

jkii_parse_err_t _parse_json(
        tio_handler_t* handler,
        const char* json_string,
        size_t json_string_size,
        jkii_field_t* fields);

tio_code_t _parse_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length,
        TIO_CB_PARSED_ACTION cb_parsed_action,
        void* userdata);

#ifdef __cplusplus
}
#endif

#endif
