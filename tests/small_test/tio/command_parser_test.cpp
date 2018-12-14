#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "tio.h"
#include "command_parser.h"
#include "jkii.h"

TEST_CASE( "_get_object_in_array" ) {
    const char json_arr[] = "[{\"a\":1},{\"b\":2}]";

    jkii_token_t tokens[16];
    jkii_resource_t resource = {tokens, 16};

    char* obj_str = NULL;
    size_t obj_str_len = 0;

    SECTION("Get object at index 0") {
        _cmd_parser_code_t p_res = _get_object_in_array(
                &resource,
                NULL,
                NULL,
                json_arr,
                strlen(json_arr),
                0,
                &obj_str,
                &obj_str_len
                );
        REQUIRE( p_res == _CMD_PARSE_OK );
        REQUIRE( obj_str_len == 7);
        char obj_str_copy[obj_str_len+1];
        strncpy(obj_str_copy, obj_str, obj_str_len);
        REQUIRE( strcmp(obj_str_copy, "{\"a\":1}") == 0 );
    }

    SECTION("Get object at index 1") {
        _cmd_parser_code_t p_res = _get_object_in_array(
                &resource,
                NULL,
                NULL,
                json_arr,
                strlen(json_arr),
                1,
                &obj_str,
                &obj_str_len
                );
        REQUIRE( p_res == _CMD_PARSE_OK );
        REQUIRE( obj_str_len == 7);
        char obj_str_copy[obj_str_len+1];
        strncpy(obj_str_copy, obj_str, obj_str_len);
        REQUIRE( strcmp(obj_str_copy, "{\"b\":2}") == 0 );
    }

    SECTION("Get object at index 2") {
        _cmd_parser_code_t p_res = _get_object_in_array(
                &resource,
                NULL,
                NULL,
                json_arr,
                strlen(json_arr),
                2,
                &obj_str,
                &obj_str_len
                );
        REQUIRE( p_res == _CMD_PARSE_ARRAY_OUT_OF_INDEX );
    }
}

TEST_CASE( "_parse_first_kv" ) {
    // TODO: Add other types test.
    SECTION("Int value") {
        const char json_obj[] = "{\"a\":1}";
        char* out_key = NULL;
        size_t out_key_len = 0;
        char* out_value = NULL;
        size_t out_value_len = 0;
        jsmntype_t out_value_type = JSMN_OBJECT;
        _cmd_parser_code_t p_res = _parse_first_kv(
                json_obj,
                strlen(json_obj),
                &out_key,
                &out_key_len,
                &out_value,
                &out_value_len,
                &out_value_type);

        REQUIRE( p_res == _CMD_PARSE_OK );

        // Check key.
        REQUIRE( out_key_len == 1 );
        char out_key_copy[out_key_len+1];
        strncpy(out_key_copy, out_key, out_key_len);
        out_key_copy[out_key_len] = '\0';
        REQUIRE( strcmp(out_key_copy, "a") == 0 );

        // Check value.
        REQUIRE( out_value_len == 1 );
        char out_value_copy[out_value_len+1];
        strncpy(out_value_copy, out_value, out_value_len);
        out_value_copy[out_value_len] = '\0';
        REQUIRE( strcmp(out_value_copy, "1") == 0 );

        // Check value type.
        REQUIRE( out_value_type == JSMN_PRIMITIVE );
    }

}

TEST_CASE( "_parse_action_object" ) {
    tio_action_t action;
    tio_handler_t handler;
    jkii_token_t tokens[16];
    jkii_resource_t resource = { tokens, 16 };
    handler._kii._json_resource = &resource;

    // TODO: Add other types tests.
    SECTION("Object action") {
        const char json_str[] = "[{\"setPower\":{\"power\":true}}]";
        const char alias[] = "myalias";
        _cmd_parser_code_t p_res = _parse_action(
                &handler,
                alias,
                strlen(alias),
                json_str,
                strlen(json_str),
                0,
                &action);

        REQUIRE( p_res == _CMD_PARSE_OK );

        // Check action name.
        char action_name[action.action_name_length + 1];
        strncpy(action_name, action.action_name, action.action_name_length);
        action_name[action.action_name_length] = '\0';

        REQUIRE ( strcmp(action_name, "setPower") == 0 );

        // Check action value.
        char action_value[action.action_value.opaque_value_length + 1];
        strncpy(action_value, action.action_value.param.opaque_value, action.action_value.opaque_value_length);
        action_value[action.action_value.opaque_value_length] = '\0';

        REQUIRE ( strcmp(action_value, "{\"power\":true}") == 0 );

        // Check action value type
        REQUIRE ( action.action_value.type == TIO_TYPE_OBJECT );

        // Check alias
        char alias_copy[action.alias_length+1];
        strncpy(alias_copy, action.alias, action.alias_length);
        alias_copy[action.alias_length] = '\0';
        REQUIRE ( strcmp(alias_copy, alias) == 0 );
    }

}
