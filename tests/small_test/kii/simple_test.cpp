#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>
#include <string>
#include <stdint.h>
#include <string.h>

TEST_CASE( "Simple test" ) {
    const size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    const size_t mqtt_buff_size = 1024;
    char mqtt_buff[mqtt_buff_size];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());
    khct::cb::SockCtx mqtt_ctx;
    kii_set_mqtt_socket(&kii, mqtt_ctx.socket());

    kii_set_system(&kii, khct::cb::delay_only_system());

    khct::cb::PushCtx push_ctx;
    kii_set_cb_mqtt_message(&kii, khct::cb::cb_push, &push_ctx);

    SECTION("kii_enable_insecure_http test") {
        REQUIRE(kii._khc._enable_insecure == false);
        kii_enable_insecure_http(&kii, true);
        REQUIRE(kii._khc._enable_insecure == true);
        kii_enable_insecure_http(&kii, false);
        REQUIRE(kii._khc._enable_insecure == false);
    }

    SECTION("kii_set_use_m_0 test") {
        REQUIRE(kii._use_m_0_header == false);
        kii_set_use_m_0_header_flag(&kii, true);
        REQUIRE(kii._use_m_0_header == true);
        kii_set_use_m_0_header_flag(&kii, false);
        REQUIRE(kii._use_m_0_header == false);
    }
}

// kii_set_app_id and kii_set_site used to fill their fixed-size fields with
// strncpy(dest, src, sizeof(dest)). strncpy writes no terminator when the
// source is at least as long as the count, so input at or over the field size
// left the field unterminated and every later strlen over it ran off the end.
TEST_CASE( "Over-long app id and site are truncated and stay terminated" ) {
    kii_t kii;
    kii_init(&kii);

    const std::string long_app_id(KII_APP_ID_MAX_SIZE + 64, 'a');
    kii_set_app_id(&kii, long_app_id.c_str());
    REQUIRE( strlen(kii._app_id) == KII_APP_ID_MAX_SIZE - 1 );

    const std::string long_site(KII_APP_HOST_MAX_SIZE + 64, 'h');
    kii_set_site(&kii, long_site.c_str());
    REQUIRE( strlen(kii._app_host) == KII_APP_HOST_MAX_SIZE - 1 );

    // Exactly one byte too long: the boundary the old code got wrong.
    const std::string exact(KII_APP_ID_MAX_SIZE, 'b');
    kii_set_app_id(&kii, exact.c_str());
    REQUIRE( strlen(kii._app_id) == KII_APP_ID_MAX_SIZE - 1 );

    // Something that fits is copied whole.
    kii_set_app_id(&kii, "dummyAppID");
    REQUIRE( std::string(kii._app_id) == "dummyAppID" );
}

// It turned "JP", "US", "SG", "EU" and "CN3" into hosts of a list of its own,
// which a site of the application's own was never on. "JP", the public site,
// stays for 1.x's applications.
TEST_CASE( "kii_set_site takes the host as given" ) {
    kii_t kii;
    kii_init(&kii);
    for (const char* site : { "api-jp.kii.com", "kii.example.com", "US", "SG", "EU", "CN3" }) {
        INFO("site " << site);
        kii_set_site(&kii, site);
        CHECK( std::string(kii._app_host) == site );
    }

    kii_set_site(&kii, "JP");
    CHECK( std::string(kii._app_host) == "api-jp.kii.com" );
}

static bool points_into(const char* p, const char* buff, size_t buff_size) {
    uintptr_t addr = (uintptr_t)p;
    uintptr_t begin = (uintptr_t)buff;
    return begin <= addr && addr < begin + buff_size;
}

// kii_set_stream_buff and kii_set_resp_header_buff used to hand the buffers
// straight to khc, but every kii API begins by resetting khc, which clears
// them. No request ever used them: khc allocated its defaults every time.
TEST_CASE( "Stream and response header buffers are used by every request" ) {
    kii_t kii;
    kii_init(&kii);
    char kii_buff[1024];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_site(&kii, "api.kii.com");
    strcpy(kii._author.author_id, "thingID");
    strcpy(kii._author.access_token, "token");

    char stream_buff[512];
    char resp_header_buff[256];
    kii_set_stream_buff(&kii, stream_buff, sizeof(stream_buff));
    kii_set_resp_header_buff(&kii, resp_header_buff, sizeof(resp_header_buff));

    // Where khc asks the socket to read into: the response header buffer for
    // the status line and headers, the stream buffer for the body. Anywhere
    // else can only be a default khc allocated itself.
    int recv_into_header = 0;
    int recv_into_stream = 0;
    int recv_elsewhere = 0;
    size_t resp_pos = 0;
    // No Content-Length, so that the end is read into the stream buffer too.
    const std::string resp = "HTTP/1.1 200 OK\r\n\r\n{}";
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        if (points_into(buffer, resp_header_buff, sizeof(resp_header_buff))) {
            ++recv_into_header;
        } else if (points_into(buffer, stream_buff, sizeof(stream_buff))) {
            ++recv_into_stream;
        } else {
            ++recv_elsewhere;
        }
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    // Two different APIs, each of which resets khc before its request.
    REQUIRE( kii_api_call_start(&kii, "POST", "/api/apps/dummyAppID/echo",
                "application/json", true) == KII_ERR_OK );
    const char body[] = "{\"key\":\"value\"}";
    REQUIRE( kii_api_call_append_body(&kii, body, strlen(body)) == KII_ERR_OK );
    REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&kii) == 200 );
    REQUIRE( recv_into_header > 0 );
    REQUIRE( recv_into_stream > 0 );
    REQUIRE( recv_elsewhere == 0 );

    recv_into_header = 0;
    recv_into_stream = 0;
    kii_bucket_t bucket = { KII_SCOPE_THING, "thingID", "bucket" };
    REQUIRE( kii_get_object(&kii, &bucket, "objectID") == KII_ERR_OK );
    REQUIRE( recv_into_header > 0 );
    REQUIRE( recv_into_stream > 0 );
    REQUIRE( recv_elsewhere == 0 );

    // NULL hands the stream buffer back to khc's allocated default.
    kii_set_stream_buff(&kii, NULL, 0);
    recv_into_header = 0;
    recv_into_stream = 0;
    REQUIRE( kii_get_object(&kii, &bucket, "objectID") == KII_ERR_OK );
    REQUIRE( recv_into_header > 0 );
    REQUIRE( recv_into_stream == 0 );
    REQUIRE( recv_elsewhere > 0 );
}

// Every response header line goes through the ETag parser, up to the size of the
// response header buffer, so a long one must not cost its length in stack.
TEST_CASE( "The ETag is read from the response, whatever the other headers" ) {
    kii_t kii;
    kii_init(&kii);
    char kii_buff[1024];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_site(&kii, "api.kii.com");
    static char resp_header_buff[8192];
    kii_set_resp_header_buff(&kii, resp_header_buff, sizeof(resp_header_buff));

    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&resp, &resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    auto send = [&kii]() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, false);
        return (res == KII_ERR_OK) ? kii_api_call_run(&kii) : res;
    };

    const std::string cookie = "Set-Cookie: c=" + std::string(8000, 'x') + "\r\n";
    resp = "HTTP/1.1 200 OK\r\n" + cookie +
        "ETag: \"1\"\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "\"1\"" );

    // In any case, and with whitespace around the value.
    resp = "HTTP/1.1 200 OK\r\n"
        "etag:  \"2\" \r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "\"2\"" );

    resp = "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "" );
}

// kii_get_etag() gave the first 63 bytes of a longer ETag, and nothing for one
// in a header too long for the response header buffer. Given to
// kii_put_object(), the first fails as if the object had changed, and the
// second sends no If-Match: the update goes through unconditionally.
TEST_CASE( "An ETag kii can't keep whole fails the object call that got it" ) {
    kii_t kii;
    kii_init(&kii);
    char kii_buff[4096];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_site(&kii, "api.kii.com");
    // kii's own, of 256 bytes: none is set.

    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&resp, &resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.bucket_name = "b";
    bucket.scope_id = "th.1";
    auto respond = [&resp](int status, const std::string& headers, const std::string& body) {
        resp = "HTTP/1.1 " + std::to_string(status) + " Status\r\n" + headers +
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    };
    const std::string kept(KII_ETAG_MAX_SIZE - 1, 'k');
    const std::string too_long(KII_ETAG_MAX_SIZE, 'x');
    // Its header, 6 bytes and the value, longer than the buffer.
    const std::string skipped(300, 's');
    const std::string object = "{\"a\":1}";
    const std::string created = "{\"objectID\":\"o.1\",\"createdAt\":1}";
    kii_object_id_t id;

    SECTION("kept whole, to the last byte that fits") {
        respond(200, "ETag: " + kept + "\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == kept );
    }
    SECTION("one byte longer, or in a header skipped") {
        for (const std::string& etag : { too_long, skipped, std::string(1000, 'y') }) {
            CAPTURE( etag.size() );
            const std::string header = "ETag: " + etag + "\r\n";
            respond(200, header, object);
            CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_TOO_LARGE_DATA );
            CHECK( std::string(kii_get_etag(&kii)) == "" );
            // The object, read all the same.
            CHECK( std::string(kii_buff, kii_get_resp_body_length(&kii)) == object );

            respond(200, header, "");
            CHECK( kii_put_object(&kii, &bucket, "o.1", object.c_str(), NULL, kept.c_str()) ==
                    KII_ERR_TOO_LARGE_DATA );
            CHECK( std::string(kii_get_etag(&kii)) == "" );
            CHECK( kii_patch_object(&kii, &bucket, "o.1", object.c_str(), kept.c_str()) ==
                    KII_ERR_TOO_LARGE_DATA );

            respond(201, header, created);
            memset(&id, 0, sizeof(id));
            CHECK( kii_post_object(&kii, &bucket, object.c_str(), NULL, &id) ==
                    KII_ERR_TOO_LARGE_DATA );
            CHECK( std::string(id.id) == "o.1" );

            // The next response's is read as ever.
            respond(200, "ETag: \"2\"\r\n", object);
            CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_OK );
            CHECK( std::string(kii_get_etag(&kii)) == "\"2\"" );
        }
    }
    SECTION("whatever comes after one too long") {
        respond(200, "ETag: " + too_long + "\r\nETag: \"3\"\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_TOO_LARGE_DATA );
        respond(200, "ETag: \"3\"\r\nETag: " + too_long + "\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_TOO_LARGE_DATA );
        CHECK( std::string(kii_get_etag(&kii)) == "" );
    }
    SECTION("another header skipped, with the ETag read") {
        respond(200, "Set-Cookie: " + skipped + "\r\nETag: \"4\"\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == "\"4\"" );
    }
    SECTION("another header skipped, and no ETag") {
        // It may have been one.
        respond(200, "Set-Cookie: " + skipped + "\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_TOO_LARGE_DATA );
        // Not without a header skipped.
        respond(200, "Set-Cookie: c=1\r\n", object);
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == "" );
    }
    SECTION("an error is reported as one") {
        respond(404, "ETag: " + too_long + "\r\n", "{\"errorCode\":\"OBJECT_NOT_FOUND\"}");
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_RESP_STATUS );
        respond(409, "ETag: " + too_long + "\r\n", "{\"errorCode\":\"OBJECT_VERSION_IS_STALE\"}");
        CHECK( kii_put_object(&kii, &bucket, "o.1", object.c_str(), NULL, kept.c_str()) ==
                KII_ERR_RESP_STATUS );
    }
    SECTION("calls that give no ETag to use go on") {
        respond(204, "ETag: " + too_long + "\r\n", "");
        CHECK( kii_delete_object(&kii, &bucket, "o.1") == KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == "" );
        respond(200, "ETag: " + too_long + "\r\n", "{}");
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, false) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == "" );
    }
}
