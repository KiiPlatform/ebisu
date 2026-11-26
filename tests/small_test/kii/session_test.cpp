#include "catch_amalgamated.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "http_script.h"
#include <ebisu_retry.h>

#include <atomic>
#include <mutex>
#include <set>
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

void cb_lock(void* userdata);
void cb_unlock(void* userdata);
uint64_t cb_now(void* userdata);

// What the session asks of the platform, plus a record of what it was told.
struct Platform {
    uint64_t now_ms;
    int depth;
    int max_depth;
    std::vector<kii_author_t> updates;
    khct::cb::LockCtx lock;
    khct::cb::SystemCtx system;
    Platform() : now_ms(0), depth(0), max_depth(0) {
        lock.on_lock = [this] { cb_lock(this); };
        lock.on_unlock = [this] { cb_unlock(this); };
        system.on_monotonic_ms = [this] { return cb_now(this); };
    }
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
    kii_session_set_lock(session, platform->lock.get());
    kii_session_set_system(session, platform->system.system());
    kii_session_set_cb_author_updated(session, cb_updated, platform);
}

struct Client {
    char buff[4096];
    kii_t kii;
    khct::HttpScript http;

    explicit Client(size_t buff_size = 4096) {
        // Defined, as a test may look through all of it.
        memset(buff, 0, sizeof(buff));
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, buff_size);
        kii_set_system(&kii, khct::cb::delay_only_system());
        http.attach(&kii);
    }

    // Built like tio's command results, which is what most needs sending again.
    kii_code_t start(const std::string& body) {
        kii_code_t res = kii_api_call_start(
                &kii, "PUT", "/api/apps/app/things/th.1/x", "application/json", true);
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

const char REQ_LINE[] = "PUT /api/apps/app/things/th.1/x HTTP/1.1";
const char TOKEN_LINE[] = "POST /api/apps/app/oauth2/token HTTP/1.1";
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

// How long after it was issued to thing th.1 a token of this lifetime is
// renewed: from a fifth of its lifetime, or 5 minutes if that is less, down
// to half of that before it expires, as the thing's ID has it.
uint64_t renew_after_ms(uint64_t lifetime_s, const char* thing_id = "th.1") {
    uint64_t lifetime_ms = lifetime_s * 1000;
    uint64_t margin_ms = lifetime_ms / 5 < 300000 ? lifetime_ms / 5 : 300000;
    return lifetime_ms - ebisu_retry_spread((uint32_t)margin_ms, ebisu_retry_seed(thing_id));
}

}

TEST_CASE( "kii_set_session requires the lock and the clock" ) {
    Platform platform;
    Client c;
    kii_session_t session;

    kii_session_init(&session);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    kii_session_set_lock(&session, platform.lock.get());
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    platform.system.ops.monotonic_ms = NULL;
    kii_session_set_system(&session, platform.system.system());
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    platform.system.ops.monotonic_ms = khct::cb::mock_monotonic_ms;
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

// The response with a Date header after its status line.
std::string dated(const std::string& resp, const std::string& date) {
    size_t eol = resp.find("\r\n");
    return resp.substr(0, eol + 2) + "Date: " + date + "\r\n" + resp.substr(eol + 2);
}

TEST_CASE( "The date of a call that renewed the token is its last response's" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    // These responses are dated 1994.
    c.kii._khc._resp_date_floor = 0;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // The refusal, the refresh and the request sent again each bring their own,
    // a second apart.
    c.http.responses = {
        dated(REFUSED, "Sun, 06 Nov 1994 08:49:37 GMT"),
        dated(refreshed("at2", "rt2", 3600), "Sun, 06 Nov 1994 08:49:38 GMT"),
        dated(DONE, "Sun, 06 Nov 1994 08:49:39 GMT"),
    };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( kii_get_resp_date(&c.kii) == 784111779 );

    // Without one in the last response, the earlier ones don't stand in for it.
    kii_session_set_author(&session, &author);
    c.http.requests.clear();
    c.http.responses = {
        dated(REFUSED, "Sun, 06 Nov 1994 08:49:37 GMT"),
        dated(refreshed("at2", "rt2", 3600), "Sun, 06 Nov 1994 08:49:38 GMT"),
        DONE,
    };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "A request not sent again after the renewal keeps its 403's date" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    // These responses are dated 1994.
    c.kii._khc._resp_date_floor = 0;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    // As in "A 403 with no room after the request's body": a refusal too long
    // to read after the body has it given up.
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";
    const std::string refused = http_response(403,
            "{\"errorCode\":\"WRONG_TOKEN\",\"message\":\"" + std::string(3800, 'm') + "\"}");

    c.http.responses = {
        dated(refused, "Sun, 06 Nov 1994 08:49:37 GMT"),
        dated(refreshed("at2", "rt2", 3600), "Sun, 06 Nov 1994 08:49:38 GMT"),
    };
    REQUIRE( c.put(body) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );
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

// How long a thing with this vendor thing ID waits after the n-th refusal of
// its credentials in a row: its share of 10 minutes doubled up to 2 hours.
uint64_t password_wait_ms(const char* vendor_thing_id, uint32_t n) {
    return ebisu_retry_wait(10 * 60 * 1000, 120 * 60 * 1000, n,
            ebisu_retry_seed(vendor_thing_id));
}

// Where the application limits login attempts, trying a wrong password on every
// refused request would lock the thing out.
TEST_CASE( "A refused password is not tried again until the session has waited" ) {
    // Two things, which wait differently.
    const char* vendor_thing_id = GENERATE(as<const char*>{}, "vid", "vid2");
    REQUIRE( password_wait_ms("vid", 1) != password_wait_ms("vid2", 1) );
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, vendor_thing_id, "pw");
    // No refresh token: a renewal logs in straight away.
    kii_author_t author = make_author("th.1", "at1", "");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // 5 minutes after the first refusal, twice as long after each further one,
    // up to an hour, and as much again at most.
    const uint64_t least[] = { 5, 10, 20, 40, 60, 60 };
    for (uint32_t n = 1; n <= 6; ++n) {
        uint64_t wait_ms = password_wait_ms(vendor_thing_id, n);
        INFO("vendor thing ID " << vendor_thing_id << ", refusal " << n << ", wait " << wait_ms);
        CHECK( wait_ms >= least[n - 1] * MINUTE_MS );
        CHECK( wait_ms <= 2 * least[n - 1] * MINUTE_MS );
        c.http.requests.clear();
        c.http.responses = { REFUSED, INVALID_GRANT };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], PASSWORD_GRANT) );

        // Meanwhile a refused request is only reported.
        platform.now_ms += wait_ms - 1;
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

    // So a refusal after that waits as long as the first again.
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    platform.now_ms += password_wait_ms(vendor_thing_id, 1) - 1;
    c.http.requests.clear();
    c.http.responses = { REFUSED };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 1 );
    platform.now_ms += 1;
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

    // Refused twice.
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    platform.now_ms += password_wait_ms("vid", 1);
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );

    // Tried at once, and refused, they wait as long as after a first refusal.
    kii_session_set_password(&session, "vid", "pw2");
    REQUIRE( platform.depth == 0 );
    c.http.requests.clear();
    c.http.responses = { REFUSED, INVALID_GRANT };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( contains(c.http.requests[1], "\"password\":\"pw2\"") );
    platform.now_ms += password_wait_ms("vid", 1) - 1;
    c.http.requests.clear();
    c.http.responses = { REFUSED };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 1 );

    platform.now_ms += 1;
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

    SECTION("too many requests") {
        c.http.responses = { REFUSED, http_response(429, "{}") };
    }

    SECTION("a request that timed out") {
        c.http.responses = { REFUSED, http_response(408, "{}") };
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

    // A 100 second token, renewed between 80% and 90% of its lifetime.
    const uint64_t after_ms = renew_after_ms(100);
    CHECK( after_ms >= 80000 );
    CHECK( after_ms <= 90000 );
    platform.now_ms = 1000;
    c.http.responses = {
        http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at1\",\"refresh_token\":\"rt1\",\"expires_in\":100}")
    };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    REQUIRE( platform.updates.size() == 1 );
    REQUIRE( std::string(held(&session).refresh_token) == "rt1" );

    SECTION("not yet") {
        platform.now_ms = 1000 + after_ms - 1;
        c.http.responses.push_back(DONE);
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at1\r\n") );
    }

    SECTION("due") {
        platform.now_ms = 1000 + after_ms;
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
        platform.now_ms = 1000 + after_ms + after_ms - 1;
        c.http.responses.push_back(DONE);
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 4 );
        REQUIRE( contains(c.http.requests[3], "Authorization: Bearer at2\r\n") );
    }
}

// How long the session waits after the n-th renewal ahead of expiry in a row
// that failed: thing th.1's share of 30 seconds doubled up to 5 minutes.
uint64_t renew_wait_ms(uint32_t n) {
    return ebisu_retry_wait(30 * 1000, 5 * 60 * 1000, n, ebisu_retry_seed("th.1"));
}

// While the server can't be reached, a request that renewed first would be
// two that fail.
TEST_CASE( "A renewal ahead of expiry that fails is tried again after a wait" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    // A 1000 second token, now due.
    platform.now_ms = 1000;
    c.http.responses = { refreshed("at1", "rt1", 1000) };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    platform.now_ms = 1000 + renew_after_ms(1000);

    // The renewal can't connect, and the request goes with the token it has.
    c.http.requests.clear();
    c.http.responses = { "", DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at1\r\n") );

    SECTION("longer after each failure") {
        // 15 to 30 seconds after the first, twice as long after each further
        // one, and 2.5 to 5 minutes at most.
        const uint64_t least_s[] = { 15, 30, 60, 120, 150, 150 };
        for (uint32_t n = 1; n <= 6; ++n) {
            uint64_t wait_ms = renew_wait_ms(n);
            INFO("failure " << n << ", wait " << wait_ms);
            CHECK( wait_ms >= least_s[n - 1] * 1000 );
            CHECK( wait_ms <= 2 * least_s[n - 1] * 1000 );

            // Meanwhile requests go without it.
            platform.now_ms += wait_ms - 1;
            c.http.requests.clear();
            c.http.responses = { DONE };
            REQUIRE( c.put(BODY) == KII_ERR_OK );
            REQUIRE( c.http.requests.size() == 1 );
            REQUIRE( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );

            platform.now_ms += 1;
            c.http.requests.clear();
            c.http.responses = { http_response(503, "{}"), DONE };
            REQUIRE( c.put(BODY) == KII_ERR_OK );
            REQUIRE( c.http.requests.size() == 2 );
            REQUIRE( contains(c.http.requests[0], "\"refresh_token\":\"rt1\"") );
        }

        // Renewed at last.
        platform.now_ms += renew_wait_ms(7);
        c.http.requests.clear();
        c.http.responses = { refreshed("at2", "rt2", 1000), DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );

        // So the next renewal that fails waits as long as after a first.
        platform.now_ms += renew_after_ms(1000);
        c.http.requests.clear();
        c.http.responses = { "", DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        platform.now_ms += renew_wait_ms(1) - 1;
        c.http.requests.clear();
        c.http.responses = { DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 1 );
        platform.now_ms += 1;
        c.http.requests.clear();
        c.http.responses = { refreshed("at3", "rt3", 1000), DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at3\r\n") );
    }

    SECTION("a token the server refuses meanwhile is renewed at once") {
        c.http.requests.clear();
        c.http.responses = { REFUSED, refreshed("at2", "rt2", 1000), DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( contains(c.http.requests[1], "\"refresh_token\":\"rt1\"") );
        REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    }

    SECTION("a renewal of a refused token that fails doesn't make the wait longer") {
        c.http.requests.clear();
        c.http.responses = { REFUSED, "" };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 2 );

        platform.now_ms += renew_wait_ms(1);
        c.http.requests.clear();
        c.http.responses = { refreshed("at2", "rt2", 1000), DONE };
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );
    }
    REQUIRE( platform.depth == 0 );
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

// A kii_t took the session's author as it was given the session, and again
// for each request's Authorization header: after the request's path was made.
// So its first request after another kii_t logged in went to a path of the
// thing it had before, or of none, with the new thing's token.
TEST_CASE( "A path has the thing another kii_t of the session logged in as" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client a;
    Client b;
    REQUIRE( kii_set_session(&a.kii, &session) == KII_ERR_OK );
    REQUIRE( kii_set_session(&b.kii, &session) == KII_ERR_OK );
    a.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at1\"}") };
    REQUIRE( kii_auth_thing(&a.kii, "vid", "pw") == KII_ERR_OK );
    b.http.responses = { DONE };

    SECTION("of Thing-IF") {
        kii_ti_put_firmware_version(&b.kii, "1.0");
        REQUIRE( b.http.requests.size() == 1 );
        CHECK( contains(b.http.requests[0], "PUT /thing-if/apps/app/things/th.1/firmware-version HTTP/1.1") );
        CHECK( contains(b.http.requests[0], "Authorization: Bearer at1\r\n") );
    }
    SECTION("of a subscription to a bucket") {
        kii_bucket_t bucket;
        bucket.scope = KII_SCOPE_APP;
        bucket.bucket_name = "b";
        bucket.scope_id = NULL;
        kii_subscribe_bucket(&b.kii, &bucket);
        REQUIRE( b.http.requests.size() == 1 );
        CHECK( contains(b.http.requests[0], "/buckets/b/filters/all/push/subscriptions/things/th.1 HTTP/1.1") );
    }
    SECTION("of a subscription to a topic") {
        kii_topic_t topic;
        topic.scope = KII_SCOPE_APP;
        topic.topic_name = "t";
        topic.scope_id = NULL;
        kii_subscribe_topic(&b.kii, &topic);
        REQUIRE( b.http.requests.size() == 1 );
        CHECK( contains(b.http.requests[0], "/topics/t/push/subscriptions/things/th.1 HTTP/1.1") );
    }
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

TEST_CASE( "kii_get_thing_id gives the session's thing, whichever kii_t obtained it" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client a;
    Client b;
    CHECK( std::string(kii_get_thing_id(&b.kii)) == "" );
    REQUIRE( kii_set_session(&a.kii, &session) == KII_ERR_OK );
    REQUIRE( kii_set_session(&b.kii, &session) == KII_ERR_OK );

    a.http.responses = { refreshed("at1", "rt1", 3600) };
    a.http.responses[0].replace(a.http.responses[0].find("th.1"), 4, "th.2");
    REQUIRE( kii_auth_thing(&a.kii, "vid", "pw") == KII_ERR_OK );

    // b has made no request since, and so still holds no author itself.
    REQUIRE( std::string(b.kii._author.author_id) == "" );
    CHECK( std::string(kii_get_thing_id(&b.kii)) == "th.2" );
    CHECK( std::string(kii_get_thing_id(&a.kii)) == "th.2" );
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
    REQUIRE( contains(c.http.requests[2], "PUT /thing-if/apps/app/targets/thing:th.1/states HTTP/1.1") );
    REQUIRE( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    REQUIRE( contains(c.http.requests[2], reader.state) );
}

namespace {

size_t append_to_string(char* buffer, size_t size, void* userdata) {
    ((std::string*)userdata)->append(buffer, size);
    return size;
}

}

// A download handed every response's body to the caller's callback: an
// error's went where the object's body was to go, and a refusal of the token,
// unseen by the session, was never followed by a renewal.
TEST_CASE( "A download gives its callback the object's body alone" ) {
    kii_bucket_t bucket = { KII_SCOPE_THING, "th.1", "b" };
    std::string sink;

    SECTION("an error's is read into kii's buffer") {
        Client c;
        const std::string error = "{\"errorCode\":\"OBJECT_BODY_NOT_FOUND\"}";
        c.http.responses = { http_response(404, error) };
        CHECK( kii_download_object_body(&c.kii, &bucket, "o", append_to_string, &sink) ==
                KII_ERR_RESP_STATUS );
        CHECK( kii_get_resp_status(&c.kii) == 404 );
        CHECK( sink == "" );
        CHECK( std::string(c.buff, kii_get_resp_body_length(&c.kii)) == error );
    }
    SECTION("an error's longer than the buffer is cut, and still an error") {
        Client c;
        c.http.responses = { http_response(500, std::string(3 * sizeof(c.buff), 'e')) };
        CHECK( kii_download_object_body(&c.kii, &bucket, "o", append_to_string, &sink) ==
                KII_ERR_RESP_STATUS );
        CHECK( kii_get_resp_status(&c.kii) == 500 );
        CHECK( sink == "" );
        CHECK( kii_get_resp_body_length(&c.kii) < sizeof(c.buff) );
    }
    SECTION("the object's goes to the callback, whatever its length") {
        Client c;
        const std::string body(3 * sizeof(c.buff), 'o');
        c.http.responses = { http_response(200, body) };
        CHECK( kii_download_object_body(&c.kii, &bucket, "o", append_to_string, &sink) == KII_ERR_OK );
        CHECK( sink == body );
        // And kii's own callback is back for the next call.
        c.http.responses.push_back(DONE);
        CHECK( c.put(BODY) == KII_ERR_OK );
        CHECK( sink == body );
    }
    SECTION("a refused token is renewed, and the download made again") {
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);
        Client c;
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
        c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), http_response(200, "the body") };
        CHECK( kii_download_object_body(&c.kii, &bucket, "o", append_to_string, &sink) == KII_ERR_OK );
        CHECK( sink == "the body" );
        REQUIRE( c.http.requests.size() == 3 );
        CHECK( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
        CHECK( contains(c.http.requests[2], "GET /api/apps/app/things/th.1/buckets/b/objects/o/body HTTP/1.1") );
        CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        CHECK( std::string(held(&session).access_token) == "at2" );
        CHECK( platform.depth == 0 );
    }
    SECTION("a token refused again leaves the 403, and nothing in the callback") {
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);
        Client c;
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
        c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), REFUSED };
        CHECK( kii_download_object_body(&c.kii, &bucket, "o", append_to_string, &sink) ==
                KII_ERR_RESP_STATUS );
        CHECK( kii_get_resp_status(&c.kii) == 403 );
        CHECK( sink == "" );
        CHECK( c.http.requests.size() == 3 );
    }
}

// The renewal needs about 640 bytes after a 300 byte body, which a buffer of
// 800 doesn't have: it renews in room of its own, and the body is sent again.
TEST_CASE( "A body leaving too little room to renew after it is sent again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";

    c.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), DONE };
    REQUIRE( c.put(body) == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    CHECK( c.resp_body() == "{\"done\":true}" );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( contains(c.http.requests[1], TOKEN_LINE) );
    CHECK( contains(c.http.requests[2], REQ_LINE) );
    CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    CHECK( contains(c.http.requests[2], body) );
    CHECK( std::string(held(&session).access_token) == "at2" );
    CHECK( platform.updates.size() == 1 );
    CHECK( platform.depth == 0 );
}

// Ahead of expiry, a renewal the body left too little room for wasn't made,
// and the token was used until the server refused it.
TEST_CASE( "A renewal ahead of expiry is made whatever room the body leaves" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c(800);
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";

    // A 1000 second token, now due.
    platform.now_ms = 1000;
    c.http.responses = { refreshed("at1", "rt1", 1000) };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    platform.now_ms = 1000 + renew_after_ms(1000);

    c.http.requests.clear();
    c.http.responses = { refreshed("at2", "rt2", 1000), DONE };
    REQUIRE( c.put(body) == KII_ERR_OK );
    CHECK( kii_get_resp_status(&c.kii) == 200 );
    REQUIRE( c.http.requests.size() == 2 );
    CHECK( contains(c.http.requests[0], TOKEN_LINE) );
    CHECK( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );
    CHECK( contains(c.http.requests[1], body) );
    CHECK( std::string(held(&session).access_token) == "at2" );
}

// Taking a token another kii_t of the session renewed asks nothing of the
// server, but the room a renewal needs was asked for all the same: without it,
// the token wasn't taken ahead of the request, and, once it was refused, the
// body was let go and the 403 left to the caller.
TEST_CASE( "A token another kii_t renewed is taken, whatever room the body leaves" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    // As in "A body leaving too little room to renew after it is sent again".
    Client c(800);
    Client other;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    REQUIRE( kii_set_session(&other.kii, &session) == KII_ERR_OK );
    const std::string body = "{\"k\":\"" + std::string(290, 'v') + "\"}";

    SECTION("renewed while the request was being built") {
        REQUIRE( c.start(body) == KII_ERR_OK );
        other.http.responses = { REFUSED, refreshed("at2", "rt2", 3600), DONE };
        REQUIRE( other.put(BODY) == KII_ERR_OK );

        c.http.responses = { DONE };
        REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 1 );
        CHECK( contains(c.http.requests[0], "Authorization: Bearer at2\r\n") );
        CHECK( contains(c.http.requests[0], body) );
    }

    SECTION("renewed while the request was refused") {
        kii_author_t renewed = make_author("th.1", "at2", "rt2");
        c.http.on_connect = [&](size_t i) {
            if (i == 0) {
                kii_session_set_author(&session, &renewed);
            }
        };
        c.http.responses = { REFUSED, DONE };
        REQUIRE( c.put(body) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 2 );
        CHECK( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
        CHECK( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );
        CHECK( contains(c.http.requests[1], body) );
    }

    // The one renewal, other's, if any: c asked the server for none.
    CHECK( platform.updates.size() <= 1 );
    CHECK( platform.depth == 0 );
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

        REQUIRE( kii_api_call_start(&c.kii, "GET", "/api/apps/app/x", NULL, false) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_header(&c.kii, "Authorization", "Bearer other") == KII_ERR_OK );
        c.http.responses = { REFUSED };
        REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 403 );
        REQUIRE( c.http.requests.size() == 1 );
        REQUIRE( platform.updates.empty() );
    }

    SECTION("server code with a token of the application's own, or none") {
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

        for (const char* token : { "other", (const char*)nullptr }) {
            INFO( "token " << (token != nullptr ? token : "none") );
            c.http.requests.clear();
            c.http.responses = { REFUSED };
            CHECK( kii_execute_server_code_with_token(&c.kii, NULL, "ep", "{}", token) == KII_ERR_RESP_STATUS );
            REQUIRE( c.http.requests.size() == 1 );
            CHECK_FALSE( contains(c.http.requests[0], "Bearer at1") );
            CHECK( platform.updates.empty() );
        }
    }

    SECTION("server code with the application's credentials") {
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        kii_author_t author = make_author("th.1", "at1", "rt1");
        kii_session_set_author(&session, &author);
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

        c.http.requests.clear();
        c.http.responses = { REFUSED };
        CHECK( kii_execute_server_code_with_app_auth(&c.kii, NULL, "ep", "{}") == KII_ERR_RESP_STATUS );
        REQUIRE( c.http.requests.size() == 1 );
        CHECK( contains(c.http.requests[0], "Authorization: Basic ") );
        CHECK_FALSE( contains(c.http.requests[0], "Bearer at1") );
        CHECK( platform.updates.empty() );
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
        CHECK( c.kii._session_refused == false );
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
        REQUIRE( c.kii._session_refused == true );
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
                "POST /api/apps/app/things/th.1/topics/my-topic/push/messages HTTP/1.1") );
    }
    SECTION("straight to the thing") {
        REQUIRE( kii_send_push_message_to_thing(&c.kii, "th.1", message) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( contains(c.http.requests[2],
                "POST /api/apps/app/things/th.1/push/messages HTTP/1.1") );
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
const char ONBOARDING_LINE[] = "POST /thing-if/apps/app/onboardings HTTP/1.1";

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
    REQUIRE( session._renew_at_known == false );
    REQUIRE( session._renew_mode == KII_RENEW_BY_ONBOARDING );

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
    REQUIRE( session._renew_at_known == false );

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

// The headers of a request, between its request line and the empty line.
std::string headers_of(const std::string& request) {
    size_t start = request.find("\r\n") + 2;
    return request.substr(start, request.find("\r\n\r\n") + 2 - start);
}

// A renewal during a request writes its headers after the request's, in the
// buffer kii_set_req_header_buff() gave, and the request goes again with the
// new token in place of the old, its other headers as they were.
TEST_CASE( "A renewal's headers go after the request's, in the header buffer given" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    kii_author_t author = make_author("th.1", "token-one-longer-than-the-next", "");
    kii_session_set_author(&session, &author);
    session._renew_mode = KII_RENEW_BY_ONBOARDING;
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    SECTION("with room for both") {
        char headers[1024];
        memset(headers, 0, sizeof(headers));
        kii_set_req_header_buff(&c.kii, headers, sizeof(headers));
        c.http.responses = { REFUSED, onboarded("at2"), DONE };
        REQUIRE( c.start(BODY) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_header(&c.kii, "X-After", "1") == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&c.kii) == 200 );
        REQUIRE( c.http.requests.size() == 3 );
        REQUIRE( onboarding_again(c.http.requests[1]) );

        // The same headers but the token, the one before and the ones after
        // where they were.
        std::string expected = headers_of(c.http.requests[0]);
        const std::string old_line = "Authorization: Bearer token-one-longer-than-the-next\r\n";
        REQUIRE( contains(expected, old_line) );
        expected.replace(expected.find(old_line), old_line.size(), "Authorization: Bearer at2\r\n");
        CHECK( headers_of(c.http.requests[2]) == expected );
        CHECK( contains(expected, "X-After: 1\r\n") );
        CHECK( contains(c.http.requests[2], BODY) );
        // Nothing left of either token, or of the renewal's headers.
        CHECK( std::string(headers, sizeof(headers)) == std::string(sizeof(headers), '\0') );
    }

    SECTION("without room for the renewal's, the renewal fails before it is sent") {
        // The request's headers, the token's line among them, and a little.
        char headers[200];
        kii_set_req_header_buff(&c.kii, headers, sizeof(headers));
        khct::cb::LoggerCtx log;
        kii_set_logger(&c.kii, log.get());
        c.http.responses = { REFUSED };
        REQUIRE( c.start(BODY) == KII_ERR_OK );
        REQUIRE( c.kii._req_headers_len + 32 < sizeof(headers) );
        // As any renewal that fails: the caller gets the 403.
        CHECK( kii_api_call_run(&c.kii) == KII_ERR_OK );
        CHECK( kii_get_resp_status(&c.kii) == 403 );
        CHECK( c.http.requests.size() == 1 );
        CHECK( std::string(headers, sizeof(headers)) == std::string(sizeof(headers), '\0') );
#ifndef EBISU_NO_LOG
        // Which says why.
        CHECK( log.mentions("header doesn't fit in the request header buffer") );
#endif
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

    platform.now_ms += password_wait_ms("vid", 1) - 1;
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

TEST_CASE( "A renewal ahead of expiry by onboarding again that fails waits too" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_session_set_password(&session, "vid", "pw");
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    platform.now_ms = 1000;
    c.http.responses = { refreshed("at1", "rt1", 1000) };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    kii_session_set_renew_mode(&session, KII_RENEW_BY_ONBOARDING);
    platform.now_ms = 1000 + renew_after_ms(1000);

    c.http.requests.clear();
    c.http.responses = { http_response(503, "{}"), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( onboarding_again(c.http.requests[0]) );

    platform.now_ms += renew_wait_ms(1) - 1;
    c.http.requests.clear();
    c.http.responses = { DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 1 );

    platform.now_ms += 1;
    c.http.requests.clear();
    c.http.responses = { onboarded("at2"), DONE };
    REQUIRE( c.put(BODY) == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    REQUIRE( onboarding_again(c.http.requests[0]) );
    REQUIRE( contains(c.http.requests[1], "Authorization: Bearer at2\r\n") );
}

// A token longer than its array can't be kept. The call says so, and the
// log why, rather than that the response couldn't be parsed.
TEST_CASE( "A token longer than kii has room for fails the call that got it as too large" ) {
    Client c;
    khct::cb::LoggerCtx log;
    kii_set_logger(&c.kii, log.get());
    // Whether a line with the text was logged, as expected: built without
    // logging, nothing is, whatever is expected.
    auto logged = [&](const std::string& text, bool expected = true) {
#ifdef EBISU_NO_LOG
        (void)text;
        return log.lines.empty() ? expected : !expected;
#else
        for (const auto& line : log.lines) {
            if (line.second.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
#endif
    };
    const std::string fits(KII_TOKEN_MAX_SIZE - 1, 'a');
    const std::string too_long(KII_TOKEN_MAX_SIZE, 'a');
    const std::string size = std::to_string(KII_TOKEN_MAX_SIZE - 1);

    SECTION("a login's access token") {
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"" + too_long + "\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_TOO_LARGE_DATA );
        CHECK( logged("the server's access token, of " + std::to_string(too_long.size()) +
                    " bytes, is longer than the " + size + " KII_TOKEN_MAX_SIZE gives it room for.") );
    }

    SECTION("a login's refresh token") {
        const std::string long_refresh(KII_REFRESH_TOKEN_MAX_SIZE, 'r');
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at1\","
                "\"refresh_token\":\"" + long_refresh + "\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_TOO_LARGE_DATA );
        CHECK( logged("the server's refresh token") );
        CHECK( logged("KII_REFRESH_TOKEN_MAX_SIZE") );
    }

    SECTION("onboarding's") {
        c.http.responses = { onboarded(too_long.c_str()) };
        CHECK( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_TOO_LARGE_DATA );
        CHECK( logged("the server's access token") );
    }

    SECTION("a registration's") {
        c.http.responses = { http_response(201, "{\"_thingID\":\"th.1\",\"_accessToken\":\"" + too_long + "\"}") };
        CHECK( kii_register_thing(&c.kii, "vid", "type", "pw") == KII_ERR_TOO_LARGE_DATA );
        CHECK( logged("the server's access token") );
    }

    SECTION("one that just fits is taken") {
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"" + fits + "\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
        CHECK( std::string(c.kii._author.access_token) == fits );
        CHECK_FALSE( logged("is longer than", false) );
    }

    SECTION("a token that isn't a string is still a response that can't be read") {
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":7}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_PARSE_JSON );
        CHECK_FALSE( logged("is longer than", false) );
    }
}

// The response was read straight into kii's author: what it had of an ID and
// tokens before the one it couldn't read, or couldn't keep, replaced kii's,
// and the refresh token was gone, with the call failed.
TEST_CASE( "A login, registration or onboarding that can't be read whole leaves the author as it was" ) {
    Client c;
    c.kii._author = make_author("th.0", "at0", "rt0");
    const std::string too_long(KII_TOKEN_MAX_SIZE, 'a');
    auto unchanged = [&c] {
        return std::string(c.kii._author.author_id) == "th.0" &&
            std::string(c.kii._author.access_token) == "at0" &&
            std::string(c.kii._author.refresh_token) == "rt0";
    };

    SECTION("a login without the ID") {
        c.http.responses = { http_response(200, "{\"access_token\":\"at1\",\"refresh_token\":\"rt1\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_PARSE_JSON );
        CHECK( unchanged() );
    }
    SECTION("a login with a token too long, after the ID") {
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"" + too_long + "\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_TOO_LARGE_DATA );
        CHECK( unchanged() );
    }
    SECTION("a login refused") {
        c.http.responses = { http_response(400, "{\"errorCode\":\"invalid_grant\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_RESP_STATUS );
        CHECK( unchanged() );
    }
    SECTION("a registration without the token") {
        c.http.responses = { http_response(201, "{\"_thingID\":\"th.1\"}") };
        CHECK( kii_register_thing(&c.kii, "vid", "type", "pw") == KII_ERR_PARSE_JSON );
        CHECK( unchanged() );
    }
    SECTION("a registration with a token too long") {
        c.http.responses = { http_response(201, "{\"_thingID\":\"th.1\",\"_accessToken\":\"" + too_long + "\"}") };
        CHECK( kii_register_thing(&c.kii, "vid", "type", "pw") == KII_ERR_TOO_LARGE_DATA );
        CHECK( unchanged() );
    }
    SECTION("onboarding without the thing's ID") {
        c.http.responses = { http_response(200, "{\"accessToken\":\"at1\"}") };
        CHECK( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_PARSE_JSON );
        CHECK( unchanged() );
    }
    SECTION("onboarding with a token too long") {
        c.http.responses = { onboarded(too_long.c_str()) };
        CHECK( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_TOO_LARGE_DATA );
        CHECK( unchanged() );
    }
    SECTION("and a login read whole replaces it all, the refresh token with none") {
        c.http.responses = { http_response(200, "{\"id\":\"th.1\",\"access_token\":\"at1\"}") };
        CHECK( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
        CHECK( std::string(c.kii._author.author_id) == "th.1" );
        CHECK( std::string(c.kii._author.access_token) == "at1" );
        CHECK( std::string(c.kii._author.refresh_token) == "" );
    }
    SECTION("as does a registration") {
        c.http.responses = { http_response(201, "{\"_thingID\":\"th.2\",\"_accessToken\":\"at2\"}") };
        CHECK( kii_register_thing(&c.kii, "vid", "type", "pw") == KII_ERR_OK );
        CHECK( std::string(c.kii._author.author_id) == "th.2" );
        CHECK( std::string(c.kii._author.access_token) == "at2" );
        CHECK( std::string(c.kii._author.refresh_token) == "" );
    }
    SECTION("and onboarding") {
        c.http.responses = { onboarded("at3") };
        CHECK( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );
        CHECK( std::string(c.kii._author.author_id) == "th.1" );
        CHECK( std::string(c.kii._author.access_token) == "at3" );
        CHECK( std::string(c.kii._author.refresh_token) == "" );
    }
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
        REQUIRE( session._renew_mode == KII_RENEW_BY_TOKEN_ENDPOINT );
        REQUIRE( c.put(BODY) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 5 );
        REQUIRE( contains(c.http.requests[3], "\"refresh_token\":\"rt2\"") );
        REQUIRE( contains(c.http.requests[4], "Authorization: Bearer at3\r\n") );
    }

    SECTION("a restored author is renewed by onboarding when told so") {
        kii_author_t saved = make_author("th.1", "at1", "");
        kii_session_set_author(&session, &saved);
        REQUIRE( session._renew_mode == KII_RENEW_BY_TOKEN_ENDPOINT );
        kii_session_set_renew_mode(&session, KII_RENEW_BY_ONBOARDING);
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
    REQUIRE( session._renew_at_known == false );
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

// khc keeps a path of up to 255 bytes, and a host of its own length, and
// refuses a longer one. Nine calls didn't look: with an application's and a
// thing's IDs long enough, the request went out with the path of none, and a
// state to a normalizer's host too long, to the host of none.
TEST_CASE( "A path or a host khc has no room for fails the call, sending nothing" ) {
    Client c;
    const std::string app_id(KII_APP_ID_MAX_SIZE - 1, 'a');
    kii_set_app_id(&c.kii, app_id.c_str());
    c.kii._author = make_author(std::string(KII_AUTHOR_ID_MAX_SIZE - 1, 't').c_str(), "at1", "");
    c.http.responses = { DONE };

    SECTION("a thing's state") {
        StateReader reader = { "{}", 0 };
        CHECK( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, NULL) ==
                KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("its firmware version, stored") {
        CHECK( kii_ti_put_firmware_version(&c.kii, "1.0") == KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("its firmware version, read") {
        kii_ti_firmware_version_t version;
        CHECK( kii_ti_get_firmware_version(&c.kii, &version) == KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("a normalizer's host") {
        kii_set_app_id(&c.kii, "app");
        c.kii._author = make_author("th.1", "at1", "");
        const std::string host(KHC_HOST_MAX_SIZE, 'h');
        StateReader reader = { "{}", 0 };
        CHECK( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, host.c_str()) ==
                KII_ERR_TOO_LARGE_DATA );
        // One that fits is where the state goes.
        CHECK( c.http.requests.empty() );
        const std::string fits(KHC_HOST_MAX_SIZE - 1, 'h');
        reader.pos = 0;
        CHECK( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, fits.c_str()) == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 1 );
        CHECK( contains(c.http.requests[0], "HOST: " + fits + "\r\n") );
        return;
    }
    CHECK( c.http.requests.empty() );
}

// What goes into a request's path or a header is sent as it is. A space in a
// bucket's or a topic's name, a CR and an LF in a header's value, made the
// request another one: one whose path ended early, or with a header of the
// caller's choosing, or two requests.
TEST_CASE( "What would break a request's line or a header fails the call, sending nothing" ) {
    Client c;
    kii_set_app_id(&c.kii, "app");
    c.kii._author = make_author("th.1", "at1", "");
    c.http.responses = { DONE };

    SECTION("a path of the application's") {
        CHECK( kii_api_call_start(&c.kii, "GET", "/api/apps/app/x HTTP/1.1\r\nX: y\r\n\r\nGET /z", NULL, false) ==
                KII_ERR_FAIL );
        CHECK( kii_api_call_start(&c.kii, "GET", "/api/apps/app/a b", NULL, false) == KII_ERR_FAIL );
        CHECK( kii_api_call_start(&c.kii, "GET X", "/api/apps/app/x", NULL, false) == KII_ERR_FAIL );
        // Too long is still that.
        const std::string long_path(KHC_PATH_MAX_SIZE, 'p');
        CHECK( kii_api_call_start(&c.kii, "GET", long_path.c_str(), NULL, false) == KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("a header of the application's") {
        REQUIRE( kii_api_call_start(&c.kii, "GET", "/api/apps/app/x", NULL, false) == KII_ERR_OK );
        CHECK( kii_api_call_append_header(&c.kii, "X-A", "1\r\nAuthorization: Bearer other") == KII_ERR_FAIL );
        CHECK( kii_api_call_append_header(&c.kii, "X-A\n", "1") == KII_ERR_FAIL );
        CHECK( kii_api_call_append_header(&c.kii, "X-A", "1\r") == KII_ERR_FAIL );
        CHECK( c.kii._req_headers == nullptr );
    }
    SECTION("a bucket's name") {
        kii_bucket_t bucket = { KII_SCOPE_APP, NULL, "b HTTP/1.1" };
        CHECK( kii_get_object(&c.kii, &bucket, "o.1") == KII_ERR_FAIL );
        CHECK( kii_subscribe_bucket(&c.kii, &bucket) == KII_ERR_FAIL );
    }
    SECTION("a topic's name") {
        kii_topic_t topic = { KII_SCOPE_APP, NULL, "t\r\nX: y" };
        CHECK( kii_subscribe_topic(&c.kii, &topic) == KII_ERR_FAIL );
    }
    SECTION("a normalizer's host") {
        StateReader reader = { "{}", 0 };
        CHECK( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, "h.example\r\nX: y") ==
                KII_ERR_FAIL );
    }
    CHECK( c.http.requests.empty() );
    // And the next call goes as it should.
    c.http.responses = { DONE };
    CHECK( kii_ti_put_firmware_version(&c.kii, "1.0") == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 1 );
    CHECK( contains(c.http.requests[0], "PUT /thing-if/apps/app/things/th.1/firmware-version HTTP/1.1\r\n") );
}

TEST_CASE( "Token lifetimes are read as 64-bit integers" ) {
    // A 32-bit long, as on Cortex-M, with -m32 and on 64-bit Windows, used to
    // turn the second into a negative lifetime, left unscheduled.
    const struct {
        const char* text;
        uint64_t renew_after_ms;
    } cases[] = {
        { "2147483647", renew_after_ms(UINT64_C(2147483647)) },
        { "2147483648", renew_after_ms(UINT64_C(2147483648)) },
        { "4294967295", renew_after_ms(UINT64_C(4294967295)) },
    };
    for (const auto& c : cases) {
        INFO("expires_in " << c.text);
        Login login(c.text, 1000);
        REQUIRE( login.res == KII_ERR_OK );
        CHECK( login.session._renew_at_known == true );
        CHECK( login.session._stored_at_ms == 1000 );
        CHECK( login.session._renew_after_ms == c.renew_after_ms );
    }
}

TEST_CASE( "Things whose tokens were issued together don't renew them together" ) {
    const char* ids[] = { "th.1", "th.2" };
    std::set<uint64_t> after;
    for (const char* id : ids) {
        INFO("thing " << id);
        Platform platform;
        kii_session_t session;
        init_session(&session, &platform);
        Client c;
        REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
        platform.now_ms = 1000;
        c.http.responses = { http_response(200, std::string("{\"id\":\"") + id +
                "\",\"access_token\":\"at1\",\"expires_in\":3600}") };
        REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
        CHECK( session._renew_at_known == true );
        CHECK( session._renew_after_ms == renew_after_ms(3600, id) );
        CHECK( session._renew_after_ms >= 3600000 - 300000 );
        CHECK( session._renew_after_ms <= 3600000 - 150000 );
        after.insert(session._renew_after_ms);
    }
    CHECK( after.size() == 2 );
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
        CHECK( login.session._renew_at_known == false );
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
    REQUIRE( session._renew_at_known == false );
}

TEST_CASE( "Renewal ahead of expiry does not depend on the clock's origin" ) {
    // The origin is arbitrary. This close to the top of the range, a deadline
    // of the time stored plus 80 to 90 seconds would wrap around and be due at
    // once.
    const uint64_t origin = UINT64_MAX - 1000;
    Login login("100", origin);
    REQUIRE( login.res == KII_ERR_OK );
    REQUIRE( login.session._renew_at_known == true );

    login.platform.now_ms = origin + 500;
    login.c.http.responses.push_back(DONE);
    REQUIRE( login.c.put(BODY) == KII_ERR_OK );
    REQUIRE( login.c.http.requests.size() == 2 );
    REQUIRE( contains(login.c.http.requests[1], "Authorization: Bearer at1\r\n") );
    REQUIRE( login.platform.updates.size() == 1 );

    // Even a clock that wraps around has it renewed when due.
    login.platform.now_ms = origin + renew_after_ms(100);
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
            return EBISU_SOCK_OK;
        };
        ctx.on_send = [this](void*, const char* buffer, size_t length, size_t* out_sent_length) {
            received.back().append(buffer, length);
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };
        ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            size_t i = received.size() - 1;
            if (i >= served.size() || pos == served[i].size()) {
                return EBISU_SOCK_FAIL;
            }
            *out_actual_length = served[i].copy(buffer, length_to_read, pos);
            pos += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    }

    Broker(const Broker&) = delete;
    Broker& operator=(const Broker&) = delete;

    void attach(kii_t* kii) {
        kii_set_mqtt_socket(kii, ctx.socket());
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
    "GET /api/apps/app/installations/ins1/mqtt-endpoint HTTP/1.1";

// Runs the MQTT task until it has subscribed on its third connection.
struct UntilThirdSubscription {
    Broker* broker;
    int steps;
};

bool cb_continue(void* task_info, void* userdata) {
    UntilThirdSubscription* until = (UntilThirdSubscription*)userdata;
    kii_mqtt_task_info_t* info = (kii_mqtt_task_info_t*)task_info;
    if (++until->steps > 100) {
        return false;
    }
    return (until->broker->received.size() == 3 &&
            info->task_state == KII_MQTT_ST_RECV_READY) ? false : true;
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
    // Every failed receive is a lost connection, not a timeout.
    broker.ctx.on_recv_timed_out = [](void*) { return false; };
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

// A lock that is one, for tasks that really run at once. The system's
// monotonic clock stays at 0.
struct SharedPlatform {
    std::mutex mutex;
    std::atomic<int> refreshes;
    std::vector<kii_author_t> updates;
    khct::cb::LockCtx lock;
    khct::cb::SystemCtx system;
    SharedPlatform() : refreshes(0) {
        lock.on_lock = [this] { mutex.lock(); };
        lock.on_unlock = [this] { mutex.unlock(); };
    }
};

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
        kii_session_set_lock(&session, platform.lock.get());
        kii_session_set_system(&session, platform.system.system());
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

    SECTION("a network failure is tried again") {
        c.http.ctx.on_connect = [](void* sock_ctx, const char* host, uint16_t port) {
            return EBISU_SOCK_FAIL;
        };
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        CHECK( state.info.error == KII_MQTT_ERR_OK );
        CHECK( std::string(held(&session).access_token) == "at1" );
    }

    SECTION("too many requests is tried again, without waiting for the password") {
        c.http.responses.push_back(http_response(429, "{}"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );

        c.http.responses.push_back(onboarded_with_endpoint("at2", "2"));
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( std::string(held(&session).access_token) == "at2" );
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

        platform.now_ms += password_wait_ms("vid", 1);
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
        kii_session_set_renew_mode(&session, KII_RENEW_BY_TOKEN_ENDPOINT);
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

// The endpoint of installation ins.n from Kii Cloud's API, its user and topic
// named after n.
std::string endpoint_of(const std::string& n) {
    return http_response(200,
            "{\"installationID\":\"ins." + n + "\",\"username\":\"user." + n +
            "\",\"password\":\"pass." + n + "\",\"mqttTopic\":\"topic." + n +
            "\",\"host\":\"mqtt.example.com\",\"portTCP\":1883,\"portSSL\":8883}");
}

const char INSTALL_LINE[] = "POST /api/apps/app/installations HTTP/1.1";

// A push task whose kii_t didn't onboard, with a token saved before a restart,
// gets its endpoint the way its session renews the token: by onboarding again,
// which installs push as well, when it has the password. It installed push
// first all the same, an installation onboarding then replaced, and without
// the password it couldn't onboard and ended.
TEST_CASE( "A push task with a restored author of an onboarding session" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "");
    kii_session_set_author(&session, &author);
    kii_session_set_renew_mode(&session, KII_RENEW_BY_ONBOARDING);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    mqtt_state_t state;

    SECTION("with the password, onboards for the endpoint, installing nothing") {
        kii_session_set_password(&session, "vid", "pw");
        _init_mqtt_state(&c.kii, &state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        c.http.responses = { onboarded_with_endpoint("at2", "2") };
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        REQUIRE( c.http.requests.size() == 1 );
        CHECK( onboarding_again(c.http.requests[0]) );
        CHECK( std::string(state.ins_id.id) == "ins.2" );
        CHECK( std::string(state.endpoint.username) == "user.2" );
        CHECK( std::string(held(&session).access_token) == "at2" );
    }

    SECTION("without the password, installs push and gets the endpoint from Kii Cloud") {
        _init_mqtt_state(&c.kii, &state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
        c.http.responses = {
            http_response(201, "{\"installationID\":\"ins.9\"}"),
            endpoint_of("9"),
        };
        _mqtt_state_install_push(&state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
        CHECK( state.info.error == KII_MQTT_ERR_OK );
        REQUIRE( c.http.requests.size() == 2 );
        CHECK( contains(c.http.requests[0], INSTALL_LINE) );
        CHECK( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
        CHECK( contains(c.http.requests[1], "/installations/ins.9/mqtt-endpoint") );
        CHECK( std::string(state.ins_id.id) == "ins.9" );
        CHECK( std::string(state.endpoint.username) == "user.9" );
        CHECK( std::string(held(&session).access_token) == "at1" );
    }

    SECTION("whose password is let go, installs push before asking for an endpoint") {
        kii_session_set_password(&session, "vid", "pw");
        _init_mqtt_state(&c.kii, &state);
        REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
        kii_session_set_password(&session, NULL, NULL);
        _mqtt_state_get_endpoint(&state);
        CHECK( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
        CHECK( state.info.error == KII_MQTT_ERR_OK );
        CHECK( c.http.requests.empty() );
    }
}

// Without the password, credentials the broker refused are fetched again for
// the installation onboarding made, as without a session: onboarding again
// can't be done, and the task ended.
TEST_CASE( "Refused MQTT credentials of an onboarding session without the password" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );
    c.http.responses = { onboarded_with_endpoint("at1", "1") };
    REQUIRE( kii_ti_onboard(&c.kii, "vid", "pw", NULL, NULL, NULL, NULL) == KII_ERR_OK );

    mqtt_state_t state;
    _init_mqtt_state(&c.kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    // Where a CONNACK refusing the credentials sends the task.
    state.info.task_state = KII_MQTT_ST_GET_ENDPOINT;
    c.http.responses.push_back(endpoint_of("1"));
    _mqtt_state_get_endpoint(&state);
    CHECK( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    CHECK( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( c.http.requests.size() == 2 );
    CHECK( contains(c.http.requests[1], "/installations/ins.1/mqtt-endpoint") );
    CHECK( contains(c.http.requests[1], "Authorization: Bearer at1\r\n") );
}

TEST_CASE( "The renewal mode is set by name, and an unknown one is ignored" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    REQUIRE( session._renew_mode == KII_RENEW_BY_TOKEN_ENDPOINT );

    kii_session_set_renew_mode(&session, KII_RENEW_BY_ONBOARDING);
    CHECK( session._renew_mode == KII_RENEW_BY_ONBOARDING );
    // A mode a later version adds means nothing to this one: left as it was.
    kii_session_set_renew_mode(&session, (kii_renew_mode_t)(KII_RENEW_BY_ONBOARDING + 1));
    CHECK( session._renew_mode == KII_RENEW_BY_ONBOARDING );
    kii_session_set_renew_mode(&session, (kii_renew_mode_t)-1);
    CHECK( session._renew_mode == KII_RENEW_BY_ONBOARDING );
    kii_session_set_renew_mode(&session, KII_RENEW_BY_TOKEN_ENDPOINT);
    CHECK( session._renew_mode == KII_RENEW_BY_TOKEN_ENDPOINT );

    // Each with the lock held.
    CHECK( platform.depth == 0 );
    CHECK( platform.max_depth == 1 );
}

// The renewal borrows the buffer: the refresh token went out from it, and the
// new tokens came back into it. A request whose streamed body can't be sent
// again leaves the 403 to the caller, and the buffer as the renewal left it.
TEST_CASE( "A renewal leaves no token in the buffer" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1-SECRET");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    StateReader reader = { "{\"alias\":{\"power\":true}}", 0 };
    c.http.responses = { REFUSED, refreshed("at2-SECRET", "rt2-SECRET", 3600) };
    REQUIRE( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, NULL) == KII_ERR_RESP_STATUS );
    REQUIRE( contains(c.http.requests[1], "\"refresh_token\":\"rt1-SECRET\"") );
    REQUIRE( std::string(held(&session).refresh_token) == "rt2-SECRET" );

    const std::string buff(c.buff, sizeof(c.buff));
    CHECK( buff.find("rt1-SECRET") == std::string::npos );
    CHECK( buff.find("rt2-SECRET") == std::string::npos );
    CHECK( buff.find("at2-SECRET") == std::string::npos );
}

// What kii_get_server_code_result() reads is the last response's: neither the
// refusal's nor the renewal's.
TEST_CASE( "Server code refused for its token is read from the response sent again" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    auto with = [](std::string resp, const std::string& headers) {
        const std::string type = "Content-Type: application/json\r\n";
        resp.replace(resp.find(type), type.size(), headers);
        return resp;
    };
    const std::string refused = with(REFUSED,
            "Content-Type: application/vnd.kii.WrongTokenException+json\r\n"
            "X-Step-Count: 1\r\nX-Environment-Version: 1\r\n");
    kii_server_code_result_t result;

    SECTION( "sent again" ) {
        c.http.responses = {
            refused,
            with(refreshed("at2", "rt2", 3600), "X-Step-Count: 2\r\n"),
            with(http_response(200, "{\"returnedValue\":\"ok\"}"),
                    "Content-Type: application/json\r\nX-Environment-Version: 16\r\n"),
        };
        REQUIRE( kii_execute_server_code(&c.kii, "ep", "{\"a\":1}") == KII_ERR_OK );
        REQUIRE( c.http.requests.size() == 3 );
        CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
        REQUIRE( kii_get_server_code_result(&c.kii, &result) == KII_ERR_OK );
        CHECK( result.status == 200 );
        CHECK( std::string(result.returned_value.value, result.returned_value.value_length) == "ok" );
        CHECK( result.step_count == -1 );
        CHECK( result.environment_version == 16 );
    }
    SECTION( "not sent again, the renewal refused" ) {
        c.http.responses = { refused, with(INVALID_GRANT, "X-Step-Count: 2\r\n") };
        REQUIRE( kii_execute_server_code(&c.kii, "ep", "{\"a\":1}") == KII_ERR_RESP_STATUS );
        REQUIRE( c.http.requests.size() == 2 );
        // The refusal, without its body, as the request reports it.
        REQUIRE( kii_get_server_code_result(&c.kii, &result) == KII_ERR_RESP_STATUS );
        CHECK( result.status == 403 );
        CHECK( result.body_length == 0 );
        CHECK( result.error_code.value == nullptr );
        CHECK( result.step_count == -1 );
        CHECK( result.environment_version == -1 );
    }
}

// What kii_get_etag() gives, and whether it kept it, is the last response's:
// neither the refusal's nor the renewal's, which had one too long to keep.
TEST_CASE( "An object read again after its token is renewed has that response's ETag" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    const std::string type = "Content-Type: application/json\r\n";
    auto with_etag = [&type](std::string resp, const std::string& etag) {
        resp.replace(resp.find(type), type.size(), type + "ETag: " + etag + "\r\n");
        return resp;
    };
    const std::string too_long(KII_ETAG_MAX_SIZE, 'x');
    c.http.responses = {
        with_etag(REFUSED, too_long),
        with_etag(refreshed("at2", "rt2", 3600), too_long),
        with_etag(http_response(200, "{\"a\":1}"), "\"5\""),
    };

    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.scope_id = "th.1";
    bucket.bucket_name = "b";
    REQUIRE( kii_get_object(&c.kii, &bucket, "o.1") == KII_ERR_OK );
    REQUIRE( c.http.requests.size() == 3 );
    CHECK( contains(c.http.requests[2], "Authorization: Bearer at2\r\n") );
    CHECK( std::string(kii_get_etag(&c.kii)) == "\"5\"" );
}

// A renewal's response isn't the application's, nor, once the request is sent
// again, the refusal: a request that gets no response after either leaves the
// ETag the last response gave, for the request to be tried again with. Each
// response here has an ETag of its own, which none of them may leave.
TEST_CASE( "A request that gets no response after a renewal leaves the ETag before it" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    const std::string type = "Content-Type: application/json\r\n";
    auto with_etag = [&type](std::string resp, const std::string& etag) {
        resp.replace(resp.find(type), type.size(), type + "ETag: " + etag + "\r\n");
        return resp;
    };
    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.scope_id = "th.1";
    bucket.bucket_name = "b";
    const char object[] = "{\"a\":2}";
    const char if_match[] = "\r\nIf-Match: \"5\"\r\n";

    // A 1000 second token, and an object read with it.
    platform.now_ms = 1000;
    c.http.responses = {
        refreshed("at1", "rt1", 1000),
        with_etag(http_response(200, "{\"a\":1}"), "\"5\""),
    };
    REQUIRE( kii_auth_thing(&c.kii, "vid", "pw") == KII_ERR_OK );
    REQUIRE( kii_get_object(&c.kii, &bucket, "o.1") == KII_ERR_OK );
    REQUIRE( std::string(kii_get_etag(&c.kii)) == "\"5\"" );
    c.http.requests.clear();

    SECTION("renewed ahead of its expiry") {
        platform.now_ms = 1000 + renew_after_ms(1000);
        // The request's connection fails.
        c.http.responses = { with_etag(refreshed("at2", "rt2", 1000), "\"r\""), "" };
        CHECK( kii_put_object(&c.kii, &bucket, "o.1", object, NULL, kii_get_etag(&c.kii)) ==
                KII_ERR_SOCK_CONNECT );
        REQUIRE( c.http.requests.size() == 2 );
        CHECK( contains(c.http.requests[0], TOKEN_LINE) );
        CHECK( std::string(kii_get_etag(&c.kii)) == "\"5\"" );
    }
    SECTION("renewed after a refusal") {
        // The request sent again fails to connect.
        c.http.responses = {
            with_etag(REFUSED, "\"x\""),
            with_etag(refreshed("at2", "rt2", 1000), "\"r\""),
            "",
        };
        CHECK( kii_put_object(&c.kii, &bucket, "o.1", object, NULL, kii_get_etag(&c.kii)) ==
                KII_ERR_SOCK_CONNECT );
        REQUIRE( c.http.requests.size() == 3 );
        CHECK( contains(c.http.requests[0], if_match) );
        CHECK( contains(c.http.requests[1], TOKEN_LINE) );
        CHECK( std::string(kii_get_etag(&c.kii)) == "\"5\"" );
    }
    // Tried again with it, the request goes with the same If-Match.
    c.http.responses.push_back(with_etag(DONE, "\"6\""));
    CHECK( kii_put_object(&c.kii, &bucket, "o.1", object, NULL, kii_get_etag(&c.kii)) ==
            KII_ERR_OK );
    REQUIRE( c.http.requests.size() == c.http.responses.size() );
    CHECK( contains(c.http.requests.back(), if_match) );
    CHECK( std::string(kii_get_etag(&c.kii)) == "\"6\"" );
}

// Not sent again, the request leaves the refusal its response, and its ETag
// the refusal's: not the renewal's.
TEST_CASE( "A refused request not sent again has the refusal's ETag" ) {
    Platform platform;
    kii_session_t session;
    init_session(&session, &platform);
    kii_author_t author = make_author("th.1", "at1", "rt1");
    kii_session_set_author(&session, &author);
    Client c;
    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_OK );

    const std::string type = "Content-Type: application/json\r\n";
    auto with_etag = [&type](std::string resp, const std::string& etag) {
        resp.replace(resp.find(type), type.size(), type + "ETag: " + etag + "\r\n");
        return resp;
    };
    StateReader reader = { "{\"alias\":{\"power\":true}}", 0 };
    c.http.responses = {
        with_etag(REFUSED, "\"x\""),
        with_etag(refreshed("at2", "rt2", 3600), "\"r\""),
    };
    REQUIRE( kii_ti_put_state(&c.kii, read_state, &reader, NULL, NULL, NULL) == KII_ERR_RESP_STATUS );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( c.http.requests.size() == 2 );
    CHECK( std::string(kii_get_etag(&c.kii)) == "\"x\"" );
}
