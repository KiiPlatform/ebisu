#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "http_script.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <string.h>

using khct::contains;
using khct::http_response;

namespace {

kii_author_t make_author(const char* id, const char* access_token, const char* refresh_token) {
    kii_author_t author;
    memset(&author, 0, sizeof(author));
    strcpy(author.author_id, id);
    strcpy(author.access_token, access_token);
    strcpy(author.refresh_token, refresh_token);
    return author;
}

// What the session asks of the platform, plus a record of what it was told.
struct Platform {
    uint64_t now_ms;
    int depth;
    int max_depth;
    std::vector<kii_author_t> updates;
    Platform() : now_ms(0), depth(0), max_depth(0) {}
};

void cb_lock(void* userdata) {
    Platform* platform = (Platform*)userdata;
    ++platform->depth;
    if (platform->depth > platform->max_depth) {
        platform->max_depth = platform->depth;
    }
}

void cb_unlock(void* userdata) {
    --((Platform*)userdata)->depth;
}

uint64_t cb_now(void* userdata) {
    return ((Platform*)userdata)->now_ms;
}

void cb_updated(const kii_author_t* author, void* userdata) {
    ((Platform*)userdata)->updates.push_back(*author);
}

void init_session(kii_session_t* session, Platform* platform) {
    kii_session_init(session);
    kii_session_set_cb_lock(session, cb_lock, cb_unlock, platform);
    kii_session_set_cb_monotonic_ms(session, cb_now, platform);
    kii_session_set_cb_author_updated(session, cb_updated, platform);
}

struct Client {
    char buff[4096];
    jkii_token_t tokens[256];
    jkii_resource_t resource;
    kii_t kii;
    khct::HttpScript http;

    explicit Client(size_t buff_size = 4096) {
        resource.tokens = tokens;
        resource.tokens_num = 256;
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, buff_size);
        kii_set_json_parser_resource(&kii, &resource);
        kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);
        http.attach(&kii);
    }

    // Built like tio's command results, which is what most needs sending again.
    kii_code_t start(const std::string& body) {
        kii_code_t res = kii_api_call_start(
                &kii, "PUT", "/api/apps/app/things/th.1/x", "application/json", KII_TRUE);
        if (res != KII_ERR_OK) {
            return res;
        }
        return kii_api_call_append_body(&kii, body.c_str(), body.size());
    }

    kii_code_t put(const std::string& body) {
        kii_code_t res = start(body);
        if (res != KII_ERR_OK) {
            return res;
        }
        return kii_api_call_run(&kii);
    }

    std::string resp_body() {
        return std::string(buff, kii_get_resp_body_length(&kii));
    }
};

kii_author_t held(kii_session_t* session) {
    kii_author_t author;
    kii_session_get_author(session, &author);
    return author;
}

const char REQ_LINE[] = "PUT https://api.kii.com/api/apps/app/things/th.1/x HTTP/1.1";
const char TOKEN_LINE[] = "POST https://api.kii.com/api/apps/app/oauth2/token HTTP/1.1";
const char BODY[] = "{\"k\":\"v\"}";

std::string refreshed(const char* access_token, const char* refresh_token, int64_t expires_in) {
    return http_response(200,
            std::string("{\"id\":\"th.1\",\"access_token\":\"") + access_token +
            "\",\"refresh_token\":\"" + refresh_token +
            "\",\"expires_in\":" + std::to_string(expires_in) + ",\"token_type\":\"Bearer\"}");
}

// How Kii refuses a token that has expired or been replaced.
const std::string REFUSED = http_response(403, "{\"errorCode\":\"WRONG_TOKEN\"}");
const std::string INVALID_GRANT = http_response(400, "{\"errorCode\":\"invalid_grant\"}");
const std::string DONE = http_response(200, "{\"done\":true}");

}

TEST_CASE( "kii_set_session requires the lock and the clock" ) {
    Platform platform;
    Client c;
    kii_session_t session;

    kii_session_init(&session);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    kii_session_set_cb_lock(&session, cb_lock, cb_unlock, &platform);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    kii_session_set_cb_monotonic_ms(&session, cb_now, &platform);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    REQUIRE( c.kii._session == &session );

    // Detaching needs neither.
    REQUIRE( kii_set_session(&c.kii, NULL) == KII_ERR_OK );
    REQUIRE( c.kii._session == nullptr );
}

TEST_CASE( "A request refused for its token is sent again with a refreshed token" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );

    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.resp_body() == "{\"done\":true}" );
    REQUIRE( c.http.requests.size() == 3 );

    REQUIRE( contains(c.http.requests[0], REQ_LINE) );
    REQUIRE( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );

    REQUIRE( contains(c.http.requests[1], TOKEN_LINE) );
    REQUIRE( contains(c.http.requests[1], "\"grant_type\":\"refresh_token\"") );
    REQUIRE( contains(c.http.requests[1], "\"refresh_token\":\"rt1\"") );
    REQUIRE_FALSE( contains(c.http.requests[1], "Authorization:") );

    // The same request, body included, with the new token instead of the old.
    REQUIRE( contains(c.http.requests[2], REQ_LINE) );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE_FALSE( contains(c.http.requests[2], "at1") );
    REQUIRE( contains(c.http.requests[2], BODY) );

    REQUIRE( platform.updates.size() == 1 );
    REQUIRE( std::string(platform.updates[0].author_id) == "th.1" );
    REQUIRE( std::string(platform.updates[0].access_token) == "at2" );
    REQUIRE( std::string(platform.updates[0].refresh_token) == "rt2" );
    REQUIRE( std::string(held(&session).access_token) == "at2" );
    REQUIRE( std::string(c.kii._author.access_token) == "at2" );

    REQUIRE( platform.depth == 0 );
    REQUIRE( platform.max_depth == 1 );
}

TEST_CASE( "A refused refresh token falls back to the password" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = {
        REFUSED,
        INVALID_GRANT,
        // No refresh token in this one: the server need not issue one.
        http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at3\",\"expires_in\":3600}"),
        DONE
    };
    REQUIRE( c.put(BODY) == KII_ERR_OK );

    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 4 );
    REQUIRE( contains(c.http.requests[1], "\"grant_type\":\"refresh_token\"") );
    REQUIRE( contains(c.http.requests[2], TOKEN_LINE) );
    REQUIRE( contains(c.http.requests[2], "\"username\":\"VENDOR_THING_ID:vid\"") );
    REQUIRE( contains(c.http.requests[2], "\"grant_type\":\"password\"") );
    REQUIRE( contains(c.http.requests[3], "Authorization: Bearer at3\r\n") );
    REQUIRE( contains(c.http.requests[3], BODY) );

    // The old refresh token belonged to the pair the login replaced.
    REQUIRE( platform.updates.size() == 1 );
    REQUIRE( std::string(held(&session).access_token) == "at3" );
    REQUIRE( std::string(held(&session).refresh_token) == "" );
}

TEST_CASE( "Without a password a refused refresh token leaves the 403" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );

    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( platform.updates.empty() );
    REQUIRE( std::string(held(&session).access_token) == "at1" );
    REQUIRE( std::string(held(&session).refresh_token) == "rt1" );
    REQUIRE( platform.depth == 0 );
}

TEST_CASE( "A refresh that gets no answer does not fall back to the password" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // The second connection is refused: the refresh token may still be good.
    c.http.responses = { REFUSED, "" };
    REQUIRE( c.put(BODY) == KII_ERR_OK );

    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( platform.updates.empty() );
    REQUIRE( std::string(held(&session).refresh_token) == "rt1" );
}

const uint64_t MINUTE_MS = 60 * 1000;
const char PASSWORD_GRANT[] = "\"grant_type\":\"password\"";

// Where the application limits login attempts, trying a wrong password on every
// refused request would lock the thing out.
TEST_CASE( "A refused password is not tried again until the session has waited" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    // No refresh token: a renewal logs in straight away.
    kii_author_t author = make_author("th.1", "at1", "");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // 5 minutes after the first refusal, twice as long after each further one,
    // up to an hour.
    const uint64_t waits[] = { 5, 10, 20, 40, 60, 60 };
    for (uint64_t wait : waits) {
        INFO("wait " << wait);
        c.http.requests.clear();
        c.http.responses = { REFUSED, INVALID_GRANT };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], PASSWORD_GRANT) );

        // Meanwhile a refused request is only reported.
        platform.now_ms += wait * MINUTE_MS - 1;
        c.http.requests.clear();
        c.http.responses = { REFUSED };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 1 );
        platform.now_ms += 1;
    }

    // Accepted, the request is sent again, and the wait is over.
    c.http.requests.clear();
    c.http.responses = { REFUSED, http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at2\"}"), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( platform.updates.size() == 1 );

    // So a refusal after that waits 5 minutes again.
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    platform.now_ms += 5 * MINUTE_MS;
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( contains(c.http.requests[1], PASSWORD_GRANT) );
    REQUIRE( platform.depth == 0 );
}

TEST_CASE( "Setting the credentials again ends the wait" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    kii_author_t author = make_author("th.1", "at1", "");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );

    kii_session_set_password(&session, "vid", "pw2");
    REQUIRE( platform.depth == 0 );
    c.http.requests.clear();
    c.http.responses = { REFUSED, http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at2\"}"), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    REQUIRE( contains(c.http.requests[1], "\"password\":\"pw2\"") );
}

TEST_CASE( "Only an answer refusing the password makes the session wait" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    kii_author_t author = make_author("th.1", "at1", "");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    SECTION("a server error") {
        c.http.responses = { REFUSED, http_response(503, "{}") };
    }

    SECTION("no answer") {
        c.http.responses = { REFUSED, "" };
    }

    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );

    // Tried again at once.
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( contains(c.http.requests[1], PASSWORD_GRANT) );
}

TEST_CASE( "The token is refreshed ahead of expiry" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // A 100 second token, renewed at 80% of its lifetime.
    platform.now_ms = 1000;
    c.http.responses = {
        http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at1\",\"refresh_token\":\"rt1\",\"expires_in\":100}")
    };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    REQUIRE( platform.updates.size() == 1 );
    REQUIRE( std::string(held(&session).refresh_token) == "rt1" );

    SECTION("not yet") {
        platform.now_ms = 1000 + 80000 - 1;
        c.http.responses.push_back(DONE);
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at1\r\n") );
    }

    SECTION("due") {
        platform.now_ms = 1000 + 80000;
        c.http.responses.push_back(refreshed("at2", "rt2", 100));
        c.http.responses.push_back(DONE);
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( contains(c.http.requests[1], "\"refresh_token\":\"rt1\"") );
        REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        REQUIRE( contains(c.http.requests[2], BODY) );
        REQUIRE( platform.updates.size() == 2 );

        // Timed from the renewal now.
        platform.now_ms = 1000 + 80000 + 80000 - 1;
        c.http.responses.push_back(DONE);
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 4 );
        REQUIRE( contains(c.http.requests[3], "Authorization: Bearer at2\r\n") );
    }
}

TEST_CASE( "A token renewed through another kii_t is taken, not renewed again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client a;
    Client b;
    REQUIRE( kii_set_session(&a.kii, &session) == KII_ERR_OK );
    REQUIRE( kii_set_session(&b.kii, &session) == KII_ERR_OK );

    SECTION("renewed while the request was being built") {
        REQUIRE( b.start(BODY) == KII_ERR_OK );

        a.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), DONE };
        REQUIRE( a.put(BODY) == KII_ERR_OK );

        b.http.responses = { DONE };
        REQUIRE( kii_api_call_run(&b.kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&b.kii) == 200 );
        REQUIRE( b.http.requests.size() == 1 );
        REQUIRE( contains(b.http.requests[0], "Authorization: Bearer at2\r\n") );
    }

    SECTION("renewed while the request was refused") {
        kii_author_t renewed = make_author("th.1", "at2", "rt2");
        b.http.on_connect = [&](size_t i) {
            if (i == 0) {
                kii_session_set_author(&session, &renewed);
            }
        };
        b.http.responses = { REFUSED, DONE };
        REQUIRE( b.put(BODY) == KII_ERR_OK );

        REQUIRE( kii_get_resp_status(&b.kii) == 200 );
        REQUIRE( b.http.requests.size() == 2 );
        REQUIRE( contains(b.http.requests[0], "Authorization: Bearer at1\r\n") );
        REQUIRE( contains(b.http.requests[1], "Authorization: Bearer at2\r\n") );
        REQUIRE( contains(b.http.requests[1], BODY) );
    }

    // Only the one renewal: a second would have spent rt1 again.
    REQUIRE( platform.updates.size() <= 1 );
    REQUIRE( platform.depth == 0 );
}

TEST_CASE( "kii_get_author gives the session's author, renewed through any kii_t" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client a;
    Client b;
    REQUIRE( kii_set_session(&a.kii, &session) == KII_ERR_OK );
    REQUIRE( kii_set_session(&b.kii, &session) == KII_ERR_OK );

    a.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), DONE };
    REQUIRE( a.put(BODY) == KII_ERR_OK );

    // b has made no request since, and so still holds at1 itself.
    REQUIRE( std::string(b.kii._author.access_token) == "at1" );
    kii_author_t got;
    kii_get_author(&b.kii, &got);
    REQUIRE( std::string(got.author_id) == "th.1" );
    REQUIRE( std::string(got.access_token) == "at2" );
    REQUIRE( std::string(got.refresh_token) == "rt2" );
    REQUIRE( platform.depth == 0 );
}

namespace {

struct StateReader {
    std::string state;
    size_t pos;
};

size_t read_state(char* buffer, size_t size, void* userdata) {
    StateReader* reader = (StateReader*)userdata;
    size_t n = reader->state.size() - reader->pos;
    if (n > size) {
        n = size;
    }
    memcpy(buffer, reader->state.data() + reader->pos, n);
    reader->pos += n;
    return n;
}

}

TEST_CASE( "A streamed body is not sent again, but the token is renewed" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    StateReader reader = { "{\"alias\":{\"power\":true}}", 0 };
    c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600) };
    REQUIRE( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, NULL) == KII_ERR_RESP_STATUS );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( std::string(held(&session).access_token) == "at2" );

    // Calling again, with the reader started over, uses the new token.
    reader.pos = 0;
    c.http.responses.push_back(http_response(204, ""));
    REQUIRE( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, NULL) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 3 );
    REQUIRE( contains(c.http.requests[2], "PUT https://api.kii.com/thing-if/apps/app/targets/thing:th.1/states HTTP/1.1") );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(c.http.requests[2], reader.state) );
}

TEST_CASE( "A body leaving no room to renew is not sent again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    // The renewal needs about 640 bytes after a 300 byte body.
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";

    c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600) };
    REQUIRE( c.put(body) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( std::string(held(&session).access_token) == "at2" );

    c.http.responses.push_back(DONE);
    REQUIRE( c.put(body) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(c.http.requests[2], body) );
}

TEST_CASE( "Only requests carrying the session's token are renewed" ) {
    Client c;

    SECTION("no session: the 403 and its body reach the caller as before") {
        c.http.responses = { REFUSED };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 1 );
        REQUIRE( c.resp_body() == "{\"errorCode\":\"WRONG_TOKEN\"}" );
    }

    SECTION("a token of the application's own") {
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

        REQUIRE( kii_api_call_start(&c.kii, "GET", "/api/apps/app/x", NULL, KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_header(&c.kii, "Authorization", "Bearer other") == KII_ERR_OK );
        c.http.responses = { REFUSED };
        REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 1 );
        REQUIRE( platform.updates.empty() );
    }
}

TEST_CASE( "Responses that do not refuse the token reach the caller whole" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);

    // Kii answers 401 for other reasons, and 403 with other error codes.
    const struct {
        uint16_t status;
        std::string body;
    } cases[] = {
        { 401, "{\"errorCode\":\"WRONG_TOKEN\"}" },
        { 401, "{\"errorCode\":\"UNAUTHORIZED\"}" },
        { 403, "{\"errorCode\":\"OPERATION_NOT_ALLOWED\"}" },
        { 403, "{\"errorCode\":\"WRONG_TOKENS\"}" },
        { 403, "{\"errorCode\":\"WRONG\"}" },
        { 403, "{\"message\":\"WRONG_TOKEN\"}" },
        { 403, "<html>WRONG_TOKEN</html>" },
        { 403, "" },
    };
    for (const auto& r : cases) {
        INFO(r.status << " " << r.body);
        Client c;
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
        c.http.responses = { http_response(r.status, r.body) };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == r.status );
        CHECK( c.resp_body() == r.body );
        CHECK( c.http.requests.size() == 1 );
        CHECK( c.kii._session_refused == KII_FALSE );
    }
    CHECK( platform.updates.empty() );
    CHECK( std::string(held(&session).access_token) == "at1" );
}

TEST_CASE( "A 403 with no room after the request's body" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    // 300 bytes of body leave room for the renewal, but not for a 3.8 KB 403.
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";
    const std::string message = std::string(3800, 'm');

    SECTION("refusing the token: renewed, but not sent again") {
        c.http.responses = {
            http_response(403, "{\"errorCode\":\"WRONG_TOKEN\",\"message\":\"" + message + "\"}"),
            refreshed("at2", "rt2", 3600)
        };
        REQUIRE( c.put(body) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( c.kii._session_refused == KII_TRUE );
        REQUIRE( std::string(held(&session).access_token) == "at2" );

        c.http.responses.push_back(DONE);
        REQUIRE( c.put(body) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        REQUIRE( contains(c.http.requests[2], body) );
    }

    SECTION("for another reason: its body reaches the caller whole") {
        const std::string forbidden =
            "{\"errorCode\":\"OPERATION_NOT_ALLOWED\",\"message\":\"" + message + "\"}";
        c.http.responses = { http_response(403, forbidden) };
        REQUIRE( c.put(body) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.resp_body() == forbidden );
        REQUIRE( c.http.requests.size() == 1 );
        REQUIRE( platform.updates.empty() );
    }
}

TEST_CASE( "A push message refused for its token is sent again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    const char message[] = "{\"data\":{\"k\":\"v\"}}";
    c.http.responses = {
        REFUSED,
        refreshed("at2", "rt2", 3600),
        http_response(201, "{\"pushMessageID\":\"m1\"}")
    };
    SECTION("to a topic") {
        kii_topic_t topic;
        topic.scope = KII_SCOPE_THING;
        topic.scope_id = "th.1";
        topic.topic_name = "my-topic";
        REQUIRE( kii_send_push_message(&c.kii, &topic, message) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( contains(c.http.requests[2],
                "POST https://api.kii.com/api/apps/app/things/th.1/topics/my-topic/push/messages HTTP/1.1") );
    }
    SECTION("straight to the thing") {
        REQUIRE( kii_send_push_message_to_thing(&c.kii, "th.1", message) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( contains(c.http.requests[2],
                "POST https://api.kii.com/api/apps/app/things/th.1/push/messages HTTP/1.1") );
    }
    REQUIRE( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
    REQUIRE( contains(c.http.requests[1], "\"refresh_token\":\"rt1\"") );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(c.http.requests[2], message) );
}

// Onboarding again, authorized with the app's ID: how a token onboarding
// obtained is renewed, through Thing-IF alone and never the token endpoint.
std::string onboarded(const char* access_token) {
    return http_response(200, std::string("{\"accessToken\":\"") + access_token +
            "\",\"thingID\":\"th.1\",\"mqttEndpoint\":{\"installationID\":\"ins1\"}}");
}
const char ONBOARDING_LINE[] = "POST https://api.kii.com/thing-if/apps/app/onboardings HTTP/1.1";

bool onboarding_again(const std::string& onboarding) {
    return contains(onboarding, ONBOARDING_LINE) &&
        // "app:dummy"
        contains(onboarding, "Authorization: Basic YXBwOmR1bW15\r\n") &&
        contains(onboarding, "\"vendorThingID\":\"vid\"") &&
        contains(onboarding, "\"thingPassword\":\"pw\"");
}

TEST_CASE( "A token onboarding obtained is renewed by onboarding again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    // Onboarding replaces what was held, refresh token included.
    kii_author_t author = make_author("th.0", "at0", "rt0");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // Its response has neither a refresh token nor a lifetime.
    c.http.responses = { onboarded("at1") };
    REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    REQUIRE( platform.updates.size() == 1 );
    kii_author_t first = held(&session);
    REQUIRE( std::string(first.author_id) == "th.1" );
    REQUIRE( std::string(first.access_token) == "at1" );
    REQUIRE( std::string(first.refresh_token) == "" );
    REQUIRE( session._renew_at_known == KII_FALSE );
    REQUIRE( session._renew_by_onboarding == KII_TRUE );

    // Refused, it onboards again, and the request is sent again.
    c.http.responses.push_back(REFUSED);
    c.http.responses.push_back(onboarded("at2"));
    c.http.responses.push_back(DONE);
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 4 );
    REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at1\r\n") );
    REQUIRE( onboarding_again(c.http.requests[2]) );
    REQUIRE( contains(c.http.requests[3], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(c.http.requests[3], BODY) );
    REQUIRE( platform.updates.size() == 2 );
    REQUIRE( std::string(held(&session).access_token) == "at2" );
    REQUIRE( std::string(held(&session).refresh_token) == "" );
    REQUIRE( session._renew_at_known == KII_FALSE );

    // And so on, each time: nothing ever goes to the token endpoint.
    c.http.responses.push_back(REFUSED);
    c.http.responses.push_back(onboarded("at3"));
    c.http.responses.push_back(DONE);
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 7 );
    REQUIRE( onboarding_again(c.http.requests[5]) );
    REQUIRE( contains(c.http.requests[6], "Authorization: Bearer at3\r\n") );
    for (const std::string& request : c.http.requests) {
        CHECK_FALSE( contains(request, TOKEN_LINE) );
    }
}

TEST_CASE( "Without the thing's credentials, an onboarded token is not renewed" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    c.http.responses = { onboarded("at1"), REFUSED };
    REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    // kii_api_call_run() leaves the status to kii_get_resp_status().
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( std::string(held(&session).access_token) == "at1" );
}

TEST_CASE( "A refused onboarding is not tried again until the session has waited" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    c.http.responses = { onboarded("at1") };
    REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    c.http.responses.push_back(REFUSED);
    c.http.responses.push_back(http_response(400, "{\"errorCode\":\"WRONG_PASSWORD\"}"));
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 3 );
    REQUIRE( onboarding_again(c.http.requests[2]) );

    platform.now_ms += 5 * MINUTE_MS - 1;
    c.http.responses.push_back(REFUSED);
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 4 );

    platform.now_ms += 1;
    c.http.responses.push_back(REFUSED);
    c.http.responses.push_back(onboarded("at2"));
    c.http.responses.push_back(DONE);
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 7 );
    REQUIRE( onboarding_again(c.http.requests[5]) );
    REQUIRE( contains(c.http.requests[6], "Authorization: Bearer at2\r\n") );
}

TEST_CASE( "How the token was obtained decides how it is renewed" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    SECTION("a login after onboarding renews through the token endpoint") {
        c.http.responses = { onboarded("at1"),
            refreshed("at2", "rt2", 100), REFUSED, refreshed("at3", "rt3", 100), DONE };
        REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );
        REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
        REQUIRE( session._renew_by_onboarding == KII_FALSE );
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 5 );
        REQUIRE( contains(c.http.requests[3], "\"refresh_token\":\"rt2\"") );
        REQUIRE( contains(c.http.requests[4], "Authorization: Bearer at3\r\n") );
    }

    SECTION("a restored author is renewed by onboarding when told so") {
        kii_author_t saved = make_author("th.1", "at1", "");
        kii_session_set_author(&session, &saved);
        REQUIRE( session._renew_by_onboarding == KII_FALSE );
        kii_session_set_renew_by_onboarding(&session, KII_TRUE);
        c.http.responses = { REFUSED, onboarded("at2"), DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( onboarding_again(c.http.requests[1]) );
        REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        // Only the renewal told the author-updated callback.
        REQUIRE( platform.updates.size() == 1 );
    }
}

TEST_CASE( "A login replaces the refresh token, also with none" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at2\"}") };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    REQUIRE( std::string(held(&session).access_token) == "at2" );
    REQUIRE( std::string(held(&session).refresh_token) == "" );
    // Without an expiry it is used until refused.
    REQUIRE( session._renew_at_known == KII_FALSE );
}

namespace {

// A login through a session of its own, its response giving "expires_in" as
// the JSON text passed.
struct Login {
    Platform platform;
    kii_session_t session;
    Client c;
    kii_code_t res;

    Login(const std::string& expires_in, uint64_t now_ms) {
        init_session(&session, &platform);
        kii_set_session(&c.kii, &session);
        platform.now_ms = now_ms;
        c.http.responses = {
            http_response(200,
                    "{\"id\":\"th.1\",\"access_token\":\"at1\",\"refresh_token\":\"rt1\","
                    "\"expires_in\":" + expires_in + "}")
        };
        res = kii_auth_thing(&c.kii, "vid", "pw");
    }
};

}

TEST_CASE( "Token lifetimes are read as 64-bit integers" ) {
    // A 32-bit long, as on Cortex-M, with -m32 and on 64-bit Windows, used to
    // turn the second into a negative lifetime, left unscheduled.
    const struct {
        const char* text;
        uint64_t renew_after_ms;
    } cases[] = {
        { "2147483647", UINT64_C(2147483647000) - 300000 },
        { "2147483648", UINT64_C(2147483648000) - 300000 },
        { "4294967295", UINT64_C(4294967295000) - 300000 },
    };
    for (const auto& c : cases) {
        INFO("expires_in " << c.text);
        Login login(c.text, 1000);
        REQUIRE( login.res == KII_ERR_OK );
        CHECK( login.session._renew_at_known == KII_TRUE );
        CHECK( login.session._stored_at_ms == 1000 );
        CHECK( login.session._renew_after_ms == c.renew_after_ms );
    }
}

TEST_CASE( "A lifetime that cannot be scheduled keeps the token until refused" ) {
    const char* cases[] = {
        "0",
        "-1",
        "-9223372036854775808",
        // Beyond UINT32_MAX seconds. A 32-bit long used to cut the second down
        // to 100.
        "4294967296",
        "4294967396",
        "9223372036854775807",
        // Beyond int64_t.
        "99999999999999999999",
        "-99999999999999999999",
        // Not an integer.
        "3600.5",
        "\"3600\"",
        "null",
    };
    for (const char* text : cases) {
        INFO("expires_in " << text);
        Login login(text, 1000);
        REQUIRE( login.res == KII_ERR_OK );
        CHECK( login.session._renew_at_known == KII_FALSE );
        CHECK( login.platform.updates.size() == 1 );
        CHECK( std::string(held(&login.session).access_token) == "at1" );
        CHECK( std::string(held(&login.session).refresh_token) == "rt1" );

        // However late, the next request goes with it.
        login.platform.now_ms = UINT64_MAX;
        login.c.http.responses.push_back(DONE);
        REQUIRE( login.c.put(BODY) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&login.c.kii) == 200 );
        REQUIRE( login.c.http.requests.size() == 2 );
        CHECK( contains(login.c.http.requests[1], "Authorization: Bearer at1\r\n") );
    }
}

TEST_CASE( "A refresh whose lifetime cannot be scheduled still keeps its tokens" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // rt1 is spent once the server has answered: failing over the lifetime
    // would lose rt2 too.
    c.http.responses = { REFUSED, refreshed("at2", "rt2", -1), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 3 );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( std::string(held(&session).access_token) == "at2" );
    REQUIRE( std::string(held(&session).refresh_token) == "rt2" );
    REQUIRE( session._renew_at_known == KII_FALSE );
}

TEST_CASE( "Renewal ahead of expiry does not depend on the clock's origin" ) {
    // The origin is arbitrary. This close to the top of the range, a deadline
    // of the time stored plus 80 seconds would wrap around and be due at once.
    const uint64_t origin = UINT64_MAX - 1000;
    Login login("100", origin);
    REQUIRE( login.res == KII_ERR_OK );
    REQUIRE( login.session._renew_at_known == KII_TRUE );

    login.platform.now_ms = origin + 500;
    login.c.http.responses.push_back(DONE);
    REQUIRE( login.c.put(BODY) == KII_ERR_OK );
    REQUIRE( login.c.http.requests.size() == 2 );
    REQUIRE( contains(login.c.http.requests[1], "Authorization: Bearer at1\r\n") );
    REQUIRE( login.platform.updates.size() == 1 );

    // Even a clock that wraps around has it renewed 80 seconds on.
    login.platform.now_ms = origin + 80000;
    login.c.http.responses.push_back(refreshed("at2", "rt2", 100));
    login.c.http.responses.push_back(DONE);
    REQUIRE( login.c.put(BODY) == KII_ERR_OK );
    REQUIRE( login.c.http.requests.size() == 4 );
    REQUIRE( contains(login.c.http.requests[2], "\"refresh_token\":\"rt1\"") );
    REQUIRE( contains(login.c.http.requests[3], "Authorization: Bearer at2\r\n") );
}

namespace {

// An MQTT broker that serves each connection the bytes given for it, and then
// fails the receive as a lost connection does.
struct Broker {
    std::vector<std::string> served;
    std::vector<std::string> received;
    size_t pos;
    khct::cb::SockCtx ctx;

    Broker() : pos(0) {
        ctx.on_connect = [this](void*, const char*, uint16_t) {
            received.push_back("");
            pos = 0;
            return KHC_SOCK_OK;
        };
        ctx.on_send = [this](void*, const char* buffer, size_t length, size_t* out_sent_length) {
            received.back().append(buffer, length);
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            size_t i = received.size() - 1;
            if (i >= served.size() || pos == served[i].size()) {
                return KHC_SOCK_FAIL;
            }
            *out_actual_length = served[i].copy(buffer, length_to_read, pos);
            pos += *out_actual_length;
            return KHC_SOCK_OK;
        };
        ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    }

    Broker(const Broker&) = delete;
    Broker& operator=(const Broker&) = delete;

    void attach(kii_t* kii) {
        kii_set_cb_mqtt_sock_connect(kii, khct::cb::mock_connect, &ctx);
        kii_set_cb_mqtt_sock_send(kii, khct::cb::mock_send, &ctx);
        kii_set_cb_mqtt_sock_recv(kii, khct::cb::mock_recv, &ctx);
        kii_set_cb_mqtt_sock_close(kii, khct::cb::mock_close, &ctx);
    }
};

const std::string CONNACK_ACCEPTED("\x20\x02\x00\x00", 4);
const std::string CONNACK_NOT_AUTHORIZED("\x20\x02\x00\x05", 4);
const std::string SUBACK("\x90\x03\x00\x01\x00", 5);

std::string endpoint(const char* username) {
    return http_response(200,
            std::string("{\"installationID\":\"ins1\",\"username\":\"") + username +
            "\",\"password\":\"pw\",\"mqttTopic\":\"topic\",\"host\":\"mqtt.kii.com\","
            "\"portTCP\":1883,\"portSSL\":8883,\"X-MQTT-TTL\":2147483647}");
}

const char ENDPOINT_LINE[] =
    "GET https://api.kii.com/api/apps/app/installations/ins1/mqtt-endpoint HTTP/1.1";

// Every failed receive is a lost connection, not a timeout.
kii_bool_t cb_recv_never_timed_out(void*) {
    return KII_FALSE;
}

// Runs the MQTT task until it has subscribed on its third connection.
struct UntilThirdSubscription {
    Broker* broker;
    int steps;
};

kii_bool_t cb_continue(void* task_info, void* userdata) {
    UntilThirdSubscription* until = (UntilThirdSubscription*)userdata;
    kii_mqtt_task_info* info = (kii_mqtt_task_info*)task_info;
    if (++until->steps > 100) {
        return KII_FALSE;
    }
    return (until->broker->received.size() == 3 &&
            info->task_state == KII_MQTT_ST_RECV_READY) ? KII_FALSE : KII_TRUE;
}

}

TEST_CASE( "The MQTT task renews an expired token to fetch new credentials" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    char mqtt_buff[1024];
    kii_set_mqtt_buff(&c.kii, mqtt_buff, sizeof(mqtt_buff));
    Broker broker;
    broker.attach(&c.kii);
    kii_set_cb_mqtt_sock_recv_timed_out(&c.kii, cb_recv_never_timed_out, NULL);
    UntilThirdSubscription until = { &broker, 0 };
    kii_set_cb_task_continue(&c.kii, cb_continue, &until);

    // Connected and subscribed with the first credentials, the connection is
    // lost. When the task connects again the broker refuses the credentials --
    // not for the token, which they outlive -- and fetching new ones meets a
    // token that has expired since.
    broker.served = {
        CONNACK_ACCEPTED + SUBACK,
        CONNACK_NOT_AUTHORIZED,
        CONNACK_ACCEPTED + SUBACK
    };
    c.http.responses = {
        http_response(201, "{\"installationID\":\"ins1\"}"),
        endpoint("user1"),
        REFUSED,
        refreshed("at2", "rt2", 3600),
        endpoint("user2")
    };
    mqtt_start_task(&c.kii);

    REQUIRE( until.steps <= 100 );
    REQUIRE( broker.received.size() == 3 );
    REQUIRE( contains(broker.received[0], "user1") );
    REQUIRE( contains(broker.received[1], "user1") );
    REQUIRE( contains(broker.received[2], "user2") );

    REQUIRE( c.http.requests.size() == 5 );
    REQUIRE( contains(c.http.requests[1], ENDPOINT_LINE) );
    REQUIRE( contains(c.http.requests[2], ENDPOINT_LINE) );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at1\r\n") );
    REQUIRE( contains(c.http.requests[3], TOKEN_LINE) );
    REQUIRE( contains(c.http.requests[3], "\"refresh_token\":\"rt1\"") );
    REQUIRE( contains(c.http.requests[4], ENDPOINT_LINE) );
    REQUIRE( contains(c.http.requests[4], "Authorization: Bearer at2\r\n") );

    REQUIRE( platform.updates.size() == 1 );
    REQUIRE( std::string(held(&session).access_token) == "at2" );
    REQUIRE( platform.depth == 0 );
    REQUIRE( platform.max_depth == 1 );
}

namespace {

// A lock that is one, for tasks that really run at once.
struct SharedPlatform {
    std::mutex mutex;
    std::atomic<int> refreshes;
    std::vector<kii_author_t> updates;
    SharedPlatform() : refreshes(0) {}
};

void cb_mutex_lock(void* userdata) {
    ((SharedPlatform*)userdata)->mutex.lock();
}

void cb_mutex_unlock(void* userdata) {
    ((SharedPlatform*)userdata)->mutex.unlock();
}

uint64_t cb_zero(void*) {
    return 0;
}

void cb_shared_updated(const kii_author_t* author, void* userdata) {
    ((SharedPlatform*)userdata)->updates.push_back(*author);
}

// A server on which at1 has expired and rt1 may be spent once.
std::string serve(SharedPlatform* platform, const std::string& request) {
    if (contains(request, TOKEN_LINE)) {
        ++platform->refreshes;
        if (contains(request, "\"refresh_token\":\"rt1\"")) {
            return refreshed("at2", "rt2", 3600);
        }
        return INVALID_GRANT;
    }
    // Let the other task in between.
    std::this_thread::yield();
    if (contains(request, "Authorization: Bearer at2\r\n")) {
        return contains(request, ENDPOINT_LINE) ? endpoint("user2") : DONE;
    }
    return REFUSED;
}

}

TEST_CASE( "Tasks refused at the same time renew the token once" ) {
    for (int round = 0; round < 50; ++round) {
        INFO("round " << round);
        SharedPlatform platform;
        kii_session_t session;
        kii_session_init(&session);
        kii_session_set_cb_lock(&session, cb_mutex_lock, cb_mutex_unlock, &platform);
        kii_session_set_cb_monotonic_ms(&session, cb_zero, NULL);
        kii_session_set_cb_author_updated(&session, cb_shared_updated, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);

        // The handler's MQTT task fetching an endpoint, and the updater.
        Client handler;
        Client updater;
        REQUIRE( kii_set_session(&handler.kii, &session) == KII_ERR_OK );
        REQUIRE( kii_set_session(&updater.kii, &session) == KII_ERR_OK );
        handler.http.respond = [&](const std::string& request) { return serve(&platform, request); };
        updater.http.respond = [&](const std::string& request) { return serve(&platform, request); };
        mqtt_state_t state;
        _init_mqtt_state(&handler.kii, &state);
        strcpy(state.ins_id.id, "ins1");
        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;

        kii_code_t put_res = KII_ERR_FAIL;
        std::atomic<int> ready(0);
        std::thread mqtt([&] {
            ++ready;
            while (ready < 2) {
                std::this_thread::yield();
            }
            _mqtt_state_get_endpoint(&state);
        });
        std::thread update([&] {
            ++ready;
            while (ready < 2) {
                std::this_thread::yield();
            }
            put_res = updater.put(BODY);
        });
        mqtt.join();
        update.join();

        CHECK( platform.refreshes == 1 );
        CHECK( platform.updates.size() == 1 );
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( std::string(state.endpoint.username) == "user2" );
        CHECK( put_res == KII_ERR_OK );
        CHECK( kii_get_resp_status(&updater.kii) == 200 );
        CHECK( updater.resp_body() == "{\"done\":true}" );
        CHECK( std::string(held(&session).access_token) == "at2" );
        CHECK( std::string(held(&session).refresh_token) == "rt2" );
    }
}

// The response to onboarding where MQTT is enabled: its endpoint, user and
// installation named after n.
std::string onboarded_with_endpoint(const char* access_token, const std::string& n) {
    return http_response(200, std::string("{\"accessToken\":\"") + access_token +
            "\",\"thingID\":\"th.1\",\"mqttEndpoint\":{\"installationID\":\"ins." + n +
            "\",\"username\":\"user." + n + "\",\"password\":\"pass." + n +
            "\",\"mqttTopic\":\"topic." + n + "\",\"host\":\"mqtt.example.com\","
            "\"portTCP\":1883,\"portSSL\":8883}}");
}

TEST_CASE( "Refused MQTT credentials are renewed by onboarding again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    c.http.responses = { onboarded_with_endpoint("at1", "1") };
    REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    mqtt_state_t state;
    _init_mqtt_state(&c.kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    REQUIRE( std::string(state.endpoint.username) == "user.1" );
    // Where a CONNACK refusing the credentials sends the task.
    state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;

    SECTION("onboarding again gives new credentials, and a new token") {
        c.http.responses.push_back(onboarded_with_endpoint("at2", "2"));
        _mqtt_state_get_endpoint(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( std::string(state.ins_id.id) == "ins.2" );
        CHECK( std::string(state.endpoint.username) == "user.2" );
        CHECK( std::string(state.endpoint.password) == "pass.2" );
        CHECK( std::string(state.endpoint.topic) == "topic.2" );
        CHECK( state.endpoint.port_ssl == 8883 );
        REQUIRE( c.http.requests.size() == 2 );
        CHECK( onboarding_again(c.http.requests[1]) );
        CHECK( std::string(held(&session).access_token) == "at2" );
        CHECK( std::string(c.kii._author.access_token) == "at2" );
        CHECK( platform.updates.size() == 2 );
    }

    SECTION("a server error is tried again") {
        c.http.responses.push_back(http_response(503, "{}"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        CHECK( state.info.error == KII_MQTT_ERR_OK );
        CHECK( std::string(held(&session).access_token) == "at1" );
    }

    SECTION("a refused onboarding ends the task") {
        c.http.responses.push_back(http_response(400, "{\"errorCode\":\"WRONG_PASSWORD\"}"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( state.info.error == KII_MQTT_ERR_GET_ENDPOINT );

        // A task started again does not onboard again until the session has
        // waited.
        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        state.info.error = KII_MQTT_ERR_OK;
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state != KII_MQTT_ST_SOCK_CONNECT );
        CHECK( c.http.requests.size() == 2 );

        platform.now_ms += 5 * MINUTE_MS;
        c.http.responses.push_back(onboarded_with_endpoint("at2", "2"));
        state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
        state.info.error = KII_MQTT_ERR_OK;
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( c.http.requests.size() == 3 );
        CHECK( onboarding_again(c.http.requests[2]) );
    }

    SECTION("onboarding without an endpoint, MQTT no longer enabled, ends the task") {
        c.http.responses.push_back(onboarded("at2"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_ERR_EXIT );
        CHECK( state.info.error == KII_MQTT_ERR_GET_ENDPOINT );
    }

    SECTION("a session renewing through the token endpoint fetches them from Kii Cloud") {
        kii_session_set_renew_by_onboarding(&session, KII_FALSE);
        c.http.responses.push_back(http_response(200,
                "{\"installationID\":\"ins.1\",\"username\":\"user.3\",\"password\":\"pass.3\","
                "\"mqttTopic\":\"topic.1\",\"host\":\"mqtt.example.com\",\"portTCP\":1883,"
                "\"portSSL\":8883,\"X-MQTT-TTL\":2147483647}"));
        _mqtt_state_get_endpoint(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( std::string(state.endpoint.username) == "user.3" );
        REQUIRE( c.http.requests.size() == 2 );
        CHECK( contains(c.http.requests[1], "/installations/ins.1/mqtt-endpoint") );
    }
}
