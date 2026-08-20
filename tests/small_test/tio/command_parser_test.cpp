#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "tio.h"
#include "command_parser.h"
#include "jkii.h"
#include "test_callbacks.h"
#include <string>
#include <string.h>
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
        // strncpy writes no terminator when the source is at least as long as
        // the count, and obj_str is not terminated at obj_str_len, so strcmp
        // below read the uninitialised last byte of this array.
        obj_str_copy[obj_str_len] = '\0';
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
        obj_str_copy[obj_str_len] = '\0';
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

// _handle_command builds the action-result request body. The action name in it
// is a slice of the parsed command, so its length comes from the network; it
// used to be copied onto the stack into an array sized by that length, letting
// whoever publishes a command choose how much stack to consume.
static tio_bool_t cb_action_ok(
        tio_action_t* action,
        tio_action_err_t* err,
        tio_action_result_data_t* data,
        void* userdata) {
    err->err_message[0] = '\0';
    data->json[0] = '\0';
    ++(*(int*)userdata);
    return KII_TRUE;
}

TEST_CASE( "_handle_command action name length" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);

    char kii_buff[2048];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_set_buff(&handler._kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&handler._kii, &resource);
    kii_set_app_id(&handler._kii, "appid");
    kii_set_site(&handler._kii, "api.kii.com");
    strcpy(handler._kii._author.author_id, "thingid");

    // The request is actually performed at the end of _handle_command, so the
    // socket has to answer. Everything sent is captured, which is how the body
    // the action name is written into gets inspected.
    std::string sent;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [](void*, const char*, unsigned int) { return KHC_SOCK_OK; };
    http_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    size_t resp_pos = 0;
    const std::string resp = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";
    http_ctx.on_recv = [&resp_pos, &resp](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&handler._kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&handler._kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&handler._kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&handler._kii, khct::cb::mock_close, &http_ctx);
    char resp_hdr_buff[512];
    khc_set_resp_header_buff(&handler._kii._khc, resp_hdr_buff, sizeof(resp_hdr_buff));

    SECTION("an ordinary action name is written to the result body") {
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_OK );
        REQUIRE( called == 1 );
        // Written with a precision rather than copied via the stack; the body
        // must be unchanged by that.
        REQUIRE( sent.find("{\"turnPower\":{\"succeeded\":true}}") != std::string::npos );
    }

    // The result body is built in a 256 byte work buffer, so a name at least
    // that long is rejected rather than sized onto the stack.
    SECTION("an action name longer than the work buffer is rejected") {
        int called = 0;
        std::string long_name(300, 'a');
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"" +
            long_name + "\":true}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_TOO_LARGE_DATA );
    }
}
