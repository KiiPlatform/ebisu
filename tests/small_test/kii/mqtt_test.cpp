#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"

#include <sstream>

using namespace std;

TEST_CASE( "MQTT state test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);

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

    mqtt_state_t state;

    _init_mqtt_state(&kii, &state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_INSTALL_PUSH );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );

    int call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "POST https://api.kii.com/api/apps/dummyAppID/installations HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int call_recv = 0;
    stringstream ss;
    ss <<
        "HTTP/1.1 201 Created\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.InstallationCreationResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 06:41:17 GMT\r\n"
        "Location: https://api-jp.kii.com/api/apps/dummyAppID/installations/5t842kt0a4d5bvo3g61i97ft6\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 201\r\n"
        "Content-Length: 116\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID\","
        "  \"installationRegistrationID\" : \"56f6bcf7-3b1e-49c0-b625-64f810fe85a0\""
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_install_push(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_GET_ENDPOINT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( strcmp(state.ins_id.id, "dummyInstallationID") == 0 );
    REQUIRE( call_connect == 1 );
    REQUIRE( call_send > 1 );
    REQUIRE( call_recv >= 1 );
    REQUIRE( call_close == 1 );

    call_connect = 0;
    http_ctx.on_connect = [=, &call_connect](void* socket_context, const char* host, unsigned int port) {
        ++call_connect;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    call_send = 0;
    http_ctx.on_send = [=, &call_send](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        if (call_send == 0) {
            const char req_line[] = "GET https://api.kii.com/api/apps/dummyAppID/installations/dummyInstallationID/mqtt-endpoint HTTP/1.1\r\n";
            REQUIRE( length == strlen(req_line) );
            REQUIRE( strncmp(buffer, req_line, length) == 0 );
        }
        ++call_send;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    call_recv = 0;
    ss.clear();
    ss <<
        "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Expose-Headers: Content-Type, Authorization, Content-Length, X-Requested-With, ETag, X-Step-Count, X-Environment-version, X-HTTP-Status-Code\r\n"
        "Age: 0\r\n"
        "Cache-Control: max-age=0, no-cache, no-store\r\n"
        "Content-Type: application/vnd.kii.MQTTEndpointResponse+json;charset=UTF-8\r\n"
        "Date: Tue, 04 Dec 2018 08:02:07 GMT\r\n"
        "Server: openresty\r\n"
        "X-HTTP-Status-Code: 200\r\n"
        "X-MQTT-TTL: 2147483647\r\n"
        "Content-Length: 273\r\n"
        "Connection: Close\r\n"
        "\r\n"
        "{"
        "  \"installationID\" : \"dummyInstallationID_2\","
        "  \"username\" : \"dummyUser\","
        "  \"password\" : \"dummyPassword\","
        "  \"mqttTopic\" : \"dummyTopic\","
        "  \"host\" : \"jp-mqtt-dummy.kii.com\","
        "  \"portTCP\" : 1883,"
        "  \"portSSL\" : 8883,"
        "  \"portWS\" : 12470,"
        "  \"portWSS\" : 12473,"
        "  \"X-MQTT-TTL\" : 2147483647"
        "}";
    http_ctx.on_recv = [=, &call_recv, &ss](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++call_recv;
        *out_actual_length = ss.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    call_close = 0;
    http_ctx.on_close = [=, &call_close](void* socket_ctx) {
        ++call_close;
        return KHC_SOCK_OK;
    };

    _mqtt_state_get_endpoint(&state);
    REQUIRE( state.info.task_state == KII_MQTT_ST_SOCK_CONNECT );
    REQUIRE( state.info.error == KII_MQTT_ERR_OK );
    REQUIRE( strcmp(state.endpoint.username, "dummyUser") == 0 );
    REQUIRE( strcmp(state.endpoint.password, "dummyPassword") == 0 );
    REQUIRE( strcmp(state.endpoint.topic, "dummyTopic") == 0 );
    REQUIRE( strcmp(state.endpoint.host, "jp-mqtt-dummy.kii.com") == 0 );
    REQUIRE( state.endpoint.port_tcp == 1883 );
    REQUIRE( state.endpoint.port_ssl == 8883 );
    REQUIRE( state.endpoint.ttl == 2147483647 );
    REQUIRE( call_connect == 1 );
    REQUIRE( call_send > 1 );
    REQUIRE( call_recv >= 1 );
    REQUIRE( call_close == 1 );
}
