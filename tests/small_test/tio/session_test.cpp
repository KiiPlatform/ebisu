#include "catch.hpp"
#include "tio.h"
#include "tio_impl.h"
#include "command_parser.h"
#include "http_script.h"

#include <string>
#include <vector>
#include <string.h>

using khct::contains;
using khct::http_response;

namespace {

struct Platform {
    uint64_t now_ms;
    std::vector<kii_author_t> updates;
    Platform() : now_ms(0) {}
};

void cb_noop_lock(void*) {}

uint64_t cb_now(void* userdata) {
    return ((Platform*)userdata)->now_ms;
}

void cb_updated(const kii_author_t* author, void* userdata) {
    ((Platform*)userdata)->updates.push_back(*author);
}

void init_session(kii_session_t* session, Platform* platform) {
    kii_session_init(session);
    kii_session_set_cb_lock(session, cb_noop_lock, cb_noop_lock, NULL);
    kii_session_set_cb_monotonic_ms(session, cb_now, platform);
    kii_session_set_cb_author_updated(session, cb_updated, platform);
    kii_session_set_password(session, "vid", "pw");
    kii_author_t author;
    memset(&author, 0, sizeof(author));
    strcpy(author.author_id, "th.1");
    strcpy(author.access_token, "at1");
    kii_session_set_author(session, &author);
}

const std::string REFUSED = http_response(403, "{\"errorCode\":\"WRONG_TOKEN\"}");
// tio renews by onboarding again, a single request authorized with the app's
// ID.
const std::string ONBOARDED = http_response(200, "{\"accessToken\":\"at2\",\"thingID\":\"th.1\"}");
const char ONBOARDING_LINE[] = "POST https://api.kii.com/thing-if/apps/app/onboardings HTTP/1.1";
// "app:dummy"
const char APP_CREDENTIALS[] = "Authorization: Basic YXBwOmR1bW15\r\n";
const std::string NO_CONTENT = http_response(204, "");

struct State {
    std::string json;
    size_t pos;
    int size_calls;
};

size_t state_size(void* userdata) {
    State* state = (State*)userdata;
    ++state->size_calls;
    state->pos = 0;
    return state->json.size();
}

size_t read_state(char* buffer, size_t size, void* userdata) {
    State* state = (State*)userdata;
    size_t n = state->json.size() - state->pos;
    if (n > size) {
        n = size;
    }
    memcpy(buffer, state->json.data() + state->pos, n);
    state->pos += n;
    return n;
}

kii_task_code_t cb_task_not_started(const char*, KII_TASK_ENTRY, void*, void*) {
    return KII_TASKC_OK;
}

}

TEST_CASE( "The updater uploads again when its token was renewed" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);

    tio_updater_t updater;
    tio_updater_init(&updater);
    char buff[2048];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    tio_updater_set_app(&updater, "app", "api.kii.com");
    tio_updater_set_buff(&updater, buff, sizeof(buff));
    tio_updater_set_json_parser_resource(&updater, &resource);
    tio_updater_set_cb_delay_ms(&updater, khct::cb::cb_delay_ms, NULL);
    khct::HttpScript http;
    http.attach(&updater._kii);
    REQUIRE( tio_updater_set_session(&updater, &session) == TIO_ERR_OK );
    // tio's tokens come from onboarding, and are renewed by onboarding again.
    REQUIRE( session._renew_mode == KII_RENEW_BY_ONBOARDING );

    State state = { "{\"alias\":{\"power\":true}}", 0, 0 };
    updater._cb_state_size = state_size;
    updater._cb_state_size_data = &state;
    updater._state_reader = read_state;
    updater._state_reader_data = &state;

    SECTION("renewed") {
        http.responses = { REFUSED, ONBOARDED, NO_CONTENT };
        REQUIRE( _tio_updater_upload(&updater) == KII_ERR_OK );

        // The reader was started over for the second upload.
        REQUIRE( state.size_calls == 2 );
        REQUIRE( http.requests.size() == 3 );
        REQUIRE( contains(http.requests[0], "Authorization: Bearer at1\r\n") );
        REQUIRE( contains(http.requests[0], state.json) );
        REQUIRE( contains(http.requests[1], ONBOARDING_LINE) );
        REQUIRE( contains(http.requests[1], APP_CREDENTIALS) );
        REQUIRE( contains(http.requests[2], "Authorization: Bearer at2\r\n") );
        REQUIRE( contains(http.requests[2], state.json) );
        REQUIRE( platform.updates.size() == 1 );
    }

    SECTION("not renewed: no point uploading again") {
        http.responses = { REFUSED, http_response(400, "{\"errorCode\":\"WRONG_PASSWORD\"}") };
        REQUIRE( _tio_updater_upload(&updater) == KII_ERR_RESP_STATUS );
        REQUIRE( state.size_calls == 1 );
        REQUIRE( http.requests.size() == 2 );
    }
}

namespace {

tio_bool_t cb_action(
        tio_action_t* action,
        tio_action_err_t* err,
        tio_action_result_data_t* data,
        void* userdata) {
    err->err_message[0] = '\0';
    data->json[0] = '\0';
    ++(*(int*)userdata);
    return KII_TRUE;
}

}

TEST_CASE( "A command's results are sent again after the token is renewed" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);

    tio_handler_t handler;
    tio_handler_init(&handler);
    char buff[2048];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    tio_handler_set_app(&handler, "app", "api.kii.com");
    tio_handler_set_http_buff(&handler, buff, sizeof(buff));
    tio_handler_set_json_parser_resource(&handler, &resource);
    tio_handler_set_cb_delay_ms(&handler, khct::cb::cb_delay_ms, NULL);
    khct::HttpScript http;
    http.attach(&handler._kii);
    REQUIRE( tio_handler_set_session(&handler, &session) == TIO_ERR_OK );

    http.responses = { REFUSED, ONBOARDED, NO_CONTENT };
    int called = 0;
    const std::string command =
        "{\"commandID\":\"cmd1\",\"actions\":[{\"alias1\":[{\"turnPower\":true}]}]}";
    REQUIRE( _handle_command(&handler, command.c_str(), command.length(), cb_action, &called) == TIO_ERR_OK );

    // The action ran once; only its results were sent twice.
    REQUIRE( called == 1 );
    REQUIRE( http.requests.size() == 3 );
    const char result[] = "{\"turnPower\":{\"succeeded\":true}}";
    REQUIRE( contains(http.requests[0], "Authorization: Bearer at1\r\n") );
    REQUIRE( contains(http.requests[0], result) );
    REQUIRE( contains(http.requests[1], ONBOARDING_LINE) );
    REQUIRE( contains(http.requests[1], APP_CREDENTIALS) );
    REQUIRE( contains(http.requests[2],
                "PUT https://api.kii.com/thing-if/apps/app/targets/thing:th.1/commands/cmd1/action-results HTTP/1.1") );
    REQUIRE( contains(http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(http.requests[2], result) );
    REQUIRE( kii_get_resp_status(&handler._kii) == 204 );
}

TEST_CASE( "With a session, start takes the session's author" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);

    tio_handler_t handler;
    tio_handler_init(&handler);
    tio_handler_set_cb_task_create(&handler, cb_task_not_started, NULL);
    REQUIRE( tio_handler_set_session(&handler, &session) == TIO_ERR_OK );

    // Changed since it was attached, as onboarding through the other task
    // would.
    kii_author_t renewed;
    memset(&renewed, 0, sizeof(renewed));
    strcpy(renewed.author_id, "th.1");
    strcpy(renewed.access_token, "at2");
    kii_session_set_author(&session, &renewed);

    REQUIRE( tio_handler_start(&handler, NULL, cb_action, NULL) == TIO_ERR_OK );
    REQUIRE( std::string(handler._kii._author.access_token) == "at2" );

    tio_updater_t updater;
    tio_updater_init(&updater);
    tio_updater_set_cb_task_create(&updater, cb_task_not_started, NULL);
    REQUIRE( tio_updater_set_session(&updater, &session) == TIO_ERR_OK );
    REQUIRE( tio_updater_start(&updater, NULL, state_size, NULL, read_state, NULL) == TIO_ERR_OK );
    REQUIRE( std::string(updater._kii._author.access_token) == "at2" );
}

TEST_CASE( "Without a session, start needs an author" ) {
    tio_handler_t handler;
    tio_handler_init(&handler);
    tio_handler_set_cb_task_create(&handler, cb_task_not_started, NULL);
    REQUIRE( tio_handler_start(&handler, NULL, cb_action, NULL) == TIO_ERR_FAIL );

    tio_updater_t updater;
    tio_updater_init(&updater);
    tio_updater_set_cb_task_create(&updater, cb_task_not_started, NULL);
    REQUIRE( tio_updater_start(&updater, NULL, state_size, NULL, read_state, NULL) == TIO_ERR_FAIL );
}

TEST_CASE( "tio refuses a session without the lock or the clock" ) {
    kii_session_t session;
    kii_session_init(&session);
    tio_handler_t handler;
    tio_handler_init(&handler);
    REQUIRE( tio_handler_set_session(&handler, &session) == TIO_ERR_FAIL );
    tio_updater_t updater;
    tio_updater_init(&updater);
    REQUIRE( tio_updater_set_session(&updater, &session) == TIO_ERR_FAIL );
}
