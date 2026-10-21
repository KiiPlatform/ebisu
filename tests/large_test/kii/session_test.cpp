#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include <kii.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"

namespace {

// One task: the lock has nothing to keep apart.
void cb_lock(void*) {}
void cb_unlock(void*) {}

uint64_t cb_now(void* userdata) {
    return *(uint64_t*)userdata;
}

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

    uint64_t now_ms = 0;
    kii_session_t session;
    kii_session_init(&session);
    kii_session_set_cb_lock(&session, cb_lock, cb_unlock, NULL);
    kii_session_set_cb_monotonic_ms(&session, cb_now, &now_ms);
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

    SECTION("a request whose body was built in the buffer is sent again") {
        // Kii's REST API.
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"v\"}", NULL, &object_id) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&kii) == 201 );
        kii_author_t renewed = held(&session);
        CHECK( std::string(renewed.author_id) == onboarded.author_id );
        CHECK_FALSE( same_token(renewed.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( same_token(renewed.access_token, onboarded.access_token) );

        // Thing-IF's.
        spoil(&session);
        REQUIRE( kii_ti_put_firmware_version(&kii, "ltest-1.0") == KII_ERR_OK );
        CHECK_FALSE( same_token(held(&session).access_token, UNKNOWN_TOKEN) );
        kii_ti_firmware_version_t version;
        memset(&version, 0x00, sizeof(version));
        REQUIRE( kii_ti_get_firmware_version(&kii, &version) == KII_ERR_OK );
        CHECK( std::string(version.firmware_version) == "ltest-1.0" );

        // With a refresh token, when the application issues them, the renewal
        // refreshes rather than logging in.
        renewed = held(&session);
        if (renewed.refresh_token[0] == '\0') {
            WARN("The application issues no refresh tokens: only the password was used");
        } else {
            spoil(&session);
            REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"w\"}", NULL, &object_id) == KII_ERR_OK );
            kii_author_t refreshed = held(&session);
            CHECK_FALSE( same_token(refreshed.access_token, UNKNOWN_TOKEN) );
            CHECK_FALSE( same_token(refreshed.access_token, renewed.access_token) );
        }
    }

    SECTION("the lifetime the server gives is read, and the token renewed ahead of expiry") {
        // Onboarding gives no lifetime. The login that renews its token does.
        CHECK( session._renew_at_known == KII_FALSE );
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"v\"}", NULL, &object_id) == KII_ERR_OK );
        REQUIRE( session._renew_at_known == KII_TRUE );
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
        CHECK( kii._session_refused == KII_FALSE );
        const kii_author_t ahead = held(&session);
        CHECK( std::string(ahead.author_id) == onboarded.author_id );
        CHECK_FALSE( same_token(ahead.access_token, logged_in.access_token) );
        CHECK( session._renew_at_known == KII_TRUE );
        CHECK( session._stored_at_ms == now_ms );
    }

    SECTION("a streamed request is refused with 403 WRONG_TOKEN, and the token renewed") {
        State state = { "{\"power\":true}", 0 };
        spoil(&session);
        REQUIRE( kii_ti_put_state(&kii, read_state, &state, "application/json", NULL, NULL) == KII_ERR_RESP_STATUS );
        REQUIRE( kii_get_resp_status(&kii) == 403 );
        REQUIRE( kii._session_refused == KII_TRUE );
        CHECK_FALSE( same_token(held(&session).access_token, UNKNOWN_TOKEN) );

        state.pos = 0;
        REQUIRE( kii_ti_put_state(&kii, read_state, &state, "application/json", NULL, NULL) == KII_ERR_OK );
    }
}
