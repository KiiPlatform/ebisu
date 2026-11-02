#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include <kii.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"

namespace {

// One task: the lock has nothing to keep apart. The system's monotonic clock
// reads now_ms, which the test moves.
struct OneTask {
    struct LockLink {
        ebisu_lock_t base;
    } lock;
    struct SystemLink {
        ebisu_system_t base;
        OneTask* owner;
    } system;
    uint64_t now_ms = 0;

    OneTask() {
        static const ebisu_lock_ops_t lock_ops = {
            [](ebisu_lock_t*) {},
            [](ebisu_lock_t*) {},
        };
        static const ebisu_system_ops_t system_ops = {
            NULL,
            NULL,
            [](ebisu_system_t* self) { return ((SystemLink*)self)->owner->now_ms; },
        };
        lock.base.ops = &lock_ops;
        system.base.ops = &system_ops;
        system.owner = this;
    }
    OneTask(const OneTask&) = delete;
    OneTask& operator=(const OneTask&) = delete;
};

// A token the server doesn't know, which it treats as it does an expired one.
const char UNKNOWN_TOKEN[] = "ltest-unknown-token";

struct State {
    std::string json;
    size_t pos;
};

size_t read_state(char* buffer, size_t size, void* userdata) {
    State* state = (State*)userdata;
    size_t n = state->json.copy(buffer, size, state->pos);
    state->pos += n;
    return n;
}

kii_author_t held(kii_session_t* session) {
    kii_author_t author;
    kii_session_get_author(session, &author);
    return author;
}

// Whether two tokens are the same. Compared here rather than in an assertion,
// which would print them, into logs that are public.
bool same_token(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

bool empty(const char* token) {
    return token[0] == '\0';
}

const char REFRESH_GRANT[] = "\"grant_type\":\"refresh_token\"";
const char PASSWORD_GRANT[] = "\"grant_type\":\"password\"";
// Onboarding through Thing-IF, as a request line has it.
const char ONBOARDING[] = "/onboardings HTTP/1.1";
// Anything asked of Kii Cloud's token endpoint, as a request line has it.
const char TOKEN_ENDPOINT[] = "/oauth2/token HTTP/1.1";

bool sent_has(const std::string& sent, const char* grant) {
    return sent.find(grant) != std::string::npos;
}

// Has the session hold its author with an access token the server refuses.
void spoil(kii_session_t* session) {
    kii_author_t author = held(session);
    strcpy(author.access_token, UNKNOWN_TOKEN);
    kii_session_set_author(session, &author);
}

}

TEST_CASE( "The session renews a token the server refuses" ) {
    // To Avoid 429 Too Many Requests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    char buff[4096];
    kii_t kii;
    ebisu::ltest::ssl::SSLData http_ssl_ctx;
    ebisu::ltest::ssl::SSLData mqtt_ssl_ctx;
    jkii_token_t tokens[256];
    jkii_resource_t resource = {tokens, 256};
    kiiltest::init(&kii, buff, sizeof(buff), &http_ssl_ctx, &mqtt_ssl_ctx, &resource);

    OneTask one_task;
    uint64_t& now_ms = one_task.now_ms;
    kii_session_t session;
    kii_session_init(&session);
    kii_session_set_lock(&session, &one_task.lock.base);
    kii_session_set_system(&session, &one_task.system.base);
    const std::string vid = "ltest_session_" + std::to_string(kiiltest::current_time());
    const char password[] = "1234";
    kii_session_set_password(&session, vid.c_str(), password);
    REQUIRE( kii_set_session(&kii, &session) == KII_ERR_OK );

    REQUIRE( kii_ti_onboard(&kii, vid.c_str(), password, NULL, NULL, NULL, NULL) == KII_ERR_OK );
    const kii_author_t onboarded = held(&session);
    REQUIRE( std::string(onboarded.access_token).length() > 0 );

    kii_bucket_t bucket;
    bucket.bucket_name = "ltest_session";
    bucket.scope = KII_SCOPE_THING;
    bucket.scope_id = onboarded.author_id;
    kii_object_id_t object_id;

    // What kii sends from here on, to tell how a renewal went about it. It
    // holds tokens and the password, so it is searched and never printed.
    std::string sent;
    ebisu::ltest::ObservedSocket observed(http_ssl_ctx.http());
    observed.on_sent = [&sent](const char* data, size_t length) { sent.append(data, length); };
    kii_set_http_socket(&kii, observed.get());

    SECTION("a token onboarding obtained is renewed by onboarding again") {
        // Kii's REST API: the request, whose body was built in the buffer, is
        // sent again with the new token.
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"v\"}", NULL, &object_id) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&kii) == 201 );
        kii_author_t renewed = held(&session);
        CHECK( std::string(renewed.author_id) == onboarded.author_id );
        CHECK_FALSE( same_token(renewed.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( same_token(renewed.access_token, onboarded.access_token) );
        // Onboarding again issues neither a refresh token nor a lifetime.
        CHECK( empty(renewed.refresh_token) );
        CHECK( session._renew_at_known == false );

        // Thing-IF's.
        spoil(&session);
        REQUIRE( kii_ti_put_firmware_version(&kii, "ltest-1.0") == KII_ERR_OK );
        CHECK_FALSE( same_token(held(&session).access_token, UNKNOWN_TOKEN) );
        kii_ti_firmware_version_t version;
        memset(&version, 0x00, sizeof(version));
        REQUIRE( kii_ti_get_firmware_version(&kii, &version) == KII_ERR_OK );
        CHECK( std::string(version.firmware_version) == "ltest-1.0" );

        // Both renewals onboarded again, through Thing-IF alone: nothing went
        // to the token endpoint.
        CHECK( sent_has(sent, ONBOARDING) );
        CHECK_FALSE( sent_has(sent, TOKEN_ENDPOINT) );
    }

    SECTION("a token a login obtained is refreshed, and logged in again once the refresh is refused") {
        // The login gives a refresh token: the application the tests run
        // against has refresh tokens enabled, which Kii applications do not by
        // default.
        REQUIRE( kii_auth_thing(&kii, vid.c_str(), password) == KII_ERR_OK );
        const kii_author_t logged_in = held(&session);
        REQUIRE_FALSE( empty(logged_in.refresh_token) );
        CHECK( session._renew_mode == KII_RENEW_BY_TOKEN_ENDPOINT );

        sent.clear();
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"w\"}", NULL, &object_id) == KII_ERR_OK );
        CHECK( sent_has(sent, REFRESH_GRANT) );
        CHECK_FALSE( sent_has(sent, PASSWORD_GRANT) );
        CHECK_FALSE( sent_has(sent, ONBOARDING) );
        kii_author_t refreshed = held(&session);
        CHECK_FALSE( same_token(refreshed.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( same_token(refreshed.access_token, logged_in.access_token) );
        // The refresh token is replaced too.
        CHECK_FALSE( empty(refreshed.refresh_token) );
        CHECK_FALSE( same_token(refreshed.refresh_token, logged_in.refresh_token) );

        // The refresh token that refresh spent is refused, and the session
        // logs in with the password instead.
        std::this_thread::sleep_for(std::chrono::seconds(2));
        kii_author_t spent = logged_in;
        strcpy(spent.access_token, UNKNOWN_TOKEN);
        kii_session_set_author(&session, &spent);
        sent.clear();
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"x\"}", NULL, &object_id) == KII_ERR_OK );
        CHECK( sent_has(sent, REFRESH_GRANT) );
        CHECK( sent_has(sent, PASSWORD_GRANT) );
        const kii_author_t relogged = held(&session);
        CHECK_FALSE( same_token(relogged.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( empty(relogged.refresh_token) );
        CHECK_FALSE( same_token(relogged.refresh_token, logged_in.refresh_token) );
    }

    SECTION("the lifetime a login gives is read, and the token renewed ahead of expiry") {
        // Onboarding gives no lifetime. A login does.
        CHECK( session._renew_at_known == false );
        REQUIRE( kii_auth_thing(&kii, vid.c_str(), password) == KII_ERR_OK );
        REQUIRE( session._renew_at_known == true );
        REQUIRE( session._renew_after_ms > 0 );
        const kii_author_t logged_in = held(&session);

        // Not due yet.
        now_ms = session._stored_at_ms + session._renew_after_ms - 1;
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"w\"}", NULL, &object_id) == KII_ERR_OK );
        CHECK( same_token(held(&session).access_token, logged_in.access_token) );

        // Due: renewed before the request, which the server never refused.
        now_ms += 1;
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"x\"}", NULL, &object_id) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&kii) == 201 );
        CHECK( kii._session_refused == false );
        const kii_author_t ahead = held(&session);
        CHECK( std::string(ahead.author_id) == onboarded.author_id );
        CHECK_FALSE( same_token(ahead.access_token, logged_in.access_token) );
        CHECK( session._renew_at_known == true );
        CHECK( session._stored_at_ms == now_ms );
    }

    SECTION("a refused password is not tried again on the next refused request") {
        // The application locks a thing out after two refused logins, as
        // initapp.sh sets it up, onboarding included: trying the wrong
        // password on each refused request would lock this one out.
        const char wrong_password[] = "5678";
        kii_session_set_password(&session, vid.c_str(), wrong_password);
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"v\"}", NULL, &object_id) == KII_ERR_RESP_STATUS );
        REQUIRE( kii_get_resp_status(&kii) == 403 );
        CHECK( sent_has(sent, ONBOARDING) );

        sent.clear();
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"w\"}", NULL, &object_id) == KII_ERR_RESP_STATUS );
        REQUIRE( kii_get_resp_status(&kii) == 403 );
        CHECK_FALSE( sent_has(sent, ONBOARDING) );
        CHECK_FALSE( sent_has(sent, TOKEN_ENDPOINT) );

        // Refused once only, the thing is not locked out: the right password
        // is still accepted.
        REQUIRE( kii_ti_onboard(&kii, vid.c_str(), password, NULL, NULL, NULL, NULL) == KII_ERR_OK );
        CHECK_FALSE( same_token(held(&session).access_token, UNKNOWN_TOKEN) );
    }

    SECTION("a streamed request is refused with 403 WRONG_TOKEN, and the token renewed") {
        State state = { "{\"power\":true}", 0 };
        spoil(&session);
        REQUIRE( kii_ti_put_state(&kii, read_state, &state, "application/json", NULL, NULL) == KII_ERR_RESP_STATUS );
        REQUIRE( kii_get_resp_status(&kii) == 403 );
        REQUIRE( kii._session_refused == true );
        CHECK_FALSE( same_token(held(&session).access_token, UNKNOWN_TOKEN) );

        state.pos = 0;
        REQUIRE( kii_ti_put_state(&kii, read_state, &state, "application/json", NULL, NULL) == KII_ERR_OK );
    }
}
