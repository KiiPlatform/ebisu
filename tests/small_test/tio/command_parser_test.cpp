#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "tio.h"
#include "command_parser.h"
#include "jkii.h"
#include "test_callbacks.h"
#include <memory>
#include <string>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <type_traits>
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

// Leaves the stack below the caller's frame filled with JSMN_STRING, so a
// callee reading a jsmntok_t that jsmn never wrote sees a string token.
static void __attribute__((noinline)) fill_stack_with_jsmn_string()
{
    volatile int fill[4096];
    for (size_t i = 0; i < sizeof(fill) / sizeof(fill[0]); ++i) {
        fill[i] = JSMN_STRING;
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

    // jsmn fills only as many tokens as the object has, so an object without
    // a key/value pair must be rejected before the key and value tokens are
    // read. The stack is filled first so that unfilled tokens look like a
    // string key rather than whatever happened to be left there.
    SECTION("Empty object") {
        const char json_obj[] = "{}";
        char* out_key = NULL;
        size_t out_key_len = 0;
        char* out_value = NULL;
        size_t out_value_len = 0;
        jsmntype_t out_value_type = JSMN_OBJECT;
        fill_stack_with_jsmn_string();
        _cmd_parser_code_t p_res = _parse_first_kv(
                json_obj,
                strlen(json_obj),
                &out_key,
                &out_key_len,
                &out_value,
                &out_value_len,
                &out_value_type);

        REQUIRE( p_res == _CMD_PARSE_FAIL );
    }

    SECTION("Key without value") {
        const char json_obj[] = "{\"a\"}";
        char* out_key = NULL;
        size_t out_key_len = 0;
        char* out_value = NULL;
        size_t out_value_len = 0;
        jsmntype_t out_value_type = JSMN_OBJECT;
        fill_stack_with_jsmn_string();
        _cmd_parser_code_t p_res = _parse_first_kv(
                json_obj,
                strlen(json_obj),
                &out_key,
                &out_key_len,
                &out_value,
                &out_value_len,
                &out_value_type);

        REQUIRE( p_res == _CMD_PARSE_FAIL );
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

// An integer action value is an int64_t, whatever the width of long. It was a
// long, so where long is 32 bits, as on the ARM targets, no value past the int
// range reached the action handler.
static_assert(std::is_same<
        decltype(((tio_action_value_t*)0)->param.long_value), int64_t>::value,
        "long_value is an int64_t");

TEST_CASE( "_parse_action integer values" ) {
    tio_handler_t handler;
    jkii_token_t tokens[16];
    jkii_resource_t resource = { tokens, 16 };
    handler._kii._json_resource = &resource;

    const struct {
        const char* text;
        int64_t value;
    } cases[] = {
        { "23", 23 },
        { "-23", -23 },
        { "2147483647", INT32_MAX },
        { "-2147483648", INT32_MIN },
        { "2147483648", (int64_t)INT32_MAX + 1 },
        { "-2147483649", (int64_t)INT32_MIN - 1 },
        { "4294967296", INT64_C(4294967296) },
        { "9223372036854775807", INT64_MAX },
        { "-9223372036854775808", INT64_MIN }
    };
    const char alias[] = "myalias";

    for (const auto& c : cases) {
        INFO("value " << c.text);
        std::string actions =
            std::string("[{\"setValue\":") + c.text + "}]";
        tio_action_t action;
        memset(&action, 0, sizeof(action));

        _cmd_parser_code_t p_res = _parse_action(
                &handler,
                alias,
                strlen(alias),
                actions.c_str(),
                actions.length(),
                0,
                &action);
        CHECK( p_res == _CMD_PARSE_OK );
        CHECK( action.action_value.type == TIO_TYPE_INTEGER );
        CHECK( action.action_value.param.long_value == c.value );
    }
}

void cb_parsed_action(char* command_id, tio_action_t* action, void* expected_actions) {
    expected_parsed_actions* expected = (expected_parsed_actions *) expected_actions;
    REQUIRE( strcmp(command_id, expected->command_id) == 0 );

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

static void cb_parsed_action_count(char* command_id, tio_action_t* action, void* called) {
    ++(*(int*)called);
}

TEST_CASE( "_parse_command" ) {
    tio_handler_t handler;
    jkii_token_t tokens[25];
    jkii_resource_t resource = { tokens, 25 };
    handler._kii._json_resource = &resource;

    SECTION("has 1 alias, 1 action") {
        const char command[] = "{\"commandID\":\"eed568b4-409c-11e9-b3ec-22000aad0899\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
        expected_parsed_actions expected;
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, {1}}};
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
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, {1}}};
        tio_action_t action2 = {"alias1", 6, "setTemp", 7, {TIO_TYPE_INTEGER, {23}}};

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
        tio_action_t action1 = {"alias1", 6, "turnPower", 9, {TIO_TYPE_BOOLEAN, {1}}};
        tio_action_t action2 = {"alias2", 6, "setTemp", 7, {TIO_TYPE_INTEGER, {23}}};

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

    // The alias has to map to an array of actions. "" left nothing for
    // jkii_parse to parse, which asserted and aborted the process.
    SECTION("alias mapped to an empty string") {
        const char command[] = "{\"commandID\":\"c\",\"actions\":[{\"alias\":\"\"}]}";
        int called = 0;
        tio_code_t res = _parse_command(&handler, command, strlen(command), cb_parsed_action_count, &called);
        REQUIRE( TIO_ERR_PARSE_JSON == res);
        REQUIRE( 0 == called);
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

// kii_api_call_run reports only transport errors, so a refused action-result
// request used to come back as TIO_ERR_OK, and the results were lost without
// the error callback being told.
TEST_CASE( "_handle_command response status" ) {
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

    // Each section sets the response before calling _handle_command.
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [](void*, const char*, unsigned int) { return KHC_SOCK_OK; };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    size_t resp_pos = 0;
    std::string resp;
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

    int called = 0;
    std::string command =
        "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";

    SECTION("2xx is success") {
        resp = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_OK );
        REQUIRE( called == 1 );
    }

    SECTION("500 is reported") {
        const std::string body = "{\"errorCode\":\"INTERNAL_SERVER_ERROR\"}";
        resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + body;
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_RESP_STATUS );
        REQUIRE( called == 1 );
    }

    SECTION("401 is reported") {
        const std::string body = "{\"errorCode\":\"WRONG_TOKEN\"}";
        resp = "HTTP/1.1 401 Unauthorized\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n\r\n" + body;
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_RESP_STATUS );
        REQUIRE( called == 1 );
    }
}

// Commands arrive over MQTT, so an alias or action object without a key/value
// pair has to be turned into a parse error: no action called, nothing sent, and
// the result request _handle_command already started released.
TEST_CASE( "_handle_command empty objects" ) {
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

    // No request may be performed, so connecting is recorded and fails.
    bool connected = false;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&connected](void*, const char*, unsigned int) {
        connected = true;
        return KHC_SOCK_FAIL;
    };
    kii_set_cb_http_sock_connect(&handler._kii, khct::cb::mock_connect, &http_ctx);

    SECTION("an empty alias object is rejected") {
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_PARSE_JSON );
        REQUIRE( called == 0 );
        REQUIRE( !connected );
        REQUIRE( handler._kii._req_headers == nullptr );
    }

    SECTION("an empty action object is rejected") {
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_PARSE_JSON );
        REQUIRE( called == 0 );
        REQUIRE( !connected );
        REQUIRE( handler._kii._req_headers == nullptr );
    }
}

// The result body opens with {"actionResults":[ before any action runs. When
// that did not fit the HTTP buffer the error was dropped, so _handle_command
// carried on with the request headers already freed: it ran the actions, and
// sent a body missing its opening if the rest happened to fit.
TEST_CASE( "_handle_command result body opening does not fit" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);

    // Too short for {"actionResults":[ and its terminator, long enough for
    // {"alias1":[ -- so the appends that follow do not catch it either.
    char kii_buff[16];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_set_buff(&handler._kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&handler._kii, &resource);
    kii_set_app_id(&handler._kii, "appid");
    kii_set_site(&handler._kii, "api.kii.com");
    strcpy(handler._kii._author.author_id, "thingid");

    // Nothing should reach the socket; it answers anyway, so that sending is
    // seen as such rather than as a connection error.
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

    SECTION("the action is not run") {
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_TOO_LARGE_DATA );
        REQUIRE( called == 0 );
        REQUIRE( sent.empty() );
    }

    SECTION("a command without actions is not sent") {
        int called = 0;
        std::string command = "{\"commandID\":\"cmd1\",\"actions\":[]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_TOO_LARGE_DATA );
        REQUIRE( sent.empty() );
    }
}

// A command with nothing to parse where JSON is expected: an MQTT message with
// an empty body, or an alias mapped to "" rather than to an array of actions.
// Either left jkii_parse a zero length, which it asserted against. An alias
// mapped to whitespace was parsed against the tokens left by the previous parse
// instead, in every build. All of them have to be rejected like any other
// malformed command.
TEST_CASE( "_handle_command nothing to parse" ) {
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

    // The result request is begun before the actions are parsed. A rejected
    // command must abandon it rather than send it, so connecting fails the
    // test.
    int connects = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&connects](void*, const char*, unsigned int) {
        ++connects;
        return KHC_SOCK_FAIL;
    };
    kii_set_cb_http_sock_connect(&handler._kii, khct::cb::mock_connect, &http_ctx);

    std::string command;

    SECTION("empty payload") {
        command = "";
    }

    SECTION("alias mapped to an empty string") {
        command = "{\"commandID\":\"c\",\"actions\":[{\"alias\":\"\"}]}";
    }

    SECTION("alias mapped to whitespace") {
        command = "{\"commandID\":\"c\",\"actions\":[{\"alias\":\"  \"}]}";
    }

    // Copied to an allocation of exactly its length, without a terminator, as
    // a payload arrives.
    std::unique_ptr<char[]> payload(new char[command.size()]);
    memcpy(payload.get(), command.data(), command.size());

    int called = 0;
    tio_code_t res = _handle_command(
            &handler, payload.get(), command.size(), cb_action_ok, &called);

    REQUIRE( res == TIO_ERR_PARSE_JSON );
    REQUIRE( called == 0 );
    REQUIRE( connects == 0 );
    // Released, as on the other paths that abandon the request.
    REQUIRE( handler._kii._req_headers == nullptr );
}

// jsmn is built non-strict, so a primitive action value such as abc or - is
// tokenized even though it is not JSON. jkii cannot read it, nor a number out
// of range such as 1e999, and the action used to reach the callback with its
// value type and value never set.
static void cb_parsed_action_copy(char* command_id, tio_action_t* action, void* userdata) {
    std::vector<tio_action_t>* actions = (std::vector<tio_action_t>*)userdata;
    actions->push_back(*action);
}

TEST_CASE( "_parse_command primitive values" ) {
    tio_handler_t handler;
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    handler._kii._json_resource = &resource;

    std::vector<tio_action_t> actions;

    SECTION("JSON primitives are read with their type") {
        const char command[] =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":["
            "{\"a\":true},{\"b\":false},{\"c\":null},"
            "{\"d\":23},{\"e\":-5},{\"f\":1.5},{\"g\":-2e3}]}]}";
        tio_code_t res = _parse_command(
                &handler, command, strlen(command), cb_parsed_action_copy, &actions);

        REQUIRE( res == TIO_ERR_OK );
        REQUIRE( actions.size() == 7 );
        REQUIRE( actions[0].action_value.type == TIO_TYPE_BOOLEAN );
        REQUIRE( actions[0].action_value.param.bool_value == KII_TRUE );
        REQUIRE( actions[1].action_value.type == TIO_TYPE_BOOLEAN );
        REQUIRE( actions[1].action_value.param.bool_value == KII_FALSE );
        REQUIRE( actions[2].action_value.type == TIO_TYPE_NULL );
        REQUIRE( actions[3].action_value.type == TIO_TYPE_INTEGER );
        REQUIRE( actions[3].action_value.param.long_value == 23 );
        REQUIRE( actions[4].action_value.type == TIO_TYPE_INTEGER );
        REQUIRE( actions[4].action_value.param.long_value == -5 );
        REQUIRE( actions[5].action_value.type == TIO_TYPE_DOUBLE );
        REQUIRE( actions[5].action_value.param.double_value == 1.5 );
        REQUIRE( actions[6].action_value.type == TIO_TYPE_DOUBLE );
        REQUIRE( actions[6].action_value.param.double_value == -2000.0 );
    }

    SECTION("primitives jkii cannot read are rejected") {
        const char* values[] = { "abc", "-", "tru", "0e", "1e999" };
        for (const char* value : values) {
            CAPTURE( value );
            actions.clear();
            std::string command =
                "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"setX\":" +
                std::string(value) + "}]}]}";
            tio_code_t res = _parse_command(
                    &handler, command.c_str(), command.length(),
                    cb_parsed_action_copy, &actions);

            REQUIRE( res == TIO_ERR_PARSE_JSON );
            REQUIRE( actions.empty() );
        }
    }
}

// The same through _handle_command, which has started the result request by
// the time it reads an action: no action called, nothing sent, and the
// request released.
TEST_CASE( "_handle_command invalid primitive values" ) {
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

    // No request may be performed, so connecting is recorded and fails.
    bool connected = false;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&connected](void*, const char*, unsigned int) {
        connected = true;
        return KHC_SOCK_FAIL;
    };
    kii_set_cb_http_sock_connect(&handler._kii, khct::cb::mock_connect, &http_ctx);

    const char* values[] = { "abc", "-", "tru", "0e", "1e999" };
    for (const char* value : values) {
        CAPTURE( value );
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"setX\":" +
            std::string(value) + "}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_PARSE_JSON );
        REQUIRE( called == 0 );
        REQUIRE( !connected );
        REQUIRE( handler._kii._req_headers == nullptr );
    }
}

// jsmn is built non-strict, so a command object with an unquoted key parses,
// with the key as a primitive token. Looking /commandID or /actions up in it
// has to be a parse error rather than an assertion.
TEST_CASE( "_handle_command unquoted key" ) {
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

    // No request may be performed, so connecting is recorded and fails.
    bool connected = false;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&connected](void*, const char*, unsigned int) {
        connected = true;
        return KHC_SOCK_FAIL;
    };
    kii_set_cb_http_sock_connect(&handler._kii, khct::cb::mock_connect, &http_ctx);

    SECTION("an unquoted commandID key is rejected") {
        int called = 0;
        std::string command =
            "{commandID:\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_PARSE_JSON );
        REQUIRE( called == 0 );
        REQUIRE( !connected );
    }

    SECTION("an unquoted actions key is rejected") {
        int called = 0;
        std::string command =
            "{\"commandID\":\"cmd1\",actions:[{\"alias1\":[{\"turnPower\":true}]}]}";
        tio_code_t res = _handle_command(
                &handler, command.c_str(), command.length(), cb_action_ok, &called);

        REQUIRE( res == TIO_ERR_PARSE_JSON );
        REQUIRE( called == 0 );
        REQUIRE( !connected );
    }
}

TEST_CASE( "_parse_command unquoted key" ) {
    tio_handler_t handler;
    jkii_token_t tokens[25];
    jkii_resource_t resource = { tokens, 25 };
    handler._kii._json_resource = &resource;

    int called = 0;
    const char command[] = "{commandID:\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
    tio_code_t res = _parse_command(&handler, command, strlen(command), cb_parsed_action_count, &called);

    REQUIRE( res == TIO_ERR_PARSE_JSON );
    REQUIRE( called == 0 );
}
