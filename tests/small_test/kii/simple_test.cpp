#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>
#include <string>
#include <stdint.h>
#include <string.h>

TEST_CASE( "Simple test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_connect(&kii, khct::cb::mock_connect, &mqtt_ctx);
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    kii_set_cb_mqtt_sock_close(&kii, khct::cb::mock_close, &mqtt_ctx);

    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::PushCtx push_ctx;
    kii._cb_push_received = khct::cb::cb_push;
    kii._push_data = &push_ctx;

    SECTION("kii_enable_insecure_http test") {
        REQUIRE(kii._khc._enable_insecure == 0);
        kii_enable_insecure_http(&kii, KII_TRUE);
        REQUIRE(kii._khc._enable_insecure == 1);
        kii_enable_insecure_http(&kii, KII_FALSE);
        REQUIRE(kii._khc._enable_insecure == 0);
    }

    SECTION("kii_set_use_m_0 test") {
        REQUIRE(kii._use_m_0_header == KII_FALSE);
        kii_set_use_m_0_header_flag(&kii, KII_TRUE);
        REQUIRE(kii._use_m_0_header == KII_TRUE);
        kii_set_use_m_0_header_flag(&kii, KII_FALSE);
        REQUIRE(kii._use_m_0_header == KII_FALSE);
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
    const std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
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
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);

    // Two different APIs, each of which resets khc before its request.
    REQUIRE( kii_api_call_start(&kii, "POST", "/api/apps/dummyAppID/echo",
                "application/json", KII_TRUE) == KII_ERR_OK );
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
