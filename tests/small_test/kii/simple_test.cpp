#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>

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
}
