#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "tio2.h"
#include "command_parser.h"
#include "kii_json.h"

TEST_CASE( "_get_object_in_array" ) {
    const char json_arr[] = "[{\"a\":1},{\"b\":2}]";

    kii_json_token_t tokens[16];
    kii_json_resource_t resource = {tokens, 16};

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
    // TODO: Add tests.
    REQUIRE( true );
}

TEST_CASE( "_parse_action_object" ) {
    // TODO: Add tests.
    REQUIRE( true );
}