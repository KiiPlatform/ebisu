// The room a renewal takes, kii built with an allocator of these tests' own for
// it, which can fail: _KII_RENEW_MALLOC.
#include "catch_amalgamated.hpp"
#include "kii.h"
#include "http_script.h"

#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using khct::contains;
using khct::http_response;

namespace {

// What kii asked of the allocator since the test started.
std::vector<size_t> asked;
bool failing = false;

}

extern "C" void* khct_renew_malloc(size_t size) {
    asked.push_back(size);
    return (failing) ? NULL : malloc(size);
}

namespace {

struct Platform {
    uint64_t now_ms;
    std::vector<kii_author_t> updates;
    khct::cb::LockCtx lock;
    khct::cb::SystemCtx system;
    Platform() : now_ms(0) {
        system.on_monotonic_ms = [this] { return now_ms; };
        asked.clear();
        failing = false;
    }
};

void cb_updated(const kii_author_t* author, void* userdata) {
    ((Platform*)userdata)->updates.push_back(*author);
}

// A session holding a token issued without a lifetime, renewed through the
// token endpoint, about 640 bytes, when the server refuses it.
void init_session(kii_session_t* session, Platform* platform) {
    kii_session_init(session);
    kii_session_set_lock(session, platform->lock.get());
    kii_session_set_system(session, platform->system.system());
    kii_session_set_cb_author_updated(session, cb_updated, platform);
    kii_author_t author;
    memset(&author, 0, sizeof(author));
    strcpy(author.author_id, "th.1");
    strcpy(author.access_token, "at1");
    strcpy(author.refresh_token, "rt1");
    kii_session_set_author(session, &author);
}

std::string held_token(kii_session_t* session) {
    kii_author_t author;
    kii_session_get_author(session, &author);
    return author.access_token;
}

struct Client {
    char buff[4096];
    kii_t kii;
    khct::HttpScript http;

    explicit Client(size_t buff_size) {
        memset(buff, 0, sizeof(buff));
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, buff_size);
        kii_set_system(&kii, khct::cb::delay_only_system());
        http.attach(&kii);
    }

    kii_code_t send(const char* method, const std::string& body) {
        kii_code_t res = kii_api_call_start(
                &kii, method, "/api/apps/app/things/th.1/x", "application/json", true);
        if (res == KII_ERR_OK && !body.empty()) {
            res = kii_api_call_append_body(&kii, body.c_str(), body.size());
        }
        if (res != KII_ERR_OK) {
            return res;
        }
        return kii_api_call_run(&kii);
    }
};

const std::string REFUSED = http_response(403, "{\"errorCode\":\"WRONG_TOKEN\"}");
const std::string REFRESHED = http_response(200,
        "{\"id\":\"th.1\",\"access_token\":\"at2\",\"refresh_token\":\"rt2\",\"token_type\":\"Bearer\"}");
const std::string DONE = http_response(200, "{\"done\":true}");
const char TOKEN_LINE[] = "POST /api/apps/app/oauth2/token HTTP/1.1";

// 300 bytes, which leave a buffer of 800 too little room after them.
const std::string BODY = "{\"k\":\"" + std::string(290, 'v') + "\"}";

}

TEST_CASE( "A renewal the body leaves enough room for allocates nothing" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c(4096);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = { REFUSED, REFRESHED, DONE };
    REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( contains(c.http.requests[2], BODY) );
    CHECK( asked.empty() );
}

TEST_CASE( "A renewal the body leaves too little room for takes room of its own" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    SECTION("allocated, as much as the renewal needs") {
        c.http.responses = { REFUSED, REFRESHED, DONE };
        REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 3 );
        CHECK( contains(c.http.requests[1], TOKEN_LINE) );
        CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        CHECK( contains(c.http.requests[2], BODY) );
        REQUIRE( asked.size() == 1 );
        CHECK( asked[0] >= 640 );
        CHECK( asked[0] <= KII_SESSION_RENEW_BUFF_SIZE );
    }

    SECTION("the session's, which is cleared afterwards") {
        char room[KII_SESSION_RENEW_BUFF_SIZE];
        memset(room, 'x', sizeof(room));
        kii_session_set_renew_buff(&session, room, sizeof(room));
        c.http.responses = { REFUSED, REFRESHED, DONE };
        REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 3 );
        CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        CHECK( contains(c.http.requests[2], BODY) );
        CHECK( asked.empty() );
        CHECK( std::string(room, sizeof(room)) == std::string(sizeof(room), '\0') );
    }

    SECTION("the session's, too small for the response: renewed nothing, sent nothing again") {
        char room[64];
        kii_session_set_renew_buff(&session, room, sizeof(room));
        c.http.responses = { REFUSED, REFRESHED };
        REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 403 );
        CHECK( c.http.requests.size() == 2 );
        CHECK( asked.empty() );
        CHECK( held_token(&session) == "at1" );
    }
}

// Renewing in the request's buffer would have lost the body, and the server
// never got it, behind a 403 the caller couldn't tell from any other.
TEST_CASE( "A renewal that can have no room but the body's fails the request" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    failing = true;

    c.http.responses = { REFUSED, REFRESHED, DONE };
    REQUIRE( c.send("PUT", BODY) == KII_ERR_ALLOCATION );
    CHECK( c.http.requests.size() == 1 );
    CHECK( asked.size() == 1 );
    CHECK( held_token(&session) == "at1" );
    CHECK( platform.updates.empty() );
    CHECK( kii_get_resp_status(&c.kii) == 403 );
    CHECK( std::string(c.buff, kii_get_resp_body_length(&c.kii)) == "{\"errorCode\":\"WRONG_TOKEN\"}" );

    // Once there is room, the next request renews.
    failing = false;
    c.http.requests.clear();
    c.http.responses = { REFUSED, REFRESHED, DONE };
    REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( contains(c.http.requests[2], BODY) );
    CHECK( held_token(&session) == "at2" );
}

// An empty body has nothing to keep: it is sent again whatever room the
// renewal takes, the request's buffer among them.
TEST_CASE( "A request without a body is sent again, room of its own or not" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    // Less than the 640 bytes the renewal needs at most, but enough for the
    // one it gets.
    Client c(600);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    failing = GENERATE(false, true);
    INFO( "allocation " << (failing ? "fails" : "succeeds") );

    c.http.responses = { REFUSED, REFRESHED, DONE };
    REQUIRE( c.send("GET", "") == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( contains(c.http.requests[2], "GET /api/apps/app/things/th.1/x HTTP/1.1") );
    CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    CHECK( asked.size() == 1 );
    CHECK( held_token(&session) == "at2" );
}

// Ahead of expiry, the token is still good: a renewal that can't have room is
// left for later, and the request goes with the token it has.
TEST_CASE( "A renewal ahead of expiry that can have no room but the body's isn't made" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // A 1000 second token, renewed from 200 down to 100 seconds before it
    // expires: due at 950.
    platform.now_ms = 1000;
    c.http.responses = { http_response(200,
            "{\"id\":\"th.1\",\"access_token\":\"at1\",\"refresh_token\":\"rt1\",\"expires_in\":1000}") };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    platform.now_ms = 1000 + 950 * 1000;
    failing = true;

    c.http.requests.clear();
    c.http.responses = { DONE };
    REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 1 );
    CHECK( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
    CHECK( contains(c.http.requests[0], BODY) );
    CHECK( asked.size() == 1 );

    // With room, the next one renews first.
    failing = false;
    c.http.requests.clear();
    c.http.responses = { REFRESHED, DONE };
    REQUIRE( c.send("PUT", BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    CHECK( contains(c.http.requests[0], TOKEN_LINE) );
    CHECK( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );
}
