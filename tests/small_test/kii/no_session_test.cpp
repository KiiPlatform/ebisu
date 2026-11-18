// kii built with EBISU_NO_SESSION, as -DEBISU_SESSION=OFF builds it: no
// session can be attached, and a token the server refuses fails the request
// as it does without one. The rest of kii's tests run against the same build.
#include "catch.hpp"
#include "kii.h"
#include "http_script.h"

#include <string>
#include <string.h>

using khct::contains;
using khct::http_response;

#ifndef EBISU_NO_SESSION
#error "Built for kii without sessions"
#endif

namespace {

struct Client {
    char buff[4096];
    kii_t kii;
    khct::HttpScript http;

    Client() {
        memset(buff, 0, sizeof(buff));
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "app");
        kii_set_buff(&kii, buff, sizeof(buff));
        kii_set_system(&kii, khct::cb::delay_only_system());
        http.attach(&kii);
    }
};

}

TEST_CASE( "Built without sessions, kii_set_session refuses any session" ) {
    // A session kii would otherwise attach: with its lock and its clock.
    khct::cb::LockCtx lock;
    khct::cb::SystemCtx system;
    kii_session_t session;
    kii_session_init(&session);
    kii_session_set_lock(&session, lock.get());
    kii_session_set_system(&session, system.system());
    Client c;

    REQUIRE( kii_set_session(&c.kii, &session) == KII_ERR_FAIL );
    REQUIRE( c.kii._session == nullptr );
    REQUIRE( kii_set_session(&c.kii, NULL) == KII_ERR_OK );
    REQUIRE( c.kii._session == nullptr );
}

TEST_CASE( "Built without sessions, a refused token reaches the caller" ) {
    Client c;
    strcpy(c.kii._author.author_id, "th.1");
    strcpy(c.kii._author.access_token, "at1");
    c.http.responses = { http_response(403, "{\"errorCode\":\"WRONG_TOKEN\"}") };

    const char body[] = "{\"k\":\"v\"}";
    REQUIRE( kii_api_call_start(&c.kii, "PUT", "/api/apps/app/things/th.1/x",
            "application/json", true) == KII_ERR_OK );
    REQUIRE( kii_api_call_append_body(&c.kii, body, strlen(body)) == KII_ERR_OK );
    REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );

    // Sent once, with the token kii holds, and its 403 given back whole.
    REQUIRE( c.http.requests.size() == 1 );
    REQUIRE( contains(c.http.requests[0], "Authorization: Bearer at1\r\n") );
    REQUIRE( kii_get_resp_status(&c.kii) == 403 );
    REQUIRE( std::string(c.buff, kii_get_resp_body_length(&c.kii)) ==
            "{\"errorCode\":\"WRONG_TOKEN\"}" );
    REQUIRE( std::string(c.kii._author.access_token) == "at1" );
}
