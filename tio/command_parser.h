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

_cmd_parser_code_t _get_object_in_array(
        jkii_resource_t* resource,
        JKII_CB_RESOURCE_ALLOC cb_alloc,
        JKII_CB_RESOURCE_FREE cb_free,
        const char* json_array,
        size_t json_array_length,
        size_t index,
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
        jsmntype_t* out_value_type);

_cmd_parser_code_t _parse_action(
        tio_handler_t* handler,
        const char* alias,
        size_t alias_length,
        const char* actions_array_in_alias,
        size_t actions_array_in_alias_length,
        size_t action_index,
        tio_action_t* out_action);

tio_code_t _handle_command(
        tio_handler_t* handler,
        const char* command,
        size_t command_length);

jkii_parse_err_t _parse_json(
        tio_handler_t* handler,
        const char* json_string,
        size_t json_string_size,
        jkii_field_t* fields);

#ifdef __cplusplus
}
#endif

#endif
