#ifndef _command_parser_
#define _command_parser_

#include <string.h>
#include "tio2.h"

typedef enum _cmd_parser_code_t {
    _CMD_PARSE_OK,
    _CMD_PARSE_ARRAY_OUT_OF_INDEX,
    _CMD_PARSE_ERR_DATA_TOO_LARGE,
    _CMD_PARSE_FAIL
} _cmd_parser_code_t;

_cmd_parser_code_t _get_object_in_array(
    kii_json_resource_t* resource,
    KII_JSON_RESOURCE_ALLOC_CB alloc_cb,
    KII_JSON_RESOURCE_FREE_CB free_cb,
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
    size_t* out_value_length);

_cmd_parser_code_t _parse_action_object(
    const char* alias,
    size_t alias_length,
    const char* action_object,
    size_t action_object_length,
    tio_action_t* action);

tio_code_t _handle_action(
    tio_handler_t* handler,
    const char* actions_array,
    size_t actions_array_length);

tio_code_t _handle_command(
    tio_handler_t* handler,
    const char* command,
    size_t command_length);

kii_json_parse_result_t _parse_json(
    tio_handler_t* handler,
    const char* json_string,
    size_t json_string_size,
    kii_json_field_t* fields);

#endif