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

bool empty(const char* token) {
    return token[0] == '\0';
}

// What kii sent, recorded to tell which grant a renewal asked for. It holds
// tokens and the password, so it is searched and never printed.
struct Sent {
    KHC_CB_SOCK_SEND send;
    void* ctx;
    std::string data;
};

khc_sock_code_t record_send(void* userdata, const char* buffer, size_t length,
        size_t* out_sent_length) {
    Sent* sent = (Sent*)userdata;
    khc_sock_code_t res = sent->send(sent->ctx, buffer, length, out_sent_length);
    if (res == KHC_SOCK_OK) {
        sent->data.append(buffer, *out_sent_length);
    }
    return res;
}

const char REFRESH_GRANT[] = "\"grant_type\":\"refresh_token\"";
const char PASSWORD_GRANT[] = "\"grant_type\":\"password\"";

bool sent_has(const Sent& sent, const char* grant) {
    return sent.data.find(grant) != std::string::npos;
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

        // The login gave a refresh token: the application the tests run
        // against has refresh tokens enabled, which Kii applications do not by
        // default. So the next renewal refreshes rather than logging in.
        renewed = held(&session);
        REQUIRE_FALSE( empty(renewed.refresh_token) );
        Sent sent = {kii._khc._cb_sock_send, kii._khc._sock_ctx_send, std::string()};
        kii_set_cb_http_sock_send(&kii, record_send, &sent);
        spoil(&session);
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"w\"}", NULL, &object_id) == KII_ERR_OK );
        CHECK( sent_has(sent, REFRESH_GRANT) );
        CHECK_FALSE( sent_has(sent, PASSWORD_GRANT) );
        kii_author_t refreshed = held(&session);
        CHECK_FALSE( same_token(refreshed.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( same_token(refreshed.access_token, renewed.access_token) );
        // The refresh token is replaced too.
        CHECK_FALSE( empty(refreshed.refresh_token) );
        CHECK_FALSE( same_token(refreshed.refresh_token, renewed.refresh_token) );

        // The refresh token that refresh spent is refused, and the session
        // logs in with the password instead.
        std::this_thread::sleep_for(std::chrono::seconds(2));
        kii_author_t spent = renewed;
        strcpy(spent.access_token, UNKNOWN_TOKEN);
        kii_session_set_author(&session, &spent);
        sent.data.clear();
        REQUIRE( kii_post_object(&kii, &bucket, "{\"k\":\"x\"}", NULL, &object_id) == KII_ERR_OK );
        CHECK( sent_has(sent, REFRESH_GRANT) );
        CHECK( sent_has(sent, PASSWORD_GRANT) );
        const kii_author_t relogged = held(&session);
        CHECK_FALSE( same_token(relogged.access_token, UNKNOWN_TOKEN) );
        CHECK_FALSE( empty(relogged.refresh_token) );
        CHECK_FALSE( same_token(relogged.refresh_token, renewed.refresh_token) );
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
