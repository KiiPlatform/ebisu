#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "tio.h"
#include "command_parser.h"
#include "jkii.h"
typedef struct expected_parsed_actions {
    const char* command_id;
    tio_action_t *expected_actions;
    int expected_actions_length;
    int matched_count;
} expected_parsed_actions;

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

void cb_parsed_action(char* command_id, tio_action_t* action, void* expected_actions) {
    expected_parsed_actions* expected = (expected_parsed_actions *) expected_actions;
    REQUIRE( strcmp(command_id, expected->command_id) == 0 );
    int i = 0;

    char alias_cp[action->alias_length + 1];
    char action_name_cp[action->action_name_length + 1];
    strncpy(alias_cp, action->alias, action->alias_length);
    alias_cp[action->alias_length] = '\0';
    strncpy(action_name_cp, action->action_name, action->action_name_length);
    action_name_cp[action->action_name_length] = '\0';

    for(int i=0; i < expected -> expected_actions_length; i++) {
        tio_action_t expected_action = expected->expected_actions[i];
        if (strcmp(alias_cp, expected_action.alias) == 0 &&
            strcmp(action_name_cp, expected_action.action_name) == 0) {
                REQUIRE(action->action_name_length == expected_action.action_name_length);
                REQUIRE(action->alias_length == expected_action.alias_length);
                REQUIRE(action->action_value.type == expected_action.action_value.type);
                if (action->action_name_length == expected_action.action_name_length &&
                    action->alias_length == expected_action.alias_length &&
                    action->action_value.type == expected_action.action_value.type)
                {
                    (expected->matched_count)++;
                }

            }
    }
}

TEST_CASE( "_parse_command" ) {
    tio_action_t action;
    tio_handler_t handler;
    jkii_token_t tokens[25];
    jkii_resource_t resource = { tokens, 25 };
    handler._kii._json_resource = &resource;

    SECTION("has 1 alias, 1 action") {
        const char command[] = "{\"commandID\":\"eed568b4-409c-11e9-b3ec-22000aad0899\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
        expected_parsed_actions expected;
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, 1}};
        const char command_id[] = "eed568b4-409c-11e9-b3ec-22000aad0899";
        expected.command_id = command_id;
        tio_action_t expected_actions[1] = {action1};
        expected.expected_actions = expected_actions;
        expected.expected_actions_length = 1;
        expected.matched_count = 0;
        tio_code_t res = _parse_command(&handler, command, strlen(command), cb_parsed_action, (void *)&expected);
        REQUIRE( TIO_ERR_OK == res);
        REQUIRE( 1 == expected.matched_count);
    }

    SECTION("has 1 alias, 2 actions") {
        const char command[] = "{\"commandID\":\"eed568b4-409c-11e9-b3ec-22000aad0899\",\"actions\":[{\"alias1\":[{\"turnPower\":true},{\"setTemp\":23}]}]}";
        expected_parsed_actions expected;
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, 1}};
        tio_action_t action2 = {"alias1", 6, "setTemp", 7, {TIO_TYPE_INTEGER, 23}};

        const char command_id[] = "eed568b4-409c-11e9-b3ec-22000aad0899";
        expected.command_id = command_id;
        tio_action_t expected_actions[2] = {action1, action2};
        expected.expected_actions = expected_actions;
        expected.expected_actions_length = 2;
        expected.matched_count = 0;
        tio_code_t res = _parse_command(&handler, command, strlen(command), cb_parsed_action, (void *)&expected);
        REQUIRE( TIO_ERR_OK == res);
        REQUIRE( 2 == expected.matched_count);
    }

    SECTION("has 2 alias, 2 actions") {
        const char command[] = "{\"commandID\":\"eed568b4-409c-11e9-b3ec-22000aad0899\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]},{\"alias2\":[{\"setTemp\":23}]}]}";
        expected_parsed_actions expected;
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, 1}};
        tio_action_t action2 = {"alias2", 6, "setTemp", 7, {TIO_TYPE_INTEGER, 23}};

        const char command_id[] = "eed568b4-409c-11e9-b3ec-22000aad0899";
        expected.command_id = command_id;
        tio_action_t expected_actions[2] = {action1, action2};
        expected.expected_actions = expected_actions;
        expected.expected_actions_length = 2;
        expected.matched_count = 0;
        tio_code_t res = _parse_command(&handler, command, strlen(command), cb_parsed_action, (void *)&expected);
        REQUIRE( TIO_ERR_OK == res);
        REQUIRE( 2 == expected.matched_count);
    }
}
