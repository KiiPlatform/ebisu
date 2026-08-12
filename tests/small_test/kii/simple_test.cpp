#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>
#include <string>
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
